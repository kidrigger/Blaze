
#pragma once

namespace blaze
{
	template <typename T>
	class UniformBuffer
	{
	private:
		util::Managed<BufferObject> buffer;
		size_t size{ 0 };

	public:
		UniformBuffer() noexcept
		{
		}

		UniformBuffer(const Context& context, const T& data)
			: size(sizeof(data))
		{
			VmaAllocator allocator = context.get_allocator();

			auto [buf, alloc] = context.createBuffer(size, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_ONLY);

			void* memdata;
			vmaMapMemory(allocator, alloc, &memdata);
			memcpy(memdata, &data, size);
			vmaUnmapMemory(allocator, alloc);

			buffer = util::Managed<BufferObject>({ buf, alloc }, [allocator](BufferObject& buf) { vmaDestroyBuffer(allocator, buf.buffer, buf.allocation); });
		}

		UniformBuffer(UniformBuffer&& other) noexcept
			: buffer(std::move(other.buffer)),
			size(other.size)
		{
		}

		UniformBuffer& operator=(UniformBuffer&& other) noexcept
		{
			if (this == &other)
			{
				return *this;
			}
			buffer = std::move(other.buffer);
			size = other.size;
		}

		UniformBuffer(const UniformBuffer& other) = delete;
		UniformBuffer& operator=(const UniformBuffer& other) = delete;

		VkBuffer get_buffer() const { return buffer.get().buffer; }
		VmaAllocation get_allocation() const { return buffer.get().allocation; }
		size_t get_size() const { return size; }
	};
}
