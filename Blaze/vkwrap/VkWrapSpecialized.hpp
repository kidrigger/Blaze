
#pragma once 

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include "VkWrapBase.hpp"

namespace blaze::vkw
{
/**
 * @struct CommandBufferVector
 *
 * @brief Specialized wrapper for VkCommandBuffer
 *
 * CommandBuffers are constructed and deleted as an array of buffers instead of one at a time,
 * hence instead of iterating and destroying, a single destroy call is used to destroy all of the
 * CommandBuffers owned.
 */
struct CommandBufferVector
{
	std::vector<VkCommandBuffer> handles;
	VkCommandPool pool{VK_NULL_HANDLE};
	VkDevice device{VK_NULL_HANDLE};

	CommandBufferVector() noexcept
	{
	}

	explicit CommandBufferVector(std::vector<VkCommandBuffer>&& col, VkCommandPool pool, VkDevice device) noexcept
		: handles(std::move(col)), pool(pool), device(device)
	{
	}

	CommandBufferVector(const CommandBufferVector& other) = delete;
	CommandBufferVector& operator=(const CommandBufferVector& other) = delete;
	CommandBufferVector(CommandBufferVector&& other) noexcept
		: CommandBufferVector(std::move(other.handles), base::take(other.pool), base::take(other.device))
	{
	}

	CommandBufferVector& operator=(CommandBufferVector&& other) noexcept
	{
		if (this == &other)
		{
			return *this;
		}
		std::swap(handles, other.handles);
		std::swap(pool, other.pool);
		std::swap(device, other.device);
		return *this;
	}

	const std::vector<VkCommandBuffer>& get() const
	{
		return handles;
	}

	const VkCommandBuffer& operator[](uint32_t idx) const
	{
		return handles[idx];
	}

	inline bool valid() const
	{
		return !handles.empty();
	}

	inline uint32_t size() const
	{
		return static_cast<uint32_t>(handles.size());
	}

	~CommandBufferVector()
	{
		if (valid())
		{
			vkFreeCommandBuffers(device, pool, size(), handles.data());
			handles.clear();
			base::take(pool);
			base::take(device);
		}
	}
};

/**
 * @struct MemAllocator
 *
 * @brief VmaAllocator wrapper.
 *
 * VmaAllocator is an independent, but the function prototype for destroy is void(VmaAllocator)
 * which disallows the usage of the available wrapper class.
 */
struct MemAllocator
{
	VmaAllocator handle{VK_NULL_HANDLE};

	MemAllocator() noexcept
	{
	}

	explicit MemAllocator(VmaAllocator handle) noexcept : handle(handle)
	{
	}

	MemAllocator(const MemAllocator& other) = delete;
	MemAllocator& operator=(const MemAllocator& other) = delete;
	MemAllocator(MemAllocator&& other) noexcept : MemAllocator(base::take(other.handle))
	{
	}

	MemAllocator& operator=(MemAllocator&& other) noexcept
	{
		if (this == &other)
		{
			return *this;
		}
		std::swap(handle, other.handle);
		return *this;
	}

	const VmaAllocator& get() const
	{
		return handle;
	}

	inline bool valid() const
	{
		return handle != VK_NULL_HANDLE;
	}

	~MemAllocator()
	{
		if (valid())
		{
			vmaDestroyAllocator(base::take(handle));
		}
	}
};
}
