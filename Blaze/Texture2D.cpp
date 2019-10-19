
#include "Texture2D.hpp"

namespace blaze {

	[[nodiscard]] Texture2D loadImage(const Context& context, const std::string& name)
	{
		ImageData2D image;
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

		auto&& ti = Texture2D(context, image);
		stbi_image_free(image.data);

		return std::move(ti);
	}
}