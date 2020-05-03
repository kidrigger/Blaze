
#pragma once

#include <core/Context.hpp>
#include <cstring>

#include <thirdparty/vma/vk_mem_alloc.h>

namespace blaze
{
class BaseUBO
{
protected:
	VkBuffer buffer{VK_NULL_HANDLE};
	VmaAllocation allocation{VK_NULL_HANDLE};
	VmaAllocator allocator{VK_NULL_HANDLE};
	size_t size{0};

public:
	BaseUBO() noexcept
	{
	}

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

	virtual ~BaseUBO();

protected:
	void writeData(const void* data, size_t size);
};

/**
 * @class UBO
 *
 * @tparam T The data object to be stored in the UBO.
 *
 * @brief The class to handle operations on a uniform buffer.
 */
template <typename T>
class UBO : public BaseUBO
{
public:
	/**
	 * @fn UBO()
	 *
	 * @brief Default Constructor.
	 */
	UBO() noexcept : BaseUBO()
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
	UBO(const Context* context, const T& data) noexcept : BaseUBO(context, sizeof(data))
	{
		write(data);
	}

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
		this->writeData(&data, sizeof(T));
	}
};

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
