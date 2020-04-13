
#include "FwdRenderer.hpp"

namespace blaze
{
FwdRenderer::FwdRenderer(GLFWwindow* window, bool enableValidationLayers) noexcept
	: ARenderer(window, enableValidationLayers)
{

	isComplete = true;
}

void FwdRenderer::renderFrame()
{
}

void FwdRenderer::rebuildCommandBuffer(VkCommandBuffer cmd)
{
}

void FwdRenderer::recreateSwapchainDependents()
{

}
} // namespace blaze