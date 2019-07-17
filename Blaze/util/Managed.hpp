
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
}
