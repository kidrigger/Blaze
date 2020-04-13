
#pragma once

#include <core/Context.hpp>
#include <cstring>

#include <vma/vk_mem_alloc.h>

namespace blaze
{
/**
 * @class UBO
 *
 * @tparam T The data object to be stored in the UBO.
 *
 * @brief The class to handle operations on a uniform buffer.
 */
template <typename T>
class UBO
{
private:
	VkBuffer buffer{VK_NULL_HANDLE};
	VmaAllocation allocation{VK_NULL_HANDLE};
	VmaAllocator allocator{VK_NULL_HANDLE};
	size_t size{0};

public:
	/**
	 * @fn UBO()
	 *
	 * @brief Default Constructor.
	 */
	UBO() noexcept
	{
	}

	/**
	 * @fn UBO(const Context& context, const T& data)
	 *
	 * @brief Main constructor of the class.
	 *
	 * @param context The current Vulkan Context.
	 * @param data The data object of type T to store.
	 */
	UBO(const Context& context, const T& data) : size(sizeof(data))
	{
		allocator = context.get_allocator();

		std::tie(buffer, allocation) =
			context.createBuffer(size, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_ONLY);

		void* memdata;
		vmaMapMemory(allocator, allocation, &memdata);
		memcpy(memdata, &data, size);
		vmaUnmapMemory(allocator, allocation);
	}

	/**
	 * @name Move Constructors.
	 *
	 * @brief Move only, copy deleted.
	 *
	 * @{
	 */
	UBO(UBO&& other) noexcept : UBO()
	{
		std::swap(buffer, other.buffer);
		std::swap(allocation, other.allocation);
		std::swap(allocator, other.allocator);
		std::swap(size, other.size);
	}

	UBO& operator=(UBO&& other) noexcept
	{
		if (this == &other)
		{
			return *this;
		}
		std::swap(buffer, other.buffer);
		std::swap(allocation, other.allocation);
		std::swap(allocator, other.allocator);
		std::swap(size, other.size);
		return *this;
	}

	UBO(const UBO& other) = delete;
	UBO& operator=(const UBO& other) = delete;
	/**
	 * @}
	 */

	~UBO()
	{
		if (buffer != VK_NULL_HANDLE)
		{
			vmaDestroyBuffer(allocator, buffer, allocation);
		}
	}

	/**
	 * @name Getters.
	 *
	 * @brief Getters for private variables.
	 *
	 * @{
	 */
	VkDescriptorBufferInfo get_descriptorInfo() const
	{
		return VkDescriptorBufferInfo{buffer, 0, size};
	}
	/**
	 * @}
	 */

	/**
	 * @fn write(const Context& context, const T& data)
	 *
	 * @brief Writes the new data to the uniform buffer.
	 *
	 * @param context The Vulkan Context in use.
	 * @param data The data to write to the uniform buffer.
	 */
	void write(const T& data)
	{
		void* dataPtr;
		vmaMapMemory(allocator, allocation, &dataPtr);
		memcpy(dataPtr, &data, size);
		vmaUnmapMemory(allocator, allocation);
	}
};
} // namespace blaze
