
#include "UniformBuffer.hpp"
#include <tuple>

namespace blaze
{
BaseUBO::BaseUBO(const Context* context, size_t size) noexcept : size(size)
{
	buffer = context->createBuffer(size, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_ONLY);
};

BaseUBO::BaseUBO(BaseUBO&& other) noexcept : BaseUBO()
{
	std::swap(buffer, other.buffer);
	std::swap(size, other.size);
}

BaseUBO& BaseUBO::operator=(BaseUBO&& other) noexcept
{
	if (this == &other)
	{
		return *this;
	}
	std::swap(buffer, other.buffer);
	std::swap(size, other.size);
	return *this;
}

void BaseUBO::writeData(const void* data, size_t size)
{
	assert(size == this->size);
	void* dataPtr;
	vmaMapMemory(buffer.allocator, buffer.allocation, &dataPtr);
	memcpy(dataPtr, data, size);
	vmaUnmapMemory(buffer.allocator, buffer.allocation);
}
} // namespace blaze
