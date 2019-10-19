
#include "TextureCube.hpp"

namespace blaze {

	[[nodiscard]] TextureCube loadImageCube(const Context& context, const std::vector<std::string>& names_fbudrl)
	{
		ImageDataCube image;
		int width, height, numChannels;

		int layer = 0;
		for (const auto& name : names_fbudrl)
		{
			image.data[layer] = stbi_load(name.c_str(), &width, &height, &numChannels, STBI_rgb_alpha);

			if (!image.data[layer])
			{
				throw std::runtime_error("Image " + name + " could not be loaded.");
			}

			layer++;
		}
		image.width = width;
		image.height = height;
		image.numChannels = numChannels;
		image.layerSize = 4 * width * height;
		image.size = 6 * image.layerSize;

		auto&& ti = TextureCube(context, image);

		for (int i = 0; i < 6; i++)
		{
			stbi_image_free(image.data[i]);
		}

		return std::move(ti);
	}
}