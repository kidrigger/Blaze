
#pragma once

#include <memory>

namespace blaze::util
{
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
	};

	std::vector<T> data;
	std::vector<std::weak_ptr<InnerHandle>> handles;

public:
	using Handle = OuterHandle;

	[[nodiscard]] Handle add(T&& val)
	{
		auto handle = std::make_shared<InnerHandle>(this, static_cast<uint32_t>(data.size()));
		handles.emplace_back(handle);
		data.push_back(std::forward<T>(val));
		return {handle};
	}

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

	const std::vector<T>& get_data()
	{
		return data;
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
