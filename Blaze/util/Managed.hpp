
#pragma once

#include <functional>

namespace blaze::util
{
/**
 * @class Managed
 *
 * @tparam T The type to be managed.
 *
 * @brief Scope managed RAII class.
 *
 * Managed takes a handle or object and a custom destructor
 * \a std::function to be called when the Managed object goes
 * out of scope.
 */
template <typename T>
class Managed
{
private:
	T handle;
	std::function<void(T&)> destroyer;
	bool is_valid;

	friend void swap(Managed& a, Managed& b)
	{
		using std::swap;
		swap(a.handle, b.handle);
		swap(a.destroyer, b.destroyer);
		swap(a.is_valid, b.is_valid);
	}

public:
	/**
	 * @fn Managed()
	 *
	 * @brief Default Constructor.
	 */
	Managed() noexcept : handle(), destroyer([](T& hand) {}), is_valid(false)
	{
	}

	/**
	 * @fn Managed(T handle, Callable destroyer)
	 *
	 * @tparam Callable Any callable type.
	 *
	 * @param handle The data handle to be managed.
	 * @param destroyer The function used to destroy the handle.
	 */
	template <typename Callable>
	Managed(T handle, Callable destroyer) noexcept : handle(handle), destroyer(destroyer), is_valid(true)
	{
	}

	/**
	 * @name Move Constructor.
	 *
	 * @brief Move only, copy deleted.
	 *
	 * @{
	 */
	Managed(Managed&& other) noexcept : Managed()
	{
		swap(*this, other);
	}

	Managed& operator=(Managed&& other) noexcept
	{
		if (this == &other)
		{
			return *this;
		}
		swap(*this, other);
		return *this;
	}

	Managed(const Managed& other) = delete;
	Managed& operator=(const Managed& other) = delete;
	/**
	 * @}
	 */

	/**
	 * @name Getters.
	 *
	 * @brief Getters for private members.
	 *
	 * @{
	 */
	inline const T& get() const
	{
		return handle;
	}
	inline void set(const T& val)
	{
		handle = val;
	}
	inline T* data()
	{
		return &handle;
	}
	inline const T* data() const
	{
		return &handle;
	}
	/**
	 * @}
	 */

	/**
	 * @fn valid()
	 *
	 * @brief Checks the validity of the handle.
	 *
	 * A Managed handle is considered valid if the object
	 * was constructed using the main constructor and
	 * the handle has not been destroyed.
	 *
	 * @returns true If valid.
	 * @returns false If invalid.
	 */
	inline bool valid() const
	{
		return is_valid;
	}

	/**
	 * @fn ~Managed()
	 *
	 * @brief Destructor of Managed.
	 *
	 * Calls the destroyer function on the handle.
	 */
	~Managed()
	{
		if (is_valid)
		{
			destroyer(handle);
			is_valid = false;
		}
	}
};

/**
 * @class Unmanaged
 *
 * @tparam T The type of handle wrapped.
 *
 * @brief A wrapper for handle for uniformity.
 */
template <typename T>
class Unmanaged
{
private:
	T handle;
	bool is_valid;

public:
	/**
	 * @fn Unmanaged()
	 *
	 * @brief Default Constructor.
	 */
	Unmanaged() noexcept : handle(), is_valid(false)
	{
	}

	/**
	 * @fn Unmanaged(T handle)
	 *
	 * @brief Main constructor.
	 *
	 * @param handle The handle.
	 */
	Unmanaged(T handle) noexcept : handle(handle), is_valid(true)
	{
	}

	/**
	 * @name Move Constructors.
	 *
	 * @brief Move only, copy deleted.
	 *
	 * @{
	 */
	Unmanaged(Unmanaged&& other) noexcept : Unmanaged()
	{
		std::swap(handle, other.handle);
		std::swap(is_valid, other.is_valid);
	}

	Unmanaged& operator=(Unmanaged&& other) noexcept
	{
		if (this == &other)
		{
			return *this;
		}
		std::swap(handle, other.handle);
		std::swap(is_valid, other.is_valid);
		return *this;
	}

	Unmanaged(const Unmanaged& other) = delete;
	Unmanaged& operator=(const Unmanaged& other) = delete;
	/**
	 * @}
	 */

	/**
	 * @name Getters.
	 *
	 * @brief Getters for private fields.
	 *
	 * @{
	 */
	inline const T& get() const
	{
		return handle;
	}
	inline void set(const T& val)
	{
		handle = val;
	}
	inline T* data()
	{
		return &handle;
	}
	/**
	 * @}
	 */

