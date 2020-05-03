
#include "UniformBuffer.hpp"

namespace blaze
{
BaseUBO::BaseUBO(const Context* context, size_t size) noexcept : size(size)
{
	allocator = context->get_allocator();

	std::tie(buffer, allocation) =
		context->createBuffer(size, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_ONLY);
};

BaseUBO::BaseUBO(BaseUBO&& other) noexcept : BaseUBO()
{
	std::swap(buffer, other.buffer);
	std::swap(allocation, other.allocation);
	std::swap(allocator, other.allocator);
	std::swap(size, other.size);
}

BaseUBO& BaseUBO::operator=(BaseUBO&& other) noexcept
{
	if (this == &other)
	{
		return *this;
	}
	std::swap(buffer, other.buffer);
	std::swap(allocation, other.allocation);
	std::swap(allocator, other.allocator);
	std::swap(size, other.size);
	return *this;
}

void BaseUBO::writeData(const void* data, size_t size)
{
	assert(size == this->size);
	void* dataPtr;
	vmaMapMemory(allocator, allocation, &dataPtr);
	memcpy(dataPtr, data, size);
	vmaUnmapMemory(allocator, allocation);
}

BaseUBO::~BaseUBO()
{
	if (buffer != VK_NULL_HANDLE)
	{
		vmaDestroyBuffer(allocator, buffer, allocation);
	}
}
} // namespace blaze
