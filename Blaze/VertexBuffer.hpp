
#pragma once

#include "Context.hpp"
#include "util/Managed.hpp"
#include "Datatypes.hpp"

#include <stdexcept>
#include <vector>
#include <cstring>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/string_cast.hpp>

namespace blaze
{
	// Refactor into Buffer class

    /**
     * @class VertexBuffer
     *
     * @tparam T The type of vertex data held by the buffer.
     *
     * @brief Object encapsulating the data in a vertex buffer.
     */
	template <typename T>
	class VertexBuffer
	{
	private:
		util::Managed<BufferObject> vertexBuffer;
		uint32_t count{ 0 };
		size_t size{ 0 };

	public:
        /**
         * @fn VertexBuffer()
         *
         * @brief Default Constructor.
         */
		VertexBuffer() noexcept {}

        /**
         * @fn VertexBuffer(const Context& context, const std::vector<T>& data)
         *
         * @brief Main constructor.
         *
         * @param context The Vulkan Context in use.
         * @param data A vector of vertices (\a T) to be held in the buffer.
         */
		VertexBuffer(const Context& context, const std::vector<T>& data) noexcept
			:size(sizeof(data[0])* data.size()),
			count(static_cast<uint32_t>(data.size()))
		{
			using namespace util;
			VmaAllocator allocator = context.get_allocator();

			auto stagingBuf = context.createBuffer(size, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY);

			auto stagingBufferRAII = Managed<BufferObject>(stagingBuf, [allocator](BufferObject& bo) { vmaDestroyBuffer(allocator, bo.buffer, bo.allocation); });

			void* bufferdata;
			vmaMapMemory(allocator, stagingBuf.allocation, &bufferdata);
			memcpy(bufferdata, data.data(), size);
			vmaUnmapMemory(allocator, stagingBuf.allocation);

			auto finalBuf = context.createBuffer(size, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VMA_MEMORY_USAGE_GPU_ONLY);
			vertexBuffer = Managed<BufferObject>(finalBuf, [allocator](BufferObject& bo) { vmaDestroyBuffer(allocator, bo.buffer, bo.allocation); });

			try
			{
				VkCommandBuffer commandBuffer = context.startCommandBufferRecord();

				VkBufferCopy copyRegion = {};
				copyRegion.srcOffset = 0;
				copyRegion.dstOffset = 0;
				copyRegion.size = size;
				vkCmdCopyBuffer(commandBuffer, stagingBuf.buffer, finalBuf.buffer, 1, &copyRegion);

				context.flushCommandBuffer(commandBuffer);
			}
			catch (std::exception& e)
			{
				std::cerr << e.what() << std::endl;
			}
		}

        /**
         * @name Move Constructors.
         *
         * @brief Move only, copy deleted.
         *
         * @{
         */
		VertexBuffer(VertexBuffer&& other) noexcept
			: vertexBuffer(std::move(other.vertexBuffer)),
			count(other.count),
			size(other.size)
		{
		}

		VertexBuffer& operator=(VertexBuffer&& other) noexcept
		{
			if (this == &other)
			{
				return *this;
			}
			vertexBuffer = std::move(other.vertexBuffer);
			count = other.count;
			size = other.size;
			return *this;
		}

		VertexBuffer(const VertexBuffer& other) = delete;
		VertexBuffer& operator=(const VertexBuffer& other) = delete;
        /**
         * @}
         */

        /**
         * @fn bind(VkCommandBuffer buf)
         *
         * @brief Binds the buffers to the command buffer.
         *
         * @param buf The Command Buffer in use.
         */
		void bind(VkCommandBuffer buf)
		{
			VkBuffer vbufs[] = { vertexBuffer.get().buffer };
			VkDeviceSize offsets[] = { 0 };
			vkCmdBindVertexBuffers(buf, 0, 1, vbufs, offsets);
		}

        /**
         * @name Getters
         *
         * @brief Getters for private members.
         *
         * @{
         */
		VkBuffer get_buffer() const { return vertexBuffer.get().buffer; }
		VmaAllocation get_allocation() const { return vertexBuffer.get().allocation; }
		const size_t& get_size() const { return size; }
		const uint32_t& get_count() const { return count; }
        /**
         * @}
         */
    };

    /**
     * @class IndexedVertexBuffer
     *
     * @tparam T The type of vertex data held by the buffer.
     *
     * @brief Object encapsulating the data in a vertex buffer and the related indices.
     */
	template <typename T>
	class IndexedVertexBuffer
	{

	private:
		util::Managed<BufferObject> vertexBuffer;
		size_t vertexSize{ 0 };
		uint32_t vertexCount{ 0 };
		util::Managed<BufferObject> indexBuffer;
		size_t indexSize{ 0 };
		uint32_t indexCount{ 0 };

	public:
        /**
         * @fn IndexedVertexBuffer()
         *
         * @brief Default Constructor/
         */
		IndexedVertexBuffer() noexcept {}

