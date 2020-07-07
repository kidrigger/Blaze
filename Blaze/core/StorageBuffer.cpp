
#include "StorageBuffer.hpp"
#include <tuple>

namespace blaze
{
SSBO::SSBO(const Context* context, size_t size) noexcept : size(size)
{
	buffer = context->createBuffer(size, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_ONLY);
};

SSBO::SSBO(SSBO&& other) noexcept : SSBO()
{
	std::swap(buffer, other.buffer);
	std::swap(size, other.size);
}

SSBO& SSBO::operator=(SSBO&& other) noexcept
{
	if (this == &other)
	{
		return *this;
	}
	std::swap(buffer, other.buffer);
	std::swap(size, other.size);
	return *this;
}

void SSBO::writeData(const void* data, size_t size)
{
	assert(size == this->size);
	void* dataPtr;
	vmaMapMemory(buffer.allocator, buffer.allocation, &dataPtr);
	memcpy(dataPtr, data, size);
	vmaUnmapMemory(buffer.allocator, buffer.allocation);
}

void SSBO::writeData(const void* data, size_t offset, size_t size)
{
	assert(size > 0);
	assert(offset >= 0);
	assert(offset + size <= this->size);
	byte* dataPtr;
	vmaMapMemory(buffer.allocator, buffer.allocation, (void**)&dataPtr);
	memcpy(dataPtr + offset, data, size);
	vmaUnmapMemory(buffer.allocator, buffer.allocation);
}
} // namespace blaze
