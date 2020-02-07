
#pragma once

#include <functional>

namespace blaze::util
{

struct Hash
{
	size_t value;

	bool operator==(const Hash& other) const
	{
		return value == other.value;
	}
};

template <typename T>
inline Hash hash(const T& hashable)
{
	std::hash<T> hasher;
	return Hash{hasher(hashable)};
}

inline Hash operator+(const Hash& a, const Hash& b)
{
	size_t seed = a.value;
	size_t hash = b.value;

	seed ^= hash + 0x9e3779b9 + (seed << 6) + (seed >> 2);

	return Hash{seed};
}

inline void operator+=(Hash& a, const Hash& b)
{
	a.value ^= b.value + 0x9e3779b9 + (a.value << 6) + (a.value >> 2);
}

} // namespace blaze::util
