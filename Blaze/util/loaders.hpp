
#pragma once

#include <stb_image.h>

struct ImageData
{
	uint8_t* data;
	uint32_t width;
	uint32_t height;
	uint32_t numChannels;
	uint32_t size;
};

ImageData loadImage(const std::string& name)
{
	ImageData image;
	int width, height, numChannels;
	image.data = stbi_load(name.c_str(), &width, &height, &numChannels, STBI_rgb_alpha);
	image.width = width;
	image.height = height;
	image.numChannels = numChannels;
	image.size = 4 * width * height;

	if (!image.data)
	{
		throw std::runtime_error("Image " + name + " could not be loaded.");
	}

	return image;
}

void unloadImage(ImageData& data)
{
	stbi_image_free(data.data);
	data.data = nullptr;
	data.height = 0;
	data.width = 0;
	data.numChannels = 0;
	data.size = 0;
}
