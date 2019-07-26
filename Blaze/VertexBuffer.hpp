
#include "Renderer.hpp"
#include "util/Managed.hpp"

#include <stdexcept>
#include <vector>

namespace blaze
{
	// Refactor into Buffer class

	template <typename T>
	class VertexBuffer
	{
	private:
		util::Managed<VkDeviceMemory> memory;
		util::Managed<VkBuffer> buffer;
		size_t size;

	public:
		VertexBuffer() noexcept
			: size(0)
		{
		}

		VertexBuffer(const Renderer& renderer, const std::vector<T>& data) noexcept
			:size(sizeof(data[0])* data.size())
		{
			using namespace util;

			auto buffer_deleter = [dev = renderer.get_device()](VkBuffer& buf) { vkDestroyBuffer(dev, buf, nullptr); };
			auto memory_deleter = [dev = renderer.get_device()](VkDeviceMemory& devmem) { vkFreeMemory(dev, devmem, nullptr); };

			auto [stagingBuffer, stagingMemory] = createBuffer(renderer.get_device(), renderer.get_physicalDevice(), size, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

			auto stagingBufferManager = Managed(stagingBuffer, buffer_deleter);
			auto stagingMemoryManager = Managed(stagingMemory, memory_deleter);

			void* bufferdata;
			vkMapMemory(renderer.get_device(), stagingMemory, 0, size, 0, &bufferdata);
			memcpy(bufferdata, data.data(), size);
			vkUnmapMemory(renderer.get_device(), stagingMemory);

			auto [finalBuffer, finalMemory] = createBuffer(renderer.get_device(), renderer.get_physicalDevice(), size, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

			buffer = Managed(finalBuffer, buffer_deleter);
			memory = Managed(finalMemory, memory_deleter);

			try
			{
				VkCommandBuffer commandBuffer;
				VkCommandBufferAllocateInfo allocInfo = {};
				allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
				allocInfo.commandPool = renderer.get_transferCommandPool();
				allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
				allocInfo.commandBufferCount = 1;
				auto result = vkAllocateCommandBuffers(renderer.get_device(), &allocInfo, &commandBuffer);
				if (result != VK_SUCCESS)
				{
					throw std::runtime_error("Command buffer alloc failed with " + std::to_string(result));
				}

				VkCommandBufferBeginInfo commandBufferBeginInfo = {};
				commandBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
				commandBufferBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

				result = vkBeginCommandBuffer(commandBuffer, &commandBufferBeginInfo);
				if (result != VK_SUCCESS)
				{
					throw std::runtime_error("Begin Command Buffer failed with " + std::to_string(result));
				}

				VkBufferCopy copyRegion = {};
				copyRegion.srcOffset = 0;
				copyRegion.dstOffset = 0;
				copyRegion.size = size;
				vkCmdCopyBuffer(commandBuffer, stagingBuffer, finalBuffer, 1, &copyRegion);

				result = vkEndCommandBuffer(commandBuffer);
				if (result != VK_SUCCESS)
				{
					throw std::runtime_error("End Command Buffer failed with " + std::to_string(result));
				}

				VkSubmitInfo submitInfo = {};
				submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
				submitInfo.commandBufferCount = 1;
				submitInfo.pCommandBuffers = &commandBuffer;

				VkFence fence = createFence(renderer.get_device());
				vkResetFences(renderer.get_device(), 1, &fence);

				result = vkQueueSubmit(renderer.get_transferQueue(), 1, &submitInfo, fence);
				if (result != VK_SUCCESS)
				{
					throw std::runtime_error("Submit Command Buffer failed with " + std::to_string(result));
				}

				vkWaitForFences(renderer.get_device(), 1, &fence, VK_TRUE, std::numeric_limits<uint64_t>::max());
				vkDestroyFence(renderer.get_device(), fence, nullptr);
			}
			catch (std::exception& e)
			{
				std::cerr << e.what() << std::endl;
			}
		}

		VertexBuffer(VertexBuffer&& other) noexcept
			: buffer(std::move(other.buffer)),
			memory(std::move(other.memory)),
			size(other.size)
		{
		}

		VertexBuffer& operator=(VertexBuffer&& other) noexcept
		{
			if (this == &other)
			{
				return *this;
			}
			buffer = std::move(other.buffer);
			memory = std::move(other.memory);
			size = other.size;
			return *this;
		}

		VertexBuffer(const VertexBuffer& other) = delete;
		VertexBuffer& operator=(const VertexBuffer& other) = delete;

		VkBuffer get_buffer() const { return buffer.get(); }
		VkDeviceMemory get_memory() const { return memory.get(); }
		size_t get_size() const { return size; }
	};

	template <typename T>
	class IndexedVertexBuffer
	{

	private:
		util::Managed<VkDeviceMemory> vertexMemory;
		util::Managed<VkBuffer> vertexBuffer;
		size_t verticeSize;
		util::Managed<VkDeviceMemory> indexMemory;
		util::Managed<VkBuffer> indexBuffer;
		size_t indiceSize;
		uint32_t indexCount;

	public:
		IndexedVertexBuffer() noexcept
			: verticeSize(0),
			indiceSize(0),
			indexCount(0)
		{
		}

		IndexedVertexBuffer(const Renderer& renderer, const std::vector<T>& vertex_data, const std::vector<uint32_t>& index_data) noexcept
			:verticeSize(sizeof(vertex_data[0])* vertex_data.size()),
			indiceSize(sizeof(uint32_t)* index_data.size()),
			indexCount(static_cast<uint32_t>(index_data.size()))
		{
			using namespace util;

			auto buffer_deleter = [dev = renderer.get_device()](VkBuffer& buf) { vkDestroyBuffer(dev, buf, nullptr); };
			auto memory_deleter = [dev = renderer.get_device()](VkDeviceMemory& devmem) { vkFreeMemory(dev, devmem, nullptr); };

			// Vertex Buffer

			auto [stagingVertexBuffer, stagingVertexMemory] = createBuffer(renderer.get_device(), renderer.get_physicalDevice(), verticeSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

			auto stagingVertexBufferRAII = Managed(stagingVertexBuffer, buffer_deleter);
			auto stagingVertexMemoryRAII = Managed(stagingVertexMemory, memory_deleter);

			void* bufferdata;
			vkMapMemory(renderer.get_device(), stagingVertexMemory, 0, verticeSize, 0, &bufferdata);
			memcpy(bufferdata, vertex_data.data(), verticeSize);
			vkUnmapMemory(renderer.get_device(), stagingVertexMemory);

			auto [finalVertexBuffer, finalVertexMemory] = createBuffer(renderer.get_device(), renderer.get_physicalDevice(), verticeSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

			vertexBuffer = Managed(finalVertexBuffer, buffer_deleter);
			vertexMemory = Managed(finalVertexMemory, memory_deleter);

			// Index buffer

			auto [stagingIndexBuffer, stagingIndexMemory] = createBuffer(renderer.get_device(), renderer.get_physicalDevice(), indiceSize, VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

			auto stagingIndexBufferRAII = Managed(stagingIndexBuffer, buffer_deleter);
			auto stagingIndexMemoryRAII = Managed(stagingIndexMemory, memory_deleter);

			vkMapMemory(renderer.get_device(), stagingIndexMemory, 0, indiceSize, 0, &bufferdata);
			memcpy(bufferdata, index_data.data(), indiceSize);
			vkUnmapMemory(renderer.get_device(), stagingIndexMemory);

			auto [finalIndexBuffer, finalIndexMemory] = createBuffer(renderer.get_device(), renderer.get_physicalDevice(), indiceSize, VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

			indexBuffer = Managed(finalIndexBuffer, buffer_deleter);
			indexMemory = Managed(finalIndexMemory, memory_deleter);

			try
			{
				VkCommandBuffer commandBuffer;
				VkCommandBufferAllocateInfo allocInfo = {};
				allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
				allocInfo.commandPool = renderer.get_transferCommandPool();
				allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
				allocInfo.commandBufferCount = 1;
				auto result = vkAllocateCommandBuffers(renderer.get_device(), &allocInfo, &commandBuffer);
				if (result != VK_SUCCESS)
				{
					throw std::runtime_error("Command buffer alloc failed with " + std::to_string(result));
				}

				VkCommandBufferBeginInfo commandBufferBeginInfo = {};
				commandBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
				commandBufferBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

				result = vkBeginCommandBuffer(commandBuffer, &commandBufferBeginInfo);
				if (result != VK_SUCCESS)
				{
					throw std::runtime_error("Begin Command Buffer failed with " + std::to_string(result));
				}

				VkBufferCopy copyRegion = {};
				copyRegion.srcOffset = 0;
				copyRegion.dstOffset = 0;
				copyRegion.size = verticeSize;
				vkCmdCopyBuffer(commandBuffer, stagingVertexBuffer, finalVertexBuffer, 1, &copyRegion);

				copyRegion.size = indiceSize;
				vkCmdCopyBuffer(commandBuffer, stagingIndexBuffer, finalIndexBuffer, 1, &copyRegion);

				result = vkEndCommandBuffer(commandBuffer);
				if (result != VK_SUCCESS)
				{
					throw std::runtime_error("End Command Buffer failed with " + std::to_string(result));
				}

				VkSubmitInfo submitInfo = {};
				submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
				submitInfo.commandBufferCount = 1;
				submitInfo.pCommandBuffers = &commandBuffer;

				VkFence fence = createFence(renderer.get_device());
				vkResetFences(renderer.get_device(), 1, &fence);

				result = vkQueueSubmit(renderer.get_transferQueue(), 1, &submitInfo, fence);
				if (result != VK_SUCCESS)
				{
					throw std::runtime_error("Submit Command Buffer failed with " + std::to_string(result));
				}

				vkWaitForFences(renderer.get_device(), 1, &fence, VK_TRUE, std::numeric_limits<uint64_t>::max());
				vkDestroyFence(renderer.get_device(), fence, nullptr);
			}
			catch (std::exception& e)
			{
				std::cerr << e.what() << std::endl;
			}
		}

		IndexedVertexBuffer(IndexedVertexBuffer&& other) noexcept
			: vertexBuffer(std::move(other.vertexBuffer)),
			vertexMemory(std::move(other.vertexMemory)),
			verticeSize(other.verticeSize),
			indexBuffer(std::move(other.indexBuffer)),
			indexMemory(std::move(other.indexMemory)),
			indiceSize(other.indiceSize),
			indexCount(other.indexCount)
		{
		}

		IndexedVertexBuffer& operator=(IndexedVertexBuffer&& other) noexcept
		{
			if (this == &other)
			{
				return *this;
			}
			vertexBuffer = std::move(other.vertexBuffer);
			vertexMemory = std::move(other.vertexMemory);
			verticeSize = other.verticeSize;
			indexBuffer = std::move(other.indexBuffer);
			indexMemory = std::move(other.indexMemory);
			indiceSize = other.indiceSize;
			indexCount = other.indexCount;
			return *this;
		}

		IndexedVertexBuffer(const IndexedVertexBuffer& other) = delete;
		IndexedVertexBuffer& operator=(const IndexedVertexBuffer& other) = delete;

		VkBuffer get_vertexBuffer() const { return vertexBuffer.get(); }
		VkDeviceMemory get_vertexMemory() const { return vertexMemory.get(); }
		size_t get_verticeSize() const { return verticeSize; }
		VkBuffer get_indexBuffer() const { return indexBuffer.get(); }
		VkDeviceMemory get_indexMemory() const { return indexMemory.get(); }
		size_t get_indiceSize() const { return indiceSize; }
		uint32_t get_indexCount() const { return indexCount; }
	};
}
