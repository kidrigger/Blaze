
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
class BaseUBO
{
protected:
	VkBuffer buffer{VK_NULL_HANDLE};
	VmaAllocation allocation{VK_NULL_HANDLE};
	VmaAllocator allocator{VK_NULL_HANDLE};
	size_t size{0};

public:
    /**
     * @brief Default Constructor.
     */
	BaseUBO() noexcept
	{
	}

    /**
     * @brief Main Constructor.
     *
     * @param context Pointer to the Context in use.
     * @param size The size of the actual buffer to allocate.
     */
	BaseUBO(const Context* context, size_t size) noexcept;

	/**
	 * @name Move Constructors.
	 *
	 * @brief Move only, copy deleted.
	 *
	 * @{
	 */
	BaseUBO(BaseUBO&& other) noexcept;
	BaseUBO& operator=(BaseUBO&& other) noexcept;
	BaseUBO(const BaseUBO& other) = delete;
	BaseUBO& operator=(const BaseUBO& other) = delete;
	/**
	 * @}
	 */

	/**
	 * @brief Creates a new VkDescriptorBufferInfo for the UBO
	 */
	VkDescriptorBufferInfo get_descriptorInfo() const
	{
		return VkDescriptorBufferInfo{buffer, 0, size};
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
     * @brief Destructor
     */
    virtual ~BaseUBO();
};

/**
 * @tparam T The data object to be stored in the UBO.
 *
 * @brief The class to handle operations on a uniform buffer.
 */
template <typename T>
class UBO : public BaseUBO
{
public:
	/**
	 * @brief Default Constructor.
	 */
	UBO() noexcept : BaseUBO()
	{
	}

	/**
	 * @brief Main constructor of the class.
	 *
	 * @param context Pointer to the current Vulkan Context.
	 * @param data The data object of type T to store.
	 */
	UBO(const Context* context, const T& data) noexcept : BaseUBO(context, sizeof(data))
	{
		write(data);
	}

	/**
	 * @brief Writes the new data to the uniform buffer.
	 *
	 * @param data The data to write to the uniform buffer.
	 */
	void write(const T& data)
	{
		this->writeData(&data, sizeof(T));
	}
};

class UBODataVector
{
private:
	using ubo_t = BaseUBO;
	std::vector<ubo_t> ubos;
	uint32_t count;

public:
	UBODataVector() noexcept
	{
	}

	UBODataVector(const Context* context, uint32_t size, uint32_t numUBOS) noexcept : count(numUBOS)
	{
		ubos.reserve(count);
		for (uint32_t i = 0; i < count; ++i)
		{
			ubos.emplace_back(context, size);
		}
	}

	UBODataVector(const UBODataVector& other) = delete;
	UBODataVector& operator=(const UBODataVector& other) = delete;
	UBODataVector(UBODataVector&& other) = default;
	UBODataVector& operator=(UBODataVector&& other) = default;

	const std::vector<ubo_t>& get() const
	{
		return ubos;
	}

	uint32_t size() const
	{
		return count;
	}

	ubo_t& operator[](uint32_t idx)
	{
		return ubos[idx];
	}

	const ubo_t& operator[](uint32_t idx) const
	{
		return ubos[idx];
	}

	~UBODataVector()
	{
		ubos.clear();
		count = 0;
	}
};

/**
 * @brief a Collection of \ref UBO connected to the same data.
 *
 * Most common usage of UBOs require one UBO per uniform per swapchain image.
 * A collection such as this allows easy management of the buffers.
 */
template <typename T>
class UBOVector
{
private:
	using ubo_t = UBO<T>;

	std::vector<ubo_t> ubos;
	uint32_t count;

public:
	UBOVector() noexcept
	{
	}

	UBOVector(const Context* context, const T& data, uint32_t numUBOS) noexcept : count(numUBOS)
	{
		ubos.reserve(count);
		for (uint32_t i = 0; i < count; ++i)
		{
			ubos.emplace_back(context, data);
		}
	}

	UBOVector(const UBOVector& other) = delete;
	UBOVector& operator=(const UBOVector& other) = delete;
	UBOVector(UBOVector&& other) = default;
	UBOVector& operator=(UBOVector&& other) = default;

	const std::vector<ubo_t>& get() const
	{
		return ubos;
	}

	uint32_t size() const
	{
		return count;
	}

	ubo_t& operator[](uint32_t idx)
	{
		return ubos[idx];
	}

	const ubo_t& operator[](uint32_t idx) const
	{
		return ubos[idx];
	}

	~UBOVector()
	{
		ubos.clear();
		count = 0;
	}
};

} // namespace blaze
