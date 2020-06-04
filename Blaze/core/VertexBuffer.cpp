
#include "VertexBuffer.hpp"

#include <tuple>

namespace blaze
{
BaseVBO::BaseVBO(const Context* context, Usage usage, const void* data, uint32_t count, size_t size) noexcept
	: count(count), size(size)
{
	using namespace util;

	auto stagingBuffer = context->createBuffer(size, usage | VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY);

	void* bufferdata;
	vmaMapMemory(stagingBuffer.allocator, stagingBuffer.allocation, &bufferdata);
	memcpy(bufferdata, data, size);
	vmaUnmapMemory(stagingBuffer.allocator, stagingBuffer.allocation);

	buffer = context->createBuffer(size, usage | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VMA_MEMORY_USAGE_GPU_ONLY);

	try
	{
		VkCommandBuffer commandBuffer = context->startCommandBufferRecord();

		VkBufferCopy copyRegion = {};
		copyRegion.srcOffset = 0;
		copyRegion.dstOffset = 0;
		copyRegion.size = size;
		vkCmdCopyBuffer(commandBuffer, stagingBuffer.handle, buffer.handle, 1, &copyRegion);

		context->flushCommandBuffer(commandBuffer);
	}
	catch (std::exception& e)
	{
		std::cerr << e.what() << std::endl;
	}
}

BaseVBO::BaseVBO(BaseVBO&& other) noexcept : BaseVBO()
{
	std::swap(buffer, other.buffer);
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
	std::swap(count, other.count);
	std::swap(size, other.size);
	return *this;
}
} // namespace blaze