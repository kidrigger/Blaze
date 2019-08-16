
#pragma once

#include "Renderer.hpp"
#include "util/Managed.hpp"
#include "DataTypes.hpp"
#include "util/loaders.hpp"

#include <stdexcept>
#include <vector>

namespace blaze
{
	// Refactor into Buffer class

	template <typename T>
	class VertexBuffer
	{
	private:
		util::Managed<BufferObject> vertexBuffer;
		size_t size{ 0 };

	public:
		VertexBuffer() noexcept
		{
		}

		VertexBuffer(const Renderer& renderer, const std::vector<T>& data) noexcept
			:size(sizeof(data[0])* data.size())
		{
			using namespace util;
			VmaAllocator allocator = renderer.get_context().get_allocator();

			auto [stagingBuffer, stagingAlloc] = renderer.get_context().createBuffer(size, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY);

			auto stagingBufferRAII = Managed<BufferObject>({ stagingBuffer, stagingAlloc }, [allocator](BufferObject& bo) { vmaDestroyBuffer(allocator, bo.buffer, bo.allocation); });

			void* bufferdata;
			vmaMapMemory(allocator, stagingAlloc, &bufferdata);
			memcpy(bufferdata, data.data(), size);
			vmaUnmapMemory(allocator, stagingAlloc);

			auto [finalBuffer, finalAllocation] = renderer.get_context().createBuffer(size, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VMA_MEMORY_USAGE_GPU_ONLY);
			vertexBuffer = Managed<BufferObject>({ finalBuffer, finalAllocation }, [allocator](BufferObject& bo) { vmaDestroyBuffer(allocator, bo.buffer, bo.allocation); });

			try
			{
				VkCommandBuffer commandBuffer = renderer.startTransferCommands();

				VkBufferCopy copyRegion = {};
				copyRegion.srcOffset = 0;
				copyRegion.dstOffset = 0;
				copyRegion.size = size;
				vkCmdCopyBuffer(commandBuffer, stagingBuffer, finalBuffer, 1, &copyRegion);

				renderer.endTransferCommands(commandBuffer);
			}
			catch (std::exception& e)
			{
				std::cerr << e.what() << std::endl;
			}
		}

		VertexBuffer(VertexBuffer&& other) noexcept
			: buffer(other.buffer),
			allocation(other.allocation),
			size(other.size),
			allocator(other.allocator)
		{
		}

		VertexBuffer& operator=(VertexBuffer&& other) noexcept
		{
			if (this == &other)
			{
				return *this;
			}
			buffer = other.buffer;
			allocation = other.allocation;
			size = other.size;
			allocator = other.allocator;
			return *this;
		}

		VertexBuffer(const VertexBuffer& other) = delete;
		VertexBuffer& operator=(const VertexBuffer& other) = delete;

		~VertexBuffer()
		{
			vmaDestroyBuffer(allocator)
		}

		VkBuffer get_buffer() const { return vertexBuffer.get().buffer; }
		VmaAllocation get_memory() const { return vertexBuffer.get().allocation->GetMemory(); }
		size_t get_size() const { return size; }
	};

	template <typename T>
	class IndexedVertexBuffer
	{

	private:
		util::Managed<BufferObject> vertexBuffer;
		size_t vertexSize{ 0 };
		util::Managed<BufferObject> indexBuffer;
		size_t indexSize{ 0 };
		uint32_t indexCount{ 0 };

	public:
		IndexedVertexBuffer() noexcept
		{
		}

		IndexedVertexBuffer(const Renderer& renderer, const std::vector<T>& vertex_data, const std::vector<uint32_t>& index_data) noexcept
			:vertexSize(sizeof(vertex_data[0])* vertex_data.size()),
			indexSize(sizeof(uint32_t)* index_data.size()),
			indexCount(static_cast<uint32_t>(index_data.size()))
		{
			using namespace util;
			auto allocator = renderer.get_context().get_allocator();

			// Vertex Buffer
			auto [stagingVertexBuffer, stagingVertexAllocation] = renderer.get_context().createBuffer(vertexSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY);
			
			auto stagingVertexBufferRAII = Managed<BufferObject>({ stagingVertexBuffer, stagingVertexAllocation }, [allocator](BufferObject& bo) { vmaDestroyBuffer(allocator, bo.buffer, bo.allocation); });

			void* bufferdata;
			vmaMapMemory(allocator, stagingVertexAllocation, &bufferdata);
			memcpy(bufferdata, vertex_data.data(), vertexSize);
			vmaUnmapMemory(allocator, stagingVertexAllocation);

			auto [finalVertexBuffer, finalVertexAllocation] = renderer.get_context().createBuffer(vertexSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VMA_MEMORY_USAGE_GPU_ONLY);

			vertexBuffer = Managed<BufferObject>({ finalVertexBuffer, finalVertexAllocation }, [allocator](BufferObject& bo) { vmaDestroyBuffer(allocator, bo.buffer, bo.allocation); });

			// Index buffer

			auto [stagingIndexBuffer, stagingIndexMemory] = renderer.get_context().createBuffer(indexSize, VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY);
			
			auto stagingIndexBufferRAII = Managed<BufferObject>({ stagingIndexBuffer, stagingIndexMemory }, [allocator](BufferObject& bo) { vmaDestroyBuffer(allocator, bo.buffer, bo.allocation); });

			vmaMapMemory(allocator, stagingIndexMemory, &bufferdata);
			memcpy(bufferdata, index_data.data(), indexSize);
			vmaUnmapMemory(allocator, stagingIndexMemory);

			auto [finalIndexBuffer, finalIndexMemory] = renderer.get_context().createBuffer(indexSize, VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VMA_MEMORY_USAGE_GPU_ONLY);

			indexBuffer = Managed<BufferObject>({ finalIndexBuffer, finalIndexMemory }, [allocator](BufferObject& bo) { vmaDestroyBuffer(allocator, bo.buffer, bo.allocation); });

			try
			{
				VkCommandBuffer commandBuffer = renderer.startTransferCommands();

				VkBufferCopy copyRegion = {};
				copyRegion.srcOffset = 0;
				copyRegion.dstOffset = 0;

				copyRegion.size = vertexSize;
				vkCmdCopyBuffer(commandBuffer, stagingVertexBuffer, finalVertexBuffer, 1, &copyRegion);

				copyRegion.size = indexSize;
				vkCmdCopyBuffer(commandBuffer, stagingIndexBuffer, finalIndexBuffer, 1, &copyRegion);

				renderer.endTransferCommands(commandBuffer);
			}
			catch (std::exception& e)
			{
				std::cerr << e.what() << std::endl;
			}
		}

		IndexedVertexBuffer(IndexedVertexBuffer&& other) noexcept
			: vertexBuffer(std::move(other.vertexBuffer)),
			vertexMemory(std::move(other.vertexMemory)),
			vertexSize(other.vertexSize),
			indexBuffer(std::move(other.indexBuffer)),
			indexMemory(std::move(other.indexMemory)),
			indexSize(other.indexSize),
			indexCount(other.indexCount)
		{
		}

		IndexedVertexBuffer& operator=(IndexedVertexBuffer&& other) noexcept
		{
			if (this == &other)
			{
				return *this;
			}
			vertexBuffer = std::move(other.vertexBuffer);
			vertexSize = other.vertexSize;
			indexBuffer = std::move(other.indexBuffer);
			indexSize = other.indexSize;
			indexCount = other.indexCount;
			return *this;
		}

		IndexedVertexBuffer(const IndexedVertexBuffer& other) = delete;
		IndexedVertexBuffer& operator=(const IndexedVertexBuffer& other) = delete;

		VkBuffer get_vertexBuffer() const { return vertexBuffer.get().buffer; }
		VmaAllocation get_vertexAllocation() const { return vertexBuffer.get().allocation; }
		size_t get_verticeSize() const { return vertexSize; }
		VkBuffer get_indexBuffer() const { return indexBuffer.get().buffer; }
		VmaAllocation get_indexAllocation() const { return indexMemory.get().allocation; }
		size_t get_indiceSize() const { return indexSize; }
		uint32_t get_indexCount() const { return indexCount; }
	};

	class TextureImage
	{
	private:
		util::Managed<ImageObject> image;
		util::Managed<VkImageView> imageView;
		util::Managed<VkSampler> imageSampler;
		uint32_t width{ 0 };
		uint32_t height{ 0 };
	public:
		TextureImage() noexcept
		{
		}

