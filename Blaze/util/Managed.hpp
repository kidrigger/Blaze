
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

		friend void swap(Managed& a, Managed& b)
		{
			using std::swap;
			swap(a.handle, b.handle);
			swap(a.destroyer, b.destroyer);
		}
	public:
		Managed() noexcept
			: handle(), destroyer([](T& hand) {})
		{
		}

		template <typename U>
		Managed(T handle, U destroyer) noexcept
			: handle(handle), destroyer(destroyer)
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

		const T& get() const { return handle; }
		void set(const T& val) { handle = val; }
		T* data() { return &handle; }

		~Managed()
		{
			destroyer(handle);
		}
	};

	template <typename T>
	class Unmanaged
	{
	private:
		T handle;
	public:
		Unmanaged() noexcept
			: handle()
		{
		}

		Unmanaged(T handle) noexcept
			: handle(handle)
		{
		}

		Unmanaged(Unmanaged&& other) noexcept
			: Unmanaged()
		{
			std::swap(handle, other.handle);
		}

		Unmanaged& operator=(Unmanaged&& other) noexcept
		{
			if (this == &other)
			{
				return *this;
			}
			std::swap(handle, other.handle);
			return *this;
		}

		Unmanaged(const Unmanaged& other) = delete;
		Unmanaged& operator=(const Unmanaged& other) = delete;

		const T& get() const { return handle; }
		void set(const T& val) { handle = val; }
		T* data() { return &handle; }
	};

	template <typename T>
	class ManagedVector
	{
	private:
		std::vector<T> handles;
		std::function<void(T&)> destroyer;

		friend void swap(ManagedVector& a, ManagedVector& b)
		{
			using std::swap;
			swap(a.handles, b.handles);
			swap(a.destroyer, b.destroyer);
		}
	public:
		ManagedVector() noexcept
			: handles(), destroyer([](T& hand) {})
		{
		}

		template <typename U>
		ManagedVector(const std::vector<T>& handles, U destroyer) noexcept
			: handles(handles), destroyer(destroyer)
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

		const T& get(size_t index) const { return handles[index]; }
		void set(size_t index, const T& val) { handles[index] = val; }
		T& operator[](size_t index) { return handles[index]; }
		const T& operator[](size_t index) const { return handles[index]; }
		T* data() { return handles.data(); }
		void resize(size_t size) { handles.resize(size); }
		size_t size() const { return handles.size(); }

		~ManagedVector()
		{
			for (auto handle : handles)
			{
				destroyer(handle);
			}
		}
	};

	template <typename T>
	class UnmanagedVector
	{
	private:
		std::vector<T> handles;
	public:
		UnmanagedVector() noexcept
			: handles()
		{
		}

		UnmanagedVector(const std::vector<T>& handles) noexcept
			: handles(handles)
		{
		}

		UnmanagedVector(UnmanagedVector&& other) noexcept
			: UnmanagedVector()
		{
			std::swap(handles, other.handles);
		}

		UnmanagedVector& operator=(UnmanagedVector&& other) noexcept
		{
			if (this == &other)
			{
				return *this;
			}
			std::swap(handles, other.handles);
			return *this;
		}

		UnmanagedVector(const UnmanagedVector& other) = delete;
		UnmanagedVector& operator=(const UnmanagedVector& other) = delete;

		const T& get(size_t index) const { return handles[index]; }
		void set(size_t index, const T& val) { handles[index] = val; }
		T& operator[](size_t index) { return handles[index]; }
		const T& operator[](size_t index) const { return handles[index]; }
		T* data() { return handles.data(); }
		void resize(size_t size) { handles.resize(size); }
		size_t size() const { return handles.size(); }
	};
}