	/**
	 * @fn valid()
	 *
	 * @brief Checks validity of the handle.
	 *
	 * @returns false if default constructed.
	 * @returns true otherwise.
	 */
	inline bool const valid() const
	{
		return is_valid;
	}
};

/**
 * @class ManagedVector
 *
 * @tparam T The type of the handles in the vector.
 * @tparam singular (bool) Whether the destroyer destroys a single handle, or the vector of handles.
 *
 * @brief Manages lifetime of the handles of entire vector.
 */
template <typename T, bool singular = true>
class ManagedVector
{
private:
	std::vector<T> handles;
	std::function<void(T&)> destroyer;
	bool is_valid;

	friend void swap(ManagedVector& a, ManagedVector& b)
	{
		using std::swap;
		swap(a.handles, b.handles);
		swap(a.destroyer, b.destroyer);
		swap(a.is_valid, b.is_valid);
	}

public:
	/**
	 * @fn ManagedVector()
	 *
	 * @brief Default Constructor.
	 */
	ManagedVector() noexcept : handles(), destroyer([](T& hand) {}), is_valid(false)
	{
	}

	/**
	 * @fn ManagedVector(const std::vector<T>& handles, U destroyer)
	 *
	 * @brief Main Constructor.
	 *
	 * @param handles Vector of all handles.
	 * @param destroyer The destroyer function for a single handle.
	 */
	template <typename U>
	ManagedVector(const std::vector<T>& handles, U destroyer) noexcept
		: handles(handles), destroyer(destroyer), is_valid(true)
	{
	}

	/**
	 * @name Move constructors.
	 *
	 * @brief Move only, copy deleted.
	 *
	 * @{
	 */
	ManagedVector(ManagedVector&& other) noexcept : ManagedVector()
	{
		swap(*this, other);
	}

	ManagedVector& operator=(ManagedVector&& other) noexcept
	{
		if (this == &other)
		{
			return *this;
		}
		swap(*this, other);
		return *this;
	}

	ManagedVector(const ManagedVector& other) = delete;
	ManagedVector& operator=(const ManagedVector& other) = delete;
	/**
	 * @}
	 */

	/**
	 * @name Getters
	 *
	 * @brief Getters for private variables.
	 *
	 * @{
	 */
	const std::vector<T>& get() const
	{
		return handles;
	}
	const T& get(size_t index) const
	{
		return handles[index];
	}
	void set(size_t index, const T& val)
	{
		handles[index] = val;
	}
	T& operator[](size_t index)
	{
		return handles[index];
	}
	const T& operator[](size_t index) const
	{
		return handles[index];
	}
	T* data()
	{
		return handles.data();
	}
	void resize(size_t size)
	{
		handles.resize(size);
	}
	size_t size() const
	{
		return handles.size();
	}
	/**
	 * @}
	 */

	/**
	 * @fn valid()
	 *
	 * @brief Returns validity of the handles.
	 *
	 * The ManagedVector is considered valid if it is constructed successfully using the main constructor.
	 *
	 * @returns true if successfully constructed.
	 * @returns false otherwise.
	 */
	bool valid() const
	{
		return is_valid;
	}

	/**
	 * @fn ~ManagedVector()
	 *
	 * @brief Destructor for the handles.
	 */
	~ManagedVector()
	{
		for (auto handle : handles)
		{
			destroyer(handle);
		}
		is_valid = false;
	}
};

/**
 * @class ManagedVector<T,false>
 *
 * @tparam T The type of the handles in the vector.
 *
 * Specialized for destroyers that use entire vectors.
 *
 * @brief Manages lifetime of the handles of entire vector.
 */
template <typename T>
class ManagedVector<T, false>
{
private:
	std::vector<T> handles;
	std::function<void(std::vector<T>&)> destroyer;
	bool is_valid;

	friend void swap(ManagedVector& a, ManagedVector& b)
	{
		using std::swap;
		swap(a.handles, b.handles);
		swap(a.destroyer, b.destroyer);
		swap(a.is_valid, b.is_valid);
	}

public:
	/**
	 * @fn ManagedVector()
	 *
	 * @brief Default Constructor.
	 */
	ManagedVector() noexcept : handles(), destroyer([](std::vector<T>& hand) {}), is_valid(false)
	{
	}

	/**
	 * @fn ManagedVector(const std::vector<T>& handles, U destroyer)
	 *
	 * @brief Main Constructor.
	 *
	 * @param handles Vector of all handles.
	 * @param destroyer The destroyer function for a single handle.
	 */
	template <typename U>
	ManagedVector(const std::vector<T>& handles, U destroyer) noexcept
		: handles(handles), destroyer(destroyer), is_valid(true)
	{
	}

