
#pragma once

#include <memory>
#include <vector>

namespace blaze::util
{
/**
 * @brief A managing collection that is always packed contiguously.
 *
 * PackedHandler returns a handle that can be used to refer to the object from it as required.
 * The Handle will automatically erase the object on deletion, and it denotes an ownership of the object.
 */
template <typename T>
class PackedHandler
{
	struct InnerHandle
	{
		PackedHandler* parent;
		uint32_t index;

		InnerHandle(PackedHandler* parent, uint32_t index) noexcept : parent(parent), index(index)
		{
		}

		inline const T& get() const
		{
			return parent->get(index);
		}

		inline T& get()
		{
			return parent->get(index);
		}

		inline bool valid()
		{
			return parent != nullptr;
		}

		~InnerHandle()
		{
			if (valid())
			{
				parent->erase(index);
			}
		}
	};

	struct OuterHandle
	{
		std::shared_ptr<InnerHandle> handle;

		inline T& get()
		{
			return handle->get();
		}

		inline T& operator->()
		{
			return get();
		}

		inline const T& operator->() const
		{
			return get();
		}

		inline T& operator*()
		{
			return get();
		}

		inline const T& operator*() const
		{
			return get();
		}

		void destroy()
		{
			handle.reset();
		}
	};

	std::vector<T> data;
	std::vector<std::weak_ptr<InnerHandle>> handles;

public:
	using Handle = OuterHandle;

	/**
	 * @brief Adds the new value to the packed handler and returns the handle.
	 */
	[[nodiscard]] Handle add(T&& val)
	{
		auto handle = std::make_shared<InnerHandle>(this, static_cast<uint32_t>(data.size()));
		handles.emplace_back(handle);
		data.push_back(std::forward<T>(val));
		return {handle};
	}

	/**
	 * @brief Adds the new value to the packed handler and returns the handle.
	 */
	[[nodiscard]] Handle add(const T& val)
	{
		auto handle = std::make_shared<InnerHandle>(this, static_cast<uint32_t>(data.size()));
		handles.emplace_back(handle);
		data.push_back(val);
		return {handle};
	}

	auto begin() const
	{
		return data.begin();
	}

	auto end() const
	{
		return data.end();
	}

	const std::vector<T>& get_data() const
	{
		return data;
	}

	uint32_t get_size() const
	{
		return static_cast<uint32_t>(data.size());
	}

private:
	T& get(uint32_t index)
	{
		assert(index < data.size());
		return data[index];
	}

	const T& get(uint32_t index) const
	{
		assert(index < data.size());
		return data[index];
	}

	void erase(uint32_t index)
	{
		if (index < data.size() - 1)
		{
			std::swap(handles.at(index).lock()->index, handles.back().lock()->index);

			std::swap(data.at(index), data.back());
			std::swap(handles.at(index), handles.back());
		}
		data.pop_back();
		handles.pop_back();
	}
};

} // namespace blaze::util
