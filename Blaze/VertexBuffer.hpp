
#pragma once

#include "Renderer.hpp"
#include "util/Managed.hpp"
#include "DataTypes.hpp"
#include "util/loaders.hpp"

#include <stdexcept>
#include <vector>

namespace blaze
{
	// Refactor into Buffer class

	template <typename T>
	class VertexBuffer
	{
	private:
		util::Managed<BufferObject> vertexBuffer;
		size_t size{ 0 };

	public:
		VertexBuffer() noexcept
		{
		}

		VertexBuffer(const Renderer& renderer, const std::vector<T>& data) noexcept
			:size(sizeof(data[0])* data.size())
		{
			using namespace util;
			VmaAllocator allocator = renderer.get_context().get_allocator();

			auto [stagingBuffer, stagingAlloc] = renderer.get_context().createBuffer(size, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY);

			auto stagingBufferRAII = Managed<BufferObject>({ stagingBuffer, stagingAlloc }, [allocator](BufferObject& bo) { vmaDestroyBuffer(allocator, bo.buffer, bo.allocation); });

			void* bufferdata;
			vmaMapMemory(allocator, stagingAlloc, &bufferdata);
			memcpy(bufferdata, data.data(), size);
			vmaUnmapMemory(allocator, stagingAlloc);

			auto [finalBuffer, finalAllocation] = renderer.get_context().createBuffer(size, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VMA_MEMORY_USAGE_GPU_ONLY);
			vertexBuffer = Managed<BufferObject>({ finalBuffer, finalAllocation }, [allocator](BufferObject& bo) { vmaDestroyBuffer(allocator, bo.buffer, bo.allocation); });

			try
			{
				VkCommandBuffer commandBuffer = renderer.startTransferCommands();

				VkBufferCopy copyRegion = {};
				copyRegion.srcOffset = 0;
				copyRegion.dstOffset = 0;
				copyRegion.size = size;
				vkCmdCopyBuffer(commandBuffer, stagingBuffer, finalBuffer, 1, &copyRegion);

				renderer.endTransferCommands(commandBuffer);
			}
			catch (std::exception& e)
			{
				std::cerr << e.what() << std::endl;
			}
		}

		VertexBuffer(VertexBuffer&& other) noexcept
			: buffer(other.buffer),
			allocation(other.allocation),
			size(other.size),
			allocator(other.allocator)
		{
		}

		VertexBuffer& operator=(VertexBuffer&& other) noexcept
		{
			if (this == &other)
			{
				return *this;
			}
			buffer = other.buffer;
			allocation = other.allocation;
			size = other.size;
			allocator = other.allocator;
			return *this;
		}

		VertexBuffer(const VertexBuffer& other) = delete;
		VertexBuffer& operator=(const VertexBuffer& other) = delete;

		~VertexBuffer()
		{
			vmaDestroyBuffer(allocator)
		}

		VkBuffer get_buffer() const { return vertexBuffer.get().buffer; }
		VmaAllocation get_memory() const { return vertexBuffer.get().allocation->GetMemory(); }
		size_t get_size() const { return size; }
	};

	template <typename T>
	class IndexedVertexBuffer
	{

	private:
		util::Managed<BufferObject> vertexBuffer;
		size_t vertexSize{ 0 };
		util::Managed<BufferObject> indexBuffer;
		size_t indexSize{ 0 };
		uint32_t indexCount{ 0 };

	public:
		IndexedVertexBuffer() noexcept
		{
		}

