
#pragma once

#include <functional>

namespace blaze::util
{
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
		Managed() noexcept
			: handle(), destroyer([](T& hand) {}), is_valid(false)
		{
		}

		template <typename U>
		Managed(T handle, U destroyer) noexcept
			: handle(handle), destroyer(destroyer), is_valid(true)
		{
		}

		Managed(Managed&& other) noexcept
			: Managed()
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

		inline const T& get() const { return handle; }
		inline void set(const T& val) { handle = val; }
		inline T* data() { return &handle; }
		inline const T* data() const { return &handle; }
		inline bool valid() const { return is_valid; }

		~Managed()
		{
			destroyer(handle);
			is_valid = false;
		}
	};

	template <typename T>
	class Unmanaged
	{
	private:
		T handle;
		bool is_valid;
	public:
		Unmanaged() noexcept
			: handle(), is_valid(false)
		{
		}

		Unmanaged(T handle) noexcept
			: handle(handle), is_valid(true)
		{
		}

		Unmanaged(Unmanaged&& other) noexcept
			: Unmanaged()
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

		inline const T& get() const { return handle; }
		inline void set(const T& val) { handle = val; }
		inline T* data() { return &handle; }
		inline bool const valid() { return is_valid; }
	};

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
		ManagedVector() noexcept
			: handles(), destroyer([](T& hand) {}), is_valid(false)
		{
		}

		template <typename U>
		ManagedVector(const std::vector<T>& handles, U destroyer) noexcept
			: handles(handles), destroyer(destroyer), is_valid(true)
		{
		}

		ManagedVector(ManagedVector&& other) noexcept
			: ManagedVector()
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

		const std::vector<T>& get() const { return handles; }
		const T& get(size_t index) const { return handles[index]; }
		void set(size_t index, const T& val) { handles[index] = val; }
		T& operator[](size_t index) { return handles[index]; }
		const T& operator[](size_t index) const { return handles[index]; }
		T* data() { return handles.data(); }
		void resize(size_t size) { handles.resize(size); }
		size_t size() const { return handles.size(); }

		bool valid() const { return is_valid; }

		~ManagedVector()
		{
			for (auto handle : handles)
			{
				destroyer(handle);
			}
			is_valid = false;
		}
	};

	template <typename T>
	class ManagedVector<T,false>
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
		ManagedVector() noexcept
			: handles(), destroyer([](std::vector<T>& hand) {}), is_valid(false)
		{
		}

		template <typename U>
		ManagedVector(const std::vector<T>& handles, U destroyer) noexcept
			: handles(handles), destroyer(destroyer), is_valid(true)
		{
		}

		template <typename U>
		ManagedVector(std::vector<T>&& handles, U destroyer) noexcept
			: handles(std::move(handles)), destroyer(destroyer), is_valid(true)
		{
		}

		ManagedVector(ManagedVector&& other) noexcept
			: ManagedVector()
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

		const std::vector<T>& get() const { return handles; }
		const T& get(size_t index) const { return handles[index]; }
		void set(size_t index, const T& val) { handles[index] = val; }
		T& operator[](size_t index) { return handles[index]; }
		const T& operator[](size_t index) const { return handles[index]; }
		T* data() { return handles.data(); }
		void resize(size_t size) { handles.resize(size); }
		size_t size() const { return handles.size(); }
		bool valid() const { return is_valid; }

		~ManagedVector()
		{
			destroyer(handles);
		}
	};

	template <typename T>
	class UnmanagedVector
	{
	private:
		std::vector<T> handles;
		bool is_valid;
	public:
		UnmanagedVector() noexcept
			: handles(), is_valid(false)
		{
		}

		UnmanagedVector(const std::vector<T>& handles) noexcept
			: handles(handles), is_valid(true)
		{
		}

		UnmanagedVector(std::vector<T>&& handles) noexcept
			: handles(std::move(handles)), is_valid(true)
		{
		}

		UnmanagedVector(UnmanagedVector&& other) noexcept
			: UnmanagedVector()
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

		const std::vector<T>& get() const { return handles; }
		const T& get(size_t index) const { return handles[index]; }
		void set(size_t index, const T& val) { handles[index] = val; }
		T& operator[](size_t index) { return handles[index]; }
		const T& operator[](size_t index) const { return handles[index]; }
		T* data() { return handles.data(); }
		void resize(size_t size) { handles.resize(size); }
		size_t size() const { return handles.size(); }

		bool valid() const { return is_valid; }
	};
}
