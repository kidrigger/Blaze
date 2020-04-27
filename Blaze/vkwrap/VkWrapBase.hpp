
#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

namespace blaze::vkw::base
{
template <typename T>
T take(T& val)
{
	return std::exchange<T, T>(val, VK_NULL_HANDLE);
}

template <typename THandle>
struct BaseWrapper
{
	THandle handle{VK_NULL_HANDLE};

	BaseWrapper() noexcept
	{
	}

	explicit BaseWrapper(THandle handle) noexcept : handle(handle)
	{
	}

	BaseWrapper(const BaseWrapper& other) = delete;
	BaseWrapper& operator=(const BaseWrapper& other) = delete;
	BaseWrapper(BaseWrapper&& other) noexcept : BaseWrapper(take(other.handle))
	{
	}

	BaseWrapper& operator=(BaseWrapper&& other) noexcept
	{
		if (this == &other)
		{
			return *this;
		}
		std::swap(handle, other.handle);
		return *this;
	}

	const THandle& get() const
	{
		return handle;
	}

	inline bool valid() const
	{
		return handle != VK_NULL_HANDLE;
	}

	~BaseWrapper()
	{
		take(handle);
	}
};

template <typename THandle>
struct BaseCollection
{
	std::vector<THandle> handles{VK_NULL_HANDLE};

	BaseCollection() noexcept
	{
	}

	explicit BaseCollection(std::vector<THandle>&& handles) noexcept : handles(handles)
	{
	}

	BaseCollection(const BaseCollection& other) = delete;
	BaseCollection& operator=(const BaseCollection& other) = delete;
	BaseCollection(BaseCollection&& other) noexcept : BaseCollection(std::move(other.handles))
	{
	}

	BaseCollection& operator=(BaseCollection&& other) noexcept
	{
		if (this == &other)
		{
			return *this;
		}
		handles = std::move(other.handles);
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

	uint32_t size() const
	{
		return static_cast<uint32_t>(handles.size());
	}

	inline bool valid() const
	{
		return !handles.empty();
	}

	~BaseCollection()
	{
		handles.clear();
	}
};

template <typename THandle, void (*TDestroy)(THandle, const VkAllocationCallbacks*)>
struct IndependentHolder
{
	THandle handle{VK_NULL_HANDLE};

	IndependentHolder() noexcept
	{
	}

	explicit IndependentHolder(THandle handle) noexcept : handle(handle)
	{
	}

	IndependentHolder(const IndependentHolder& other) = delete;
	IndependentHolder& operator=(const IndependentHolder& other) = delete;
	IndependentHolder(IndependentHolder&& other) noexcept : IndependentHolder(take(other.handle))
	{
	}

	IndependentHolder& operator=(IndependentHolder&& other) noexcept
	{
		if (this == &other)
		{
			return *this;
		}
		std::swap(handle, other.handle);
		return *this;
	}

	const THandle& get() const
	{
		return handle;
	}

	inline bool valid() const
	{
		return handle != VK_NULL_HANDLE;
	}

	~IndependentHolder()
	{
		if (valid())
		{
			TDestroy(take(handle), nullptr);
		}
	}
};

template <typename THandle, typename TDep, void (*TDestroy)(TDep, THandle, const VkAllocationCallbacks*)>
struct DependentHolder
{
	THandle handle{VK_NULL_HANDLE};
	TDep dependency{VK_NULL_HANDLE};

	DependentHolder() noexcept
	{
	}

	explicit DependentHolder(THandle handle, TDep dependency) noexcept : handle(handle), dependency(dependency)
	{
	}

	DependentHolder(const DependentHolder& other) = delete;
	DependentHolder& operator=(const DependentHolder& other) = delete;
	DependentHolder(DependentHolder&& other) noexcept : DependentHolder(take(other.handle), take(other.dependency))
	{
	}

	DependentHolder& operator=(DependentHolder&& other) noexcept
	{
		if (this == &other)
		{
			return *this;
		}
		std::swap(handle, other.handle);
		std::swap(dependency, other.dependency);
		return *this;
	}

	const THandle& get() const
	{
		return handle;
	}

	inline bool valid() const
	{
		return handle != VK_NULL_HANDLE;
	}

	~DependentHolder()
	{
		if (valid())
		{
			TDestroy(take(dependency), take(handle), nullptr);
		}
	}
};

template <typename THandle, void (*TDestroy)(VkDevice, THandle, const VkAllocationCallbacks*)>
struct DeviceDependentVector
{
	std::vector<THandle> handles;
	VkDevice device{VK_NULL_HANDLE};

	DeviceDependentVector() noexcept
	{
	}

	explicit DeviceDependentVector(std::vector<THandle>&& col, VkDevice device) noexcept
		: handles(std::move(col)), device(device)
	{
	}

	DeviceDependentVector(const DeviceDependentVector& other) = delete;
	DeviceDependentVector& operator=(const DeviceDependentVector& other) = delete;
	DeviceDependentVector(DeviceDependentVector&& other) noexcept
		: DeviceDependentVector(std::move(other.handles), take(other.device))
	{
	}

	DeviceDependentVector& operator=(DeviceDependentVector&& other) noexcept
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

	uint32_t size() const
	{
		return static_cast<uint32_t>(handles.size());
	}

	inline bool valid() const
	{
		return !handles.empty();
	}

	~DeviceDependentVector()
	{
		if (valid())
		{
			for (THandle& h : handles)
			{
				TDestroy(device, take(h), nullptr);
			}
			take(device);
		}
	}
};
} // namespace blaze::vkw::base