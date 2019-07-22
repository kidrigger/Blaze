
#pragma once

#include <glm/glm.hpp>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <array>

namespace blaze
{
	struct Vertex
	{
		glm::vec3 position;
		glm::vec3 color;

		static VkVertexInputBindingDescription getBindingDescription()
		{
			VkVertexInputBindingDescription bindingDesc = {};
			bindingDesc.binding = 0;
			bindingDesc.stride = sizeof(Vertex);
			bindingDesc.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

			return bindingDesc;
		}

		static std::array<VkVertexInputAttributeDescription, 2> getAttributeDescriptions()
		{
			std::array<VkVertexInputAttributeDescription, 2> attributeDesc = {};

			attributeDesc[0].binding = 0;
			attributeDesc[0].location = 0;
			attributeDesc[0].format = VK_FORMAT_R32G32B32_SFLOAT;
			attributeDesc[0].offset = offsetof(Vertex, position);

			attributeDesc[1].binding = 0;
			attributeDesc[1].location = 1;
			attributeDesc[1].format = VK_FORMAT_R32G32B32_SFLOAT;
			attributeDesc[1].offset = offsetof(Vertex, color);

			return attributeDesc;
		}
	};

	class VertexBuffer
	{
	private:
		util::Managed<VkDeviceMemory> memory;
		util::Managed<VkBuffer> buffer;
		size_t size;

	public:
		VertexBuffer() noexcept
			: size(0)
		{
		}

		VertexBuffer(const Renderer& renderer, size_t size) noexcept
			:size(size)
		{
			auto vbo_deleter = [dev = renderer.get_device()](VkBuffer& buf) { vkDestroyBuffer(dev, buf, nullptr); };
			auto vbo_memory_freer = [dev = renderer.get_device()](VkDeviceMemory& devmem) { vkFreeMemory(dev, devmem, nullptr); };

			auto [vbo, mem] = util::createBuffer(renderer.get_device(), renderer.get_physicalDevice(), size, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
			buffer = util::Managed(vbo, vbo_deleter);
			memory = util::Managed(mem, vbo_memory_freer);
		}

		VertexBuffer(VertexBuffer&& other) noexcept
			: buffer(std::move(other.buffer)),
			memory(std::move(other.memory)),
			size(other.size)
		{
		}

		VertexBuffer& operator=(VertexBuffer&& other) noexcept
		{
			if (this == &other)
			{
				return *this;
			}
			buffer = std::move(other.buffer);
			memory = std::move(other.memory);
			size = other.size;
			return *this;
		}

		VertexBuffer(const VertexBuffer& other) = delete;
		VertexBuffer& operator=(const VertexBuffer& other) = delete;

		VkBuffer get_buffer() const { return buffer.get(); }
		VkDeviceMemory get_memory() const { return memory.get(); }
		size_t get_size() const { return size; }

		template <typename T>
		void map(VkDevice device, const std::vector<T>& data)
		{
			if (data.size() * sizeof(data[0]) != size)
			{
				throw std::runtime_error("vertex buffer memory size mismatch");
			}

			void* bufferdata;
			vkMapMemory(device, memory.get(), 0, size, 0, &bufferdata);
			memcpy(bufferdata, data.data(), size);
			vkUnmapMemory(device, memory.get());
		}
	};
}
