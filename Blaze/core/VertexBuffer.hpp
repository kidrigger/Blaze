
#pragma once

#include <Datatypes.hpp>
#include <core/Context.hpp>
#include <util/Managed.hpp>

#include <cstring>
#include <stdexcept>
#include <type_traits>
#include <vector>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/string_cast.hpp>

namespace blaze
{
// Refactor into Buffer class

class BaseVBO
{
protected:
	VkBuffer buffer{VK_NULL_HANDLE};
	VmaAllocation allocation{VK_NULL_HANDLE};
	VmaAllocator allocator{VK_NULL_HANDLE};
	uint32_t count{0};
	size_t size{0};

public:
	enum Usage
	{
		VertexBuffer = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
		IndexBuffer = VK_BUFFER_USAGE_INDEX_BUFFER_BIT
	};

	BaseVBO() noexcept
	{
	}
	BaseVBO(const Context* context, Usage usage, const void* data, uint32_t count, size_t size) noexcept;

	BaseVBO(BaseVBO&& other) noexcept;
	BaseVBO& operator=(BaseVBO&& other) noexcept;
	BaseVBO(const BaseVBO& other) = delete;
	BaseVBO& operator=(const BaseVBO& other) = delete;

	inline const VkBuffer& get_buffer() const
	{
		return buffer;
	}

	inline const uint32_t& get_count() const
	{
		return count;
	}

	virtual ~BaseVBO();
};

/**
 * @class VertexBuffer
 *
 * @tparam T The type of vertex data held by the buffer.
 *
 * @brief Object encapsulating the data in a vertex buffer.
 */
template <typename T>
class VertexBuffer : public BaseVBO
{
public:
	/**
	 * @fn VertexBuffer()
	 *
	 * @brief Default Constructor.
	 */
	VertexBuffer() noexcept : BaseVBO()
	{
	}

	/**
	 * @fn VertexBuffer(const Context* context, const std::vector<T>& data)
	 *
	 * @brief Main constructor.
	 *
	 * @param context The Vulkan Context in use.
	 * @param data A vector of vertices (\a T) to be held in the buffer.
	 */
	VertexBuffer(const Context* context, const std::vector<T>& data) noexcept
		: BaseVBO(context, Usage::VertexBuffer, data.data(), static_cast<uint32_t>(data.size()), data.size() * sizeof(T))
	{
	}

	inline void bind(VkCommandBuffer buf) const
	{
		const static VkDeviceSize offset = 0;
		vkCmdBindVertexBuffers(buf, 0, 1, &buffer, &offset);
	}
};

/**
 * @class IndexBuffer
 *
 * @tparam T The type of vertex data held by the buffer.
 *
 * @brief Object encapsulating the data in a vertex buffer.
 */
template <typename T>
class IndexBuffer : public BaseVBO
{
	static_assert(std::is_same<T, uint16_t>() || std::is_same<T, uint32_t>());
	static constexpr VkIndexType indexType = std::is_same<T, uint16_t>() ? VK_INDEX_TYPE_UINT16 : VK_INDEX_TYPE_UINT32;

public:
	/**
	 * @fn IndexBuffer()
	 *
	 * @brief Default Constructor.
	 */
	IndexBuffer() noexcept : BaseVBO()
	{
	}

	/**
	 * @fn IndexBuffer(const Context* context, const std::vector<T>& data)
	 *
	 * @brief Main constructor.
	 *
	 * @param context The Vulkan Context in use.
	 * @param data A vector of vertices (\a T) to be held in the buffer.
	 */
	IndexBuffer(const Context* context, const std::vector<T>& data) noexcept
		: BaseVBO(context, Usage::IndexBuffer, data.data(), static_cast<uint32_t>(data.size()), data.size() * sizeof(T))
	{
	}

	/**
	 * @fn bind(VkCommandBuffer buf)
	 *
	 * @brief Binds the buffers to the command buffer.
	 *
	 * @param buf The Command Buffer in use.
	 */
	inline void bind(VkCommandBuffer buf)
	{
		vkCmdBindIndexBuffer(buf, buffer, 0, indexType);
	}
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
	VertexBuffer<T> vertexBuffer;
	IndexBuffer<uint32_t> indexBuffer;

public:
	/**
	 * @fn IndexedVertexBuffer()
	 *
	 * @brief Default Constructor/
	 */
	IndexedVertexBuffer() noexcept
	{
	}

	/**
	 * @brief Main constructor for the object.
	 *
	 * @param context The Vulkan Context in use.
	 * @param vertex_data A vector of vertices (\a T) to be in the buffer.
	 * @param index_data A vector of indices (uint32_t) to be index the vertices.
	 */
	IndexedVertexBuffer(const Context* context, const std::vector<uint32_t>& index_data,
						const std::vector<T>& vertex_data) noexcept
		: vertexBuffer(context, vertex_data), indexBuffer(context, index_data)
	{
	}

	/**
	 * @name Move Constructors.
	 *
	 * @brief Move only, copy deleted.
	 *
	 * @{
	 */
	IndexedVertexBuffer(IndexedVertexBuffer&& other) noexcept
		: vertexBuffer(std::move(other.vertexBuffer)), indexBuffer(std::move(other.indexBuffer))
	{
	}

	IndexedVertexBuffer& operator=(IndexedVertexBuffer&& other) noexcept
	{
		if (this == &other)
		{
			return *this;
		}
		vertexBuffer = std::move(other.vertexBuffer);
		indexBuffer = std::move(other.indexBuffer);
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
	inline void bind(VkCommandBuffer buf)
	{
		vertexBuffer.bind(buf);
		indexBuffer.bind(buf);
	}

	/**
	 * @name Getters
	 *
	 * @brief Getters for private members.
	 *
	 * @{
	 */
	inline const VkBuffer& get_vertexBuffer() const
	{
		return vertexBuffer.get_buffer();
	}
	inline const uint32_t& get_vertexCount() const
	{
		return vertexBuffer.get_count();
	}
	inline const VkBuffer& get_indexBuffer() const
	{
		return indexBuffer.get_buffer();
	}
	inline const uint32_t& get_indexCount() const
	{
		return indexBuffer.get_count();
	}
	/**
	 * @}
	 */
};
} // namespace blaze
