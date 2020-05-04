
#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

/**
 * @brief A namespace used to hold the VkWrap structs to avoid random usage from outside the VkWrap internals.
 *
 * The structs in the vkw::base namespace are all to be used after generation using the macros.
 * They shouldn't be used as plain templated classes due to the possibilities of mistakes.
 */
namespace blaze::vkw::base
{
/**
 * @brief Vulkan Handle specialization of std::exchange.
 *
 * Used at destruction to simulataneously return and set value to VK_NULL_HANDLE.
 *
 * @param val The vulkan handle reference to erase. It is set to VK_NULL_HANDLE.
 *
 * @returns val.
 */
template <typename T>
T take(T& val)
{
	return std::exchange<T, T>(val, VK_NULL_HANDLE);
}

/**
 * @struct BaseWrapper
 *
 * @brief The basic wrapper for uniformity.
 * 
 * Basewrapper is only used for generating unmanaged wrappers around handles to force
 * a basic ownership relation in whichever holds, the raw handles being the non owned
 * references to the object.
 *
 * @tparam THandle The type of handle to own.
 */
template <typename THandle>
struct BaseWrapper
{
	THandle handle{VK_NULL_HANDLE};

    /**
     * @brief Default Constructor.
     */
	BaseWrapper() noexcept
	{
	}

    /**
     * @brief Main constructor.
     *
     * The handle is considered as owned by the wrapper. 
     * The wrapper is explicit in order to denote that.
     *
     * @param handle The Vulkan handle to own.
     */
	explicit BaseWrapper(THandle handle) noexcept : handle(handle)
	{
	}

    /**
     * @name Move Constructors.
     *
     * @brief Coopy Deleted.
     *
     * @{
     */
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
    /**
     * @}
     */

    /**
     * @brief Getter for the handle.
     */
	const THandle& get() const
	{
		return handle;
	}

    /**
     * @brief Checks if the handle is non-null
     * 
     * This is used to make sure of initialization of the handle before use.
     */
	inline bool valid() const
	{
		return handle != VK_NULL_HANDLE;
	}

    /**
     * @brief Destructor
     */
	~BaseWrapper()
	{
		take(handle);
	}
};
/**
 * @struct BaseCollection
 *
 * @brief The basic collection wrapper for uniformity.
 * 
 * BaseCollection is only used for generating unmanaged wrapper around a collection
 * of handles.
 *
 * @tparam THandle The type of handle to own.
 */
template <typename THandle>
struct BaseCollection
{
	std::vector<THandle> handles;

    /**
     * @brief Default Constructor.
     */
	BaseCollection() noexcept
	{
	}

    /**
     * @brief Main constructor.
     *
     * The handles are considered as owned by the wrapper. 
     * The wrapper is explicit in order to denote that,
     * and the handles have to be moved in.
     *
     * @param handles The vector of Vulkan handles to own.
     */
	explicit BaseCollection(std::vector<THandle>&& handles) noexcept : handles(handles)
	{
	}

    /**
     * @name Move Constructors.
     *
     * Copy Deleted.
     *
     * @{
     */
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
    /**
     * @}
     */

    /**
     * @brief Gets the vector of handles
     */
	const std::vector<THandle>& get() const
	{
		return handles;
	}

    /**
     * @brief Indexing operator that exposes the handle.
     */
	const THandle& operator[](uint32_t idx) const
	{
		return handles[idx];
	}

    /**
     * @brief Returns the number of handles owned.
     */
	uint32_t size() const
	{
		return static_cast<uint32_t>(handles.size());
	}

    /**
     * @brief Checks validity
     *
     * Validity is defined as whether the handles are empty - since that is the default initialization.
     */
	inline bool valid() const
	{
		return !handles.empty();
	}

    /**
     * @brief Destructor.
     *
     * Clears the vector of handles owned.
     */
	~BaseCollection()
	{
		handles.clear();
	}
};

/**
 * @struct IndependentHolder
 *
 * @brief Wrapper for Vulkan handles that need to be destroyed independtly.
 *
 * Manages destruction of handles that are not dependent on any other handle for destruction.
 * Also requires a specification of the destruction function.
 *
 * @tparam THandle Type of the handle to manage
 * @tparam TDestroy The destruction function to use for the given handle.
 */
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

    /**
     * @name Move Constructors
     *
     * Copy Deleted.
     *
     * @{
     */
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
    /**
     * @}
     */

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

/**
 * @struct DependentHolder
 *
 * @brief The most important wrapper to wrap handles that require some dependency to be used to deallocate.
 *
 * The dependency handle is not owned, while the handle is owned by the wrapper. At destruction, the relevant
 * passed destructor will be called.
 *
 * @tparam THandle Type of handle
 * @tparam TDep The Type of handle of dependency.
 * @tparam TDestroy The destroy function.
 */
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

/**
 * @struct DeviceDependentVector
 *
 * @brief A vector version of DependencyHandler with a VkDevice dependency.
 *
 * Wrapper to own a collection of handles which require a VkDevice to be destroyed.
 *
 * @tparam THandle Type of handle.
 * @tparam TDestroy The function for destroying the handle.
 */
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