	/*
	template <typename U>
	ManagedVector(std::vector<T>&& handles, U destroyer) noexcept
		: handles(std::move(handles)), destroyer(destroyer), is_valid(true)
	{
	}
	*/

	/**
	 * @name Move constructors.
	 *
	 * @brief Move only, copy deleted.
	 *
	 * @{
	 */
	ManagedVector(ManagedVector&& other) noexcept : ManagedVector()
	{
		swap(*this, other);
	}

	ManagedVector& operator=(ManagedVector&& other) noexcept
	{
		if (this == &other)
		{
			return *this;
		}
		swap(*this, other);
		return *this;
	}

	ManagedVector(const ManagedVector& other) = delete;
	ManagedVector& operator=(const ManagedVector& other) = delete;
	/**
	 * @}
	 */

	/**
	 * @name Getters
	 *
	 * @brief Getters for private variables.
	 *
	 * @{
	 */
	const std::vector<T>& get() const
	{
		return handles;
	}
	const T& get(size_t index) const
	{
		return handles[index];
	}
	void set(size_t index, const T& val)
	{
		handles[index] = val;
	}
	T& operator[](size_t index)
	{
		return handles[index];
	}
	const T& operator[](size_t index) const
	{
		return handles[index];
	}
	T* data()
	{
		return handles.data();
	}
	void resize(size_t size)
	{
		handles.resize(size);
	}
	size_t size() const
	{
		return handles.size();
	}
	/**
	 * @}
	 */

	/**
	 * @fn valid()
	 *
	 * @brief Returns validity of the handles.
	 *
	 * The ManagedVector is considered valid if it is constructed successfully using the main constructor.
	 *
	 * @returns true if successfully constructed.
	 * @returns false otherwise.
	 */
	bool valid() const
	{
		return is_valid;
	}

	/**
	 * @fn ~ManagedVector()
	 *
	 * @brief Destructor for the handles.
	 */
	~ManagedVector()
	{
		destroyer(handles);
	}
};

/**
 * @class UnmanagedVector
 *
 * @tparam T Type of the handle.
 *
 * @brief Wrapper on vector of handles for consistency.
 */
template <typename T>
class UnmanagedVector
{
private:
	std::vector<T> handles;
	bool is_valid;

public:
	/**
	 * @fn UnmanagedVector()
	 *
	 * @brief Default Constructor.
	 */
	UnmanagedVector() noexcept : handles(), is_valid(false)
	{
	}

	/**
	 * @fn UnmanagedVector(const std::vector<T>& handles)
	 *
	 * @brief Copies the handles for construction.
	 */
	UnmanagedVector(const std::vector<T>& handles) noexcept : handles(handles), is_valid(true)
	{
	}

	/**
	 * @fn UnmanagedVector(std::vector<T>&& handles)
	 *
	 * @brief Moves the handles for construction.
	 */
	UnmanagedVector(std::vector<T>&& handles) noexcept : handles(std::move(handles)), is_valid(true)
	{
	}

	/**
	 * @name Move Constructors.
	 *
	 * @brief Move only, copy deleted.
	 *
	 * @{
	 */
	UnmanagedVector(UnmanagedVector&& other) noexcept : UnmanagedVector()
	{
		std::swap(handles, other.handles);
		std::swap(is_valid, other.is_valid);
	}

	UnmanagedVector& operator=(UnmanagedVector&& other) noexcept
	{
		if (this == &other)
		{
			return *this;
		}
		std::swap(handles, other.handles);
		std::swap(is_valid, other.is_valid);
		return *this;
	}

	UnmanagedVector(const UnmanagedVector& other) = delete;
	UnmanagedVector& operator=(const UnmanagedVector& other) = delete;
	/**
	 * @}
	 */

	/**
	 * @name Getters.
	 *
	 * @brief Getters for private members.
	 *
	 * @{
	 */
	const std::vector<T>& get() const
	{
		return handles;
	}
	const T& get(size_t index) const
	{
		return handles[index];
	}
	void set(size_t index, const T& val)
	{
		handles[index] = val;
	}
	T& operator[](size_t index)
	{
		return handles[index];
	}
	const T& operator[](size_t index) const
	{
		return handles[index];
	}
	T* data()
	{
		return handles.data();
	}
	void resize(size_t size)
	{
		handles.resize(size);
	}
	size_t size() const
	{
		return handles.size();
	}
	/**
	 * @}
	 */

	/**
	 * @fn valid()
	 *
	 * @brief Returns the validity of the construction.
	 *
	 * @returns false if default constructed.
	 * @returns true otherwise.
	 */
	bool valid() const
	{
		return is_valid;
	}
};
} // namespace blaze::util