		IndexedVertexBuffer(const Renderer& renderer, const std::vector<T>& vertex_data, const std::vector<uint32_t>& index_data) noexcept
			:vertexSize(sizeof(vertex_data[0])* vertex_data.size()),
			indexSize(sizeof(uint32_t)* index_data.size()),
			indexCount(static_cast<uint32_t>(index_data.size()))
		{
			using namespace util;
			auto allocator = renderer.get_context().get_allocator();

			// Vertex Buffer
			auto [stagingVertexBuffer, stagingVertexAllocation] = renderer.get_context().createBuffer(vertexSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY);
			
			auto stagingVertexBufferRAII = Managed<BufferObject>({ stagingVertexBuffer, stagingVertexAllocation }, [allocator](BufferObject& bo) { vmaDestroyBuffer(allocator, bo.buffer, bo.allocation); });

			void* bufferdata;
			vmaMapMemory(allocator, stagingVertexAllocation, &bufferdata);
			memcpy(bufferdata, vertex_data.data(), vertexSize);
			vmaUnmapMemory(allocator, stagingVertexAllocation);

			auto [finalVertexBuffer, finalVertexAllocation] = renderer.get_context().createBuffer(vertexSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VMA_MEMORY_USAGE_GPU_ONLY);

			vertexBuffer = Managed<BufferObject>({ finalVertexBuffer, finalVertexAllocation }, [allocator](BufferObject& bo) { vmaDestroyBuffer(allocator, bo.buffer, bo.allocation); });

			// Index buffer

			auto [stagingIndexBuffer, stagingIndexMemory] = renderer.get_context().createBuffer(indexSize, VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY);
			
			auto stagingIndexBufferRAII = Managed<BufferObject>({ stagingIndexBuffer, stagingIndexMemory }, [allocator](BufferObject& bo) { vmaDestroyBuffer(allocator, bo.buffer, bo.allocation); });

			vmaMapMemory(allocator, stagingIndexMemory, &bufferdata);
			memcpy(bufferdata, index_data.data(), indexSize);
			vmaUnmapMemory(allocator, stagingIndexMemory);

			auto [finalIndexBuffer, finalIndexMemory] = renderer.get_context().createBuffer(indexSize, VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VMA_MEMORY_USAGE_GPU_ONLY);

			indexBuffer = Managed<BufferObject>({ finalIndexBuffer, finalIndexMemory }, [allocator](BufferObject& bo) { vmaDestroyBuffer(allocator, bo.buffer, bo.allocation); });

			try
			{
				VkCommandBuffer commandBuffer = renderer.startTransferCommands();

				VkBufferCopy copyRegion = {};
				copyRegion.srcOffset = 0;
				copyRegion.dstOffset = 0;

				copyRegion.size = vertexSize;
				vkCmdCopyBuffer(commandBuffer, stagingVertexBuffer, finalVertexBuffer, 1, &copyRegion);

				copyRegion.size = indexSize;
				vkCmdCopyBuffer(commandBuffer, stagingIndexBuffer, finalIndexBuffer, 1, &copyRegion);

				renderer.endTransferCommands(commandBuffer);
			}
			catch (std::exception& e)
			{
				std::cerr << e.what() << std::endl;
			}
		}

		IndexedVertexBuffer(IndexedVertexBuffer&& other) noexcept
			: vertexBuffer(std::move(other.vertexBuffer)),
			vertexSize(other.vertexSize),
			indexBuffer(std::move(other.indexBuffer)),
			indexSize(other.indexSize),
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
			vertexSize = other.vertexSize;
			indexBuffer = std::move(other.indexBuffer);
			indexSize = other.indexSize;
			indexCount = other.indexCount;
			return *this;
		}

		IndexedVertexBuffer(const IndexedVertexBuffer& other) = delete;
		IndexedVertexBuffer& operator=(const IndexedVertexBuffer& other) = delete;

		VkBuffer get_vertexBuffer() const { return vertexBuffer.get().buffer; }
		VmaAllocation get_vertexAllocation() const { return vertexBuffer.get().allocation; }
		size_t get_verticeSize() const { return vertexSize; }
		VkBuffer get_indexBuffer() const { return indexBuffer.get().buffer; }
		VmaAllocation get_indexAllocation() const { return indexMemory.get().allocation; }
		size_t get_indiceSize() const { return indexSize; }
		uint32_t get_indexCount() const { return indexCount; }
	};
}