		TextureImage(const Renderer& renderer, const ImageData& image_data)
			: width(image_data.width),
			height(image_data.height)
		{
			using namespace util;
			VmaAllocator allocator = renderer.get_context().get_allocator();

			auto [stagingBuffer, stagingAlloc] = renderer.get_context().createBuffer(image_data.size, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY);

			auto stagingBufferRAII = Managed<BufferObject>({ stagingBuffer, stagingAlloc }, [allocator](BufferObject& bo) { vmaDestroyBuffer(allocator, bo.buffer, bo.allocation); });

			void* bufferdata;
			vmaMapMemory(allocator, stagingAlloc, &bufferdata);
			memcpy(bufferdata, image_data.data, image_data.size);
			vmaUnmapMemory(allocator, stagingAlloc);

			auto [finalImage, finalAllocation] = renderer.get_context().createImage(width, height, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_SAMPLED_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VMA_MEMORY_USAGE_GPU_ONLY);
			image = Managed<ImageObject>({ finalImage, finalAllocation }, [allocator](ImageObject& bo) { vmaDestroyImage(allocator, bo.image, bo.allocation); });

			try
			{
				VkCommandBuffer commandBuffer = renderer.startTransferCommands();

				VkPipelineStageFlags srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
				VkPipelineStageFlags dstStage = VK_PIPELINE_STAGE_TRANSFER_BIT;

				VkImageMemoryBarrier barrier = {};
				barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
				barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
				barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
				barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
				barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

				barrier.image = image.get().image;
				barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
				barrier.subresourceRange.baseMipLevel = 0;
				barrier.subresourceRange.levelCount = 1;
				barrier.subresourceRange.baseArrayLayer = 0;
				barrier.subresourceRange.layerCount = 1;
				barrier.srcAccessMask = 0;
				barrier.dstAccessMask = 0;

				vkCmdPipelineBarrier(commandBuffer, srcStage, dstStage, 0, 0, nullptr, 0, nullptr, 1, &barrier);
				
				VkBufferImageCopy region = {};
				region.bufferOffset = 0;
				region.bufferRowLength = 0;
				region.bufferImageHeight = 0;
				region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
				region.imageSubresource.mipLevel = 0;
				region.imageSubresource.baseArrayLayer = 0;
				region.imageSubresource.layerCount = 1;

				region.imageOffset = { 0,0 };
				region.imageExtent = { width, height, 1 };

				vkCmdCopyBufferToImage(commandBuffer, stagingBuffer, finalImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

				barrier.oldLayout = barrier.newLayout;
				barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
				srcStage = dstStage;
				dstStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;

				vkCmdPipelineBarrier(commandBuffer, srcStage, dstStage, 0, 0, nullptr, 0, nullptr, 1, &barrier);

				renderer.endTransferCommands(commandBuffer);
			}
			catch (std::exception& e)
			{
				std::cerr << e.what() << std::endl;
			}

			imageView = util::Managed(util::createImageView(renderer.get_device(), get_image(), VK_FORMAT_R8G8B8A8_UNORM), [dev = renderer.get_device() ](VkImageView& iv) { vkDestroyImageView(dev, iv, nullptr); });
			imageSampler = util::Managed(createSampler(renderer.get_device()), [dev = renderer.get_device()](VkSampler& sampler) { vkDestroySampler(dev, sampler, nullptr); });
		}

		TextureImage(TextureImage&& other) noexcept
			: image(std::move(other.image)),
			imageView(std::move(other.imageView)),
			imageSampler(std::move(other.imageSampler)),
			width(other.width),
			height(other.height)
		{
		}

		TextureImage& operator=(TextureImage&& other) noexcept
		{
			if (this == &other)
			{
				return *this;
			}
			image = std::move(other.image);
			imageView = std::move(other.imageView);
			imageSampler = std::move(other.imageSampler);
			width = std::move(other.width);
			height = std::move(other.height);
			return *this;
		}

		TextureImage(const TextureImage& other) = delete;
		TextureImage& operator=(const TextureImage& other) = delete;

		VkImage get_image() const { return image.get().image; }
		VkImageView get_imageView() const { return imageView.get(); }
		VkSampler get_imageSampler() const { return imageSampler.get(); }

	private:
		VkSampler createSampler(VkDevice device) const
		{
			VkSamplerCreateInfo createInfo = {};
			createInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
			createInfo.magFilter = VK_FILTER_LINEAR;
			createInfo.minFilter = VK_FILTER_LINEAR;
			createInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
			createInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
			createInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
			createInfo.anisotropyEnable = VK_TRUE;
			createInfo.maxAnisotropy = 16;
			createInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
			createInfo.unnormalizedCoordinates = VK_FALSE;
			createInfo.compareEnable = VK_FALSE;
			createInfo.compareOp = VK_COMPARE_OP_ALWAYS;
			createInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
			createInfo.mipLodBias = 0.0f;
			createInfo.minLod = 0.0f;
			createInfo.maxLod = 0.0f;

			VkSampler sampler;
			auto result = vkCreateSampler(device, &createInfo, nullptr, &sampler);
			if (result != VK_SUCCESS)
			{
				throw std::runtime_error("Sampler creation failed with " + std::to_string(result));
			}

			return sampler;
		}
	};
}
