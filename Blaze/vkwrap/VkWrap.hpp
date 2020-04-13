
#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <util/createFunctions.hpp>

namespace blaze::vkw
{
template <typename THandle, typename void (*TDestroy)(VkDevice, THandle, const VkAllocationCallbacks*)>
struct DeviceDependent
{
	template <typename T>
	static T take(T& val)
	{
		return std::exchange<T, T>(val, VK_NULL_HANDLE);
	}

	THandle handle{VK_NULL_HANDLE};
	VkDevice device{VK_NULL_HANDLE};

	DeviceDependent() noexcept
	{
	}

	DeviceDependent(THandle handle, VkDevice device) noexcept : handle(handle), device(device)
	{
	}

	DeviceDependent(const DeviceDependent& other) = delete;
	DeviceDependent& operator=(const DeviceDependent& other) = delete;
	DeviceDependent(DeviceDependent&& other) : DeviceDependent(take(other.handle), take(other.device))
	{
	}

	DeviceDependent& operator=(DeviceDependent&& other)
	{
		if (this == &other)
		{
			return *this;
		}
		std::swap(handle, other.handle);
		std::swap(device, other.device);
		return *this;
	}

	const THandle& get() const
	{
		return handle;
	}

	inline bool valid() const
	{
		return handle == VK_NULL_HANDLE;
	}

	~DeviceDependent()
	{
		if (device != VK_NULL_HANDLE)
		{
			TDestroy(take(device), take(handle), nullptr);
		}
	}
};

template <typename THandle, typename void (*TDestroy)(VkDevice, THandle, const VkAllocationCallbacks*)>
struct DeviceDependentVector
{
	template <typename T>
	static T take(T& val)
	{
		return std::exchange<T, T>(val, {});
	}

	std::vector<THandle> handles;
	VkDevice device{VK_NULL_HANDLE};

	DeviceDependentVector() noexcept
	{
	}

	DeviceDependentVector(std::vector<THandle>&& col, VkDevice device) noexcept
		: handles(std::move(col)), device(device)
	{
	}

	DeviceDependentVector(const DeviceDependentVector& other) = delete;
	DeviceDependentVector& operator=(const DeviceDependentVector& other) = delete;
	DeviceDependentVector(DeviceDependentVector&& other)
		: DeviceDependentVector(std::move(other.handles), take(other.device))
	{
	}

	DeviceDependentVector& operator=(DeviceDependentVector&& other)
	{
		if (this == &other)
		{
			return *this;
		}
		std::swap(handles, other.handles);
		std::swap(device, other.device);
		return *this;
	}

	const std::vector<THandle>& get() const
	{
		return handles;
	}

	const THandle& operator[](uint32_t idx) const
	{
		return handles[idx];
	}

	inline bool valid() const
	{
		return handle == VK_NULL_HANDLE;
	}

	~DeviceDependentVector()
	{
		if (device != VK_NULL_HANDLE)
		{
			for (THandle& h : handles)
			{
				TDestroy(device, take(h), nullptr);
			}
			take(device);
		}
	}
};

#define GEN_DEVICE_DEPENDENT_HOLDER(Type) using Type = DeviceDependent<Vk##Type, vkDestroy##Type>

GEN_DEVICE_DEPENDENT_HOLDER(RenderPass);
GEN_DEVICE_DEPENDENT_HOLDER(DescriptorSetLayout);
GEN_DEVICE_DEPENDENT_HOLDER(DescriptorPool);
GEN_DEVICE_DEPENDENT_HOLDER(PipelineLayout);
GEN_DEVICE_DEPENDENT_HOLDER(Pipeline);

#define GEN_DEVICE_DEPENDENT_COLLECTION(Type)                                                                          \
	using Type##Vector = DeviceDependentVector<Vk##Type, vkDestroy##Type>

GEN_DEVICE_DEPENDENT_COLLECTION(Fence);
GEN_DEVICE_DEPENDENT_COLLECTION(Semaphore);

} // namespace blaze::vkw
