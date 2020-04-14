
#pragma once

#include <gui/imgui.h>
#include <gui/imgui_impl_glfw.h>
#include <gui/imgui_impl_vulkan.h>

#include <core/Context.hpp>
#include <util/Managed.hpp>
#include <util/createFunctions.hpp>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <vector>

#undef max
#undef min

#include <algorithm>
#include <vkwrap/VkWrap.hpp>

namespace blaze
{
/**
 * @class GUI
 *
 * @brief Initializes and simplified ImGUI usage.
 *
 * A GUI object must be instantiated (preferably by the renderer) to use ImGUI.
 * It initializes, and contains all the required buffers, renderpasses and pipelines
 * for ImGUI rendering.
 *
 * Certain aspects of the GUI rendering are exposed as static functions to allow use
 * from anywhere.
 *
 * @note Due to certain constraints, swapchain image views need to be passed to the GUI.
 */
class GUI
{
private:
	static void VK_ASSERT(VkResult res)
	{
		assert(res == VK_SUCCESS);
	}

	uint32_t width{0};
	uint32_t height{0};
	vkw::DescriptorPool descriptorPool;
	vkw::RenderPass renderPass;
	vkw::FramebufferVector framebuffers;
	bool valid;

	inline static bool complete = false;

public:
	/**
	 * @fn GUI()
	 *
	 * @brief Default constructor.
	 */
	GUI() noexcept : valid(false)
	{
	}

	/**
	 * @fn GUI(const Context& context, const VkExtent2D& size, const VkFormat& format, const std::vector<VkImageView>&
	 * swapchainImageViews)
	 *
	 * @brief Constructor of the object. Initializes ImGUI and required resources.
	 */
	GUI(const Context* context, const VkExtent2D& size, const VkFormat& format,
		const std::vector<VkImageView>& swapchainImageViews) noexcept;

	/**
	 * @name Move Constructors
	 *
	 * @brief Moves all members to the new construction.
	 *
	 * GUI is a move only class and must be kept unique.
	 * The copy constructors are deleted
	 *
	 * @{
	 */
	GUI(GUI&& other) noexcept;
	GUI& operator=(GUI&& other) noexcept;
	GUI(const GUI& other) = delete;
	GUI& operator=(const GUI& other) = delete;
	/**
	 * @}
	 */

	/**
	 * @fn recreate
	 *
	 * @brief Recreates the GUI framebuffers.
	 *
	 * @param context The current Vulkan context.
	 * @param size The size of the framebuffer.
	 * @param swapchainImageViews The imageviews of the current swapchain.
	 */
	void recreate(const Context* context, const VkExtent2D& size, const std::vector<VkImageView>& swapchainImageViews);

	/**
	 * @fn startFrame
	 *
	 * @brief Starts a new ImGUI frame.
	 */
	static void startFrame();

	/**
	 * @fn endFrame
	 *
	 * @brief Ends the ImGUI frame.
	 */
	static void endFrame();

	/**
	 * @fn draw
	 *
	 * @brief Renders the ImGUI GUI on top of the existing image.
	 *
	 * This must be the last step of the render drawcalls, and hence the last renderpass.
	 * The ImGUI UI will overlay.
	 *
	 * @param cmdBuffer The command buffer to record commands into.
	 * @param frameCount The index of the swapchain image to render on.
	 */
	void draw(VkCommandBuffer cmdBuffer, int frameCount);

	/**
	 * @fn ~GUI
	 *
	 * @brief Deinitializes the GUI components
	 */
	~GUI();

private:
	vkw::FramebufferVector createSwapchainFramebuffers(VkDevice device, const std::vector<VkImageView>& swapchainImageViews) const;
};
} // namespace blaze
