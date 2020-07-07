
#pragma once

#include <core/Context.hpp>
#include <cstring>

#include <thirdparty/vma/vk_mem_alloc.h>

namespace blaze
{
/**
 * @brief The base class for all UBOs
 *
 * The type independent size dependent generic class to implement a buffer for
 * uniforms.
 * Mostly not to be used directly, but extended by a type safe derived class.
 */
class SSBO
{
protected:
	vkw::Buffer buffer;
	size_t size{0};

public:
    /**
     * @brief Default Constructor.
     */
	SSBO() noexcept
	{
	}

    /**
     * @brief Main Constructor.
     *
     * @param context Pointer to the Context in use.
     * @param size The size of the actual buffer to allocate.
     */
	SSBO(const Context* context, size_t size) noexcept;

	/**
	 * @name Move Constructors.
	 *
	 * @brief Move only, copy deleted.
	 *
	 * @{
	 */
	SSBO(SSBO&& other) noexcept;
	SSBO& operator=(SSBO&& other) noexcept;
	SSBO(const SSBO& other) = delete;
	SSBO& operator=(const SSBO& other) = delete;
	/**
	 * @}
	 */

	/**
	 * @brief Creates a new VkDescriptorBufferInfo for the UBO
	 */
	inline VkDescriptorBufferInfo get_descriptorInfo() const
	{
		return VkDescriptorBufferInfo{
			buffer.handle,
			0,
			size,
		};
	}
	/**
	 * @}
	 */

    /**
     * @brief Writes data to the buffer.
     *
     * @param data The pointer to the data to write into buffer.
     * @param size The size of the data to write into the buffer.
     */
	void writeData(const void* data, size_t size);

	/**
	 * @brief Writes data to the buffer.
	 *
	 * @param data The pointer to the data to write into buffer.
	 * @param size The size of the data to write into the buffer.
	 */
	void writeData(const void* data, size_t offset, size_t size);
};
} // namespace blaze
