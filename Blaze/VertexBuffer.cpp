
#include "VertexBuffer.hpp"

#include <tuple>

namespace blaze
{
BaseVBO::BaseVBO(const Context* context, Usage usage, const void* data, uint32_t count, size_t size) noexcept
	: allocator(context->get_allocator()), count(count), size(size)
{
	using namespace util;

	auto [stagingBuf, stagingAlloc] =
		context->createBuffer(size, usage | VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY);

	void* bufferdata;
	vmaMapMemory(allocator, stagingAlloc, &bufferdata);
	memcpy(bufferdata, data, size);
	vmaUnmapMemory(allocator, stagingAlloc);

	std::tie(buffer, allocation) =
		context->createBuffer(size, usage | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VMA_MEMORY_USAGE_GPU_ONLY);

	try
	{
		VkCommandBuffer commandBuffer = context->startCommandBufferRecord();

		VkBufferCopy copyRegion = {};
		copyRegion.srcOffset = 0;
		copyRegion.dstOffset = 0;
		copyRegion.size = size;
		vkCmdCopyBuffer(commandBuffer, stagingBuf, buffer, 1, &copyRegion);

		context->flushCommandBuffer(commandBuffer);
	}
	catch (std::exception& e)
	{
		std::cerr << e.what() << std::endl;
	}
	vmaDestroyBuffer(allocator, stagingBuf, stagingAlloc);
}

BaseVBO::BaseVBO(BaseVBO&& other) noexcept : BaseVBO()
{
	std::swap(buffer, other.buffer);
	std::swap(allocation, other.allocation);
	std::swap(allocator, other.allocator);
	std::swap(count, other.count);
	std::swap(size, other.size);
}

BaseVBO& BaseVBO::operator=(BaseVBO&& other) noexcept
{
	if (this == &other)
	{
		return *this;
	}
	std::swap(buffer, other.buffer);
	std::swap(allocation, other.allocation);
	std::swap(allocator, other.allocator);
	std::swap(count, other.count);
	std::swap(size, other.size);
	return *this;
}

BaseVBO::~BaseVBO()
{
	if (buffer != VK_NULL_HANDLE)
	{
		vmaDestroyBuffer(allocator, buffer, allocation);
	}
}
} // namespace blaze