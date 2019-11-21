
#pragma once

namespace blaze
{
	class Drawable
	{
	public:
		virtual void draw(VkCommandBuffer cb, VkPipelineLayout lay) = 0;
		virtual void drawGeometry(VkCommandBuffer cb, VkPipelineLayout lay) = 0;
	};
}
