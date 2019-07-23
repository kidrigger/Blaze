
#pragma once

#include <glm/glm.hpp>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <array>

namespace blaze
{
	struct Vertex
	{
		glm::vec3 position;
		glm::vec3 color;

		static VkVertexInputBindingDescription getBindingDescription()
		{
			VkVertexInputBindingDescription bindingDesc = {};
			bindingDesc.binding = 0;
			bindingDesc.stride = sizeof(Vertex);
			bindingDesc.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

			return bindingDesc;
		}

		static std::array<VkVertexInputAttributeDescription, 2> getAttributeDescriptions()
		{
			std::array<VkVertexInputAttributeDescription, 2> attributeDesc = {};

			attributeDesc[0].binding = 0;
			attributeDesc[0].location = 0;
			attributeDesc[0].format = VK_FORMAT_R32G32B32_SFLOAT;
			attributeDesc[0].offset = offsetof(Vertex, position);

			attributeDesc[1].binding = 0;
			attributeDesc[1].location = 1;
			attributeDesc[1].format = VK_FORMAT_R32G32B32_SFLOAT;
			attributeDesc[1].offset = offsetof(Vertex, color);

			return attributeDesc;
		}
	};

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

		template <typename T>
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
			catch(std::exception& e)
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
}
