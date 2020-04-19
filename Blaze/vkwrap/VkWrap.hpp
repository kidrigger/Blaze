
#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <thirdparty/vma/vk_mem_alloc.h>
#include <util/createFunctions.hpp>
#include <util/debugMessenger.hpp>

namespace blaze::vkw
{
template <typename T>
static T take(T& val)
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

template <typename THandle, typename void (*TDestroy)(THandle, const VkAllocationCallbacks*)>
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

template <typename THandle, typename TDep, typename void (*TDestroy)(TDep, THandle, const VkAllocationCallbacks*)>
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

template <typename THandle, typename void (*TDestroy)(VkDevice, THandle, const VkAllocationCallbacks*)>
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
		: CommandBufferVector(std::move(other.handles), take(other.pool), take(other.device))
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
			take(pool);
			take(device);
		}
	}
};

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
	MemAllocator(MemAllocator&& other) noexcept : MemAllocator(take(other.handle))
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
			vmaDestroyAllocator(take(handle));
		}
	}
};

#define GEN_UNMANAGED_HOLDER(Type) using Type = BaseWrapper<Vk##Type>

GEN_UNMANAGED_HOLDER(PhysicalDevice);
GEN_UNMANAGED_HOLDER(Queue);

#define GEN_UNMANAGED_COLLECTION(Type) using Type##Vector = BaseCollection<Vk##Type>

GEN_UNMANAGED_COLLECTION(DescriptorSet);

#define GEN_INDEPENDENT_HOLDER(Type) using Type = IndependentHolder<Vk##Type, vkDestroy##Type>

GEN_INDEPENDENT_HOLDER(Instance);
GEN_INDEPENDENT_HOLDER(Device);

#define GEN_DEPENDENT_HOLDER(Type, TDep, TDestroy) using Type = DependentHolder<Vk##Type, TDep, TDestroy>

GEN_DEPENDENT_HOLDER(DebugUtilsMessengerEXT, VkInstance, util::destroyDebugUtilsMessengerEXT);

#define GEN_INSTANCE_DEPENDENT_HOLDER(Type) using Type = DependentHolder<Vk##Type, VkInstance, vkDestroy##Type>

GEN_INSTANCE_DEPENDENT_HOLDER(SurfaceKHR);

#define GEN_DEVICE_DEPENDENT_HOLDER(Type) using Type = DependentHolder<Vk##Type, VkDevice, vkDestroy##Type>

GEN_DEVICE_DEPENDENT_HOLDER(RenderPass);
GEN_DEVICE_DEPENDENT_HOLDER(DescriptorSetLayout);
GEN_DEVICE_DEPENDENT_HOLDER(DescriptorPool);
GEN_DEVICE_DEPENDENT_HOLDER(PipelineLayout);
GEN_DEVICE_DEPENDENT_HOLDER(Pipeline);
GEN_DEVICE_DEPENDENT_HOLDER(SwapchainKHR);
GEN_DEVICE_DEPENDENT_HOLDER(CommandPool);
GEN_DEVICE_DEPENDENT_HOLDER(ShaderModule);
GEN_DEVICE_DEPENDENT_HOLDER(Framebuffer);

#define GEN_DEVICE_DEPENDENT_COLLECTION(Type) using Type##Vector = DeviceDependentVector<Vk##Type, vkDestroy##Type>

GEN_DEVICE_DEPENDENT_COLLECTION(Fence);
GEN_DEVICE_DEPENDENT_COLLECTION(Semaphore);
GEN_DEVICE_DEPENDENT_COLLECTION(Framebuffer);
GEN_DEVICE_DEPENDENT_COLLECTION(ImageView);

} // namespace blaze::vkw