        /**
         * @fn IndexedVertexBuffer(const Context& context, const std::vector<T>& vertex_data, const std::vector<uint32_t>& index_data)
         *
         * @brief Main constructor for the object.
         *
         * @param context The Vulkan Context in use.
         * @param vertex_data A vector of vertices (\a T) to be in the buffer.
         * @param index_data A vector of indices (uint32_t) to be index the vertices.
         */
		IndexedVertexBuffer(const Context& context, const std::vector<T>& vertex_data, const std::vector<uint32_t>& index_data) noexcept
			:vertexSize(sizeof(vertex_data[0])* vertex_data.size()),
			vertexCount(static_cast<uint32_t>(vertex_data.size())),
			indexSize(sizeof(uint32_t)* index_data.size()),
			indexCount(static_cast<uint32_t>(index_data.size()))
		{
			using namespace util;
			auto allocator = context.get_allocator();

			// Vertex Buffer
			auto stagingVBuf = context.createBuffer(vertexSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY);
			
			auto stagingVertexBufferRAII = Managed<BufferObject>(stagingVBuf, [allocator](BufferObject& bo) { vmaDestroyBuffer(allocator, bo.buffer, bo.allocation); });

			void* bufferdata;
			vmaMapMemory(allocator, stagingVBuf.allocation, &bufferdata);
			memcpy(bufferdata, vertex_data.data(), vertexSize);
			vmaUnmapMemory(allocator, stagingVBuf.allocation);

			auto finalVBuf = context.createBuffer(vertexSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VMA_MEMORY_USAGE_GPU_ONLY);

			vertexBuffer = Managed<BufferObject>(finalVBuf, [allocator](BufferObject& bo) { vmaDestroyBuffer(allocator, bo.buffer, bo.allocation); });

			// Index buffer

			auto stagingIBuf = context.createBuffer(indexSize, VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY);
			
			auto stagingIndexBufferRAII = Managed<BufferObject>(stagingIBuf, [allocator](BufferObject& bo) { vmaDestroyBuffer(allocator, bo.buffer, bo.allocation); });

			vmaMapMemory(allocator, stagingIBuf.allocation, &bufferdata);
			memcpy(bufferdata, index_data.data(), indexSize);
			vmaUnmapMemory(allocator, stagingIBuf.allocation);

			auto finalIBuf = context.createBuffer(indexSize, VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VMA_MEMORY_USAGE_GPU_ONLY);

			indexBuffer = Managed<BufferObject>(finalIBuf, [allocator](BufferObject& bo) { vmaDestroyBuffer(allocator, bo.buffer, bo.allocation); });

			try
			{
				VkCommandBuffer commandBuffer = context.startCommandBufferRecord();

				VkBufferCopy copyRegion = {};
				copyRegion.srcOffset = 0;
				copyRegion.dstOffset = 0;

				copyRegion.size = vertexSize;
				vkCmdCopyBuffer(commandBuffer, stagingVBuf.buffer, finalVBuf.buffer, 1, &copyRegion);

				copyRegion.size = indexSize;
				vkCmdCopyBuffer(commandBuffer, stagingIBuf.buffer, finalIBuf.buffer, 1, &copyRegion);

				context.flushCommandBuffer(commandBuffer);
			}
			catch (std::exception& e)
			{
				std::cerr << e.what() << std::endl;
			}
		}

        /**
         * @name Move Constructors.
         *
         * @brief Move only, copy deleted.
         *
         * @{
         */
		IndexedVertexBuffer(IndexedVertexBuffer&& other) noexcept
			: vertexBuffer(std::move(other.vertexBuffer)),
			vertexSize(other.vertexSize),
			vertexCount(other.vertexCount),
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
			vertexCount = other.vertexCount;
			indexBuffer = std::move(other.indexBuffer);
			indexSize = other.indexSize;
			indexCount = other.indexCount;
			return *this;
		}

		IndexedVertexBuffer(const IndexedVertexBuffer& other) = delete;
		IndexedVertexBuffer& operator=(const IndexedVertexBuffer& other) = delete;
        /**
         * @}
         */

        /**
         * @fn bind(VkCommandBuffer buf)
         *
         * @brief Binds the buffers to the command buffer.
         *
         * @param buf The Command Buffer in use.
         */
		void bind(VkCommandBuffer buf)
		{
			VkBuffer vbufs[] = { vertexBuffer.get().buffer };
			VkDeviceSize offsets[] = { 0 };
			vkCmdBindVertexBuffers(buf, 0, 1, vbufs, offsets);
			vkCmdBindIndexBuffer(buf, indexBuffer.get().buffer, 0, VK_INDEX_TYPE_UINT32);
		}

        /**
         * @name Getters
         *
         * @brief Getters for private members.
         *
         * @{
         */
        const VkBuffer& get_vertexBuffer() const { return vertexBuffer.get().buffer; }
		const VmaAllocation& get_vertexAllocation() const { return vertexBuffer.get().allocation; }
		const size_t& get_verticeSize() const { return vertexSize; }
		const uint32_t& get_vertexCount() const { return vertexCount; }
		const VkBuffer& get_indexBuffer() const { return indexBuffer.get().buffer; }
		const VmaAllocation& get_indexAllocation() const { return indexBuffer.get().allocation; }
		const size_t& get_indiceSize() const { return indexSize; }
		const uint32_t& get_indexCount() const { return indexCount; }
        /**
         * @}
         */
    };
}
