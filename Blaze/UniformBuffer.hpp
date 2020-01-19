
#pragma once

#include <cstring>
#include "Context.hpp"

namespace blaze
{
    /**
     * @class UniformBuffer
     *
     * @tparam T The data object to be stored in the UniformBuffer.
     *
     * @brief The class to handle operations on a uniform buffer.
     */
	template <typename T>
	class UniformBuffer
	{
	private:
		util::Managed<BufferObject> buffer;
		size_t size{ 0 };

	public:
        /**
         * @fn UniformBuffer()
         *
         * @brief Default Constructor.
         */
		UniformBuffer() noexcept {}

        /**
         * @fn UniformBuffer(const Context& context, const T& data)
         *
         * @brief Main constructor of the class.
         *
         * @param context The current Vulkan Context.
         * @param data The data object of type T to store.
         */
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

        /**
         * @name Move Constructors.
         *
         * @brief Move only, copy deleted.
         *
         * @{
         */
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
			return *this;
		}

		UniformBuffer(const UniformBuffer& other) = delete;
		UniformBuffer& operator=(const UniformBuffer& other) = delete;
        /**
         * @}
         */

        /**
         * @name Getters.
         *
         * @brief Getters for private variables.
         *
         * @{
         */
		VkBuffer get_buffer() const { return buffer.get().buffer; }
		VmaAllocation get_allocation() const { return buffer.get().allocation; }
		size_t get_size() const { return size; }
        /**
         * @}
         */

        /**
         * @fn write(const Context& context, const T& data)
         *
         * @brief Writes the new data to the uniform buffer.
         *
         * @param context The Vulkan Context in use.
         * @param data The data to write to the uniform buffer.
         */
		void write(const Context& context, const T& data)
		{
			void* dataPtr;
			vmaMapMemory(context.get_allocator(), buffer.get().allocation, &dataPtr);
			memcpy(dataPtr, &data, size);
			vmaUnmapMemory(context.get_allocator(), buffer.get().allocation);
		}
	};
}
