
#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <thirdparty/vma/vk_mem_alloc.h>
#include <util/createFunctions.hpp>
#include <util/debugMessenger.hpp>

#include "VkWrapBase.hpp"

#include "VkWrapSpecialized.hpp"

namespace blaze::vkw {

#define GEN_UNMANAGED_HOLDER(Type) using Type = base::BaseWrapper<Vk##Type>

GEN_UNMANAGED_HOLDER(PhysicalDevice);
GEN_UNMANAGED_HOLDER(Queue);
GEN_UNMANAGED_HOLDER(DescriptorSet);

#define GEN_UNMANAGED_COLLECTION(Type) using Type##Vector = base::BaseCollection<Vk##Type>

GEN_UNMANAGED_COLLECTION(DescriptorSet);

#define GEN_INDEPENDENT_HOLDER(Type) using Type = base::IndependentHolder<Vk##Type, vkDestroy##Type>

GEN_INDEPENDENT_HOLDER(Instance);
GEN_INDEPENDENT_HOLDER(Device);

#define GEN_DEPENDENT_HOLDER(Type, TDep, TDestroy) using Type = base::DependentHolder<Vk##Type, TDep, TDestroy>

GEN_DEPENDENT_HOLDER(DebugUtilsMessengerEXT, VkInstance, util::destroyDebugUtilsMessengerEXT);

#define GEN_INSTANCE_DEPENDENT_HOLDER(Type) using Type = base::DependentHolder<Vk##Type, VkInstance, vkDestroy##Type>

GEN_INSTANCE_DEPENDENT_HOLDER(SurfaceKHR);

#define GEN_DEVICE_DEPENDENT_HOLDER(Type) using Type = base::DependentHolder<Vk##Type, VkDevice, vkDestroy##Type>

GEN_DEVICE_DEPENDENT_HOLDER(RenderPass);
GEN_DEVICE_DEPENDENT_HOLDER(DescriptorSetLayout);
GEN_DEVICE_DEPENDENT_HOLDER(DescriptorPool);
GEN_DEVICE_DEPENDENT_HOLDER(PipelineLayout);
GEN_DEVICE_DEPENDENT_HOLDER(Pipeline);
GEN_DEVICE_DEPENDENT_HOLDER(SwapchainKHR);
GEN_DEVICE_DEPENDENT_HOLDER(CommandPool);
GEN_DEVICE_DEPENDENT_HOLDER(ShaderModule);
GEN_DEVICE_DEPENDENT_HOLDER(Framebuffer);
GEN_DEVICE_DEPENDENT_HOLDER(ImageView);
GEN_DEVICE_DEPENDENT_HOLDER(Sampler);

#define GEN_DEVICE_DEPENDENT_COLLECTION(Type) using Type##Vector = base::DeviceDependentVector<Vk##Type, vkDestroy##Type>

GEN_DEVICE_DEPENDENT_COLLECTION(Fence);
GEN_DEVICE_DEPENDENT_COLLECTION(Semaphore);
GEN_DEVICE_DEPENDENT_COLLECTION(Framebuffer);
GEN_DEVICE_DEPENDENT_COLLECTION(ImageView);

} // namespace blaze::vkw
