
#include "LightSystem.hpp"

namespace blaze
{
    LightSystem::LightSystem(const Context& context) noexcept
    {
        using namespace util;

        assert(MAX_POINT_LIGHTS <= 16);
        assert(MAX_DIR_LIGHTS <= 4);
        memset(&lightsData, 0, sizeof(lightsData));
        memset(lightsData.shadowIdx, -1, sizeof(lightsData.shadowIdx));

        pointShadowHandleValidity = std::vector<bool>(MAX_POINT_LIGHTS, false);
        pointShadowFreeStack.reserve(MAX_POINT_LIGHTS);
        for (uint32_t i = 0; i < MAX_POINT_LIGHTS; i++)
        {
            pointShadowFreeStack.push_back(MAX_POINT_LIGHTS - i - 1);
        }

        dirShadowHandleValidity = std::vector<bool>(MAX_DIR_LIGHTS, false);
        dirShadowFreeStack.reserve(MAX_DIR_LIGHTS);
        for (uint32_t i = 0; i < MAX_DIR_LIGHTS; i++)
        {
            dirShadowFreeStack.push_back(MAX_DIR_LIGHTS - i - 1);
        }

        renderPassOmni = Managed(createRenderPassMultiView(context.get_device(), 0b00111111, format, VK_FORMAT_D32_SFLOAT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL), [dev = context.get_device()](VkRenderPass& rp){ vkDestroyRenderPass(dev, rp, nullptr); });
        renderPassDirectional = Managed(createShadowRenderPass(context.get_device()), [dev = context.get_device()](VkRenderPass& rp){ vkDestroyRenderPass(dev, rp, nullptr); });

        viewsUBO = UniformBuffer(context, createOmniShadowUBO());
        csmUBO = UniformBuffer(context, CascadeUniformBufferObject{});

        try
        {
            {
                std::vector<VkDescriptorPoolSize> poolSizes = {
                    {
                        VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                        2 // Omni Views + Cascade Projections
                    },
                    {
                        VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                        MAX_POINT_LIGHTS + MAX_DIR_LIGHTS
                    }
                };
                dsPool = util::Managed(util::createDescriptorPool(context.get_device(), poolSizes, 17), [dev = context.get_device()](VkDescriptorPool& descPool){vkDestroyDescriptorPool(dev, descPool, nullptr); });
                std::vector<VkDescriptorSetLayoutBinding> bindings = {
                    {
                        0,
                        VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                        1,
                        VK_SHADER_STAGE_VERTEX_BIT,
                        nullptr
                    },
                    {
                        1,
                        VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                        1,
                        VK_SHADER_STAGE_VERTEX_BIT,
                        nullptr
                    }
                };

                dsLayout = util::Managed(util::createDescriptorSetLayout(context.get_device(), bindings), [dev = context.get_device()](VkDescriptorSetLayout& lay){vkDestroyDescriptorSetLayout(dev, lay, nullptr); });

                bindings = {
                    {
                        0,
                        VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                        MAX_POINT_LIGHTS,
                        VK_SHADER_STAGE_FRAGMENT_BIT,
                        nullptr
                    },
                    {
                        1,
                        VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                        MAX_DIR_LIGHTS,
                        VK_SHADER_STAGE_FRAGMENT_BIT,
                        nullptr
                    }
                };
                shadowLayout = util::Managed(util::createDescriptorSetLayout(context.get_device(), bindings), [dev = context.get_device()](VkDescriptorSetLayout& lay){vkDestroyDescriptorSetLayout(dev, lay, nullptr); });
            }

            {
                std::vector<VkDescriptorSetLayout> descriptorLayouts = {
                    dsLayout.get()
                };
                std::vector<VkPushConstantRange> pushConstantRanges = {
                    {
                        VK_SHADER_STAGE_VERTEX_BIT,
                        0,
                        sizeof(ModelPushConstantBlock) + sizeof(ShadowPushConstantBlock)
                    }
                };
                pipelineLayout = Managed(createPipelineLayout(context.get_device(), descriptorLayouts, pushConstantRanges), [dev = context.get_device()](VkPipelineLayout& lay){vkDestroyPipelineLayout(dev, lay, nullptr); });
            }

            pipelineOmni = Managed(
                createGraphicsPipeline(context.get_device(), pipelineLayout.get(), renderPassOmni.get(),
                    { POINT_SHADOW_MAP_SIZE, POINT_SHADOW_MAP_SIZE }, "shaders/vShadow.vert.spv", "shaders/fShadow.frag.spv",
                    { VK_DYNAMIC_STATE_VIEWPORT }, VK_CULL_MODE_BACK_BIT, VK_TRUE, VK_TRUE, VK_COMPARE_OP_LESS),
                [dev = context.get_device()](VkPipeline& pipe){ vkDestroyPipeline(dev, pipe, nullptr); });

            pipelineDirectional = Managed(
                createGraphicsPipeline(context.get_device(), pipelineLayout.get(), renderPassDirectional.get(),
                    { DIR_SHADOW_MAP_SIZE, DIR_SHADOW_MAP_SIZE }, "shaders/vDirShadow.vert.spv", "shaders/fDirShadow.frag.spv",
                    { VK_DYNAMIC_STATE_VIEWPORT,VK_DYNAMIC_STATE_DEPTH_BIAS }, VK_CULL_MODE_BACK_BIT, VK_TRUE, VK_TRUE, VK_COMPARE_OP_LESS),
                [dev = context.get_device()](VkPipeline& pipe){ vkDestroyPipeline(dev, pipe, nullptr); });

            {
                VkDescriptorSetAllocateInfo allocInfo = {};
                allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
                allocInfo.descriptorPool = dsPool.get();
                allocInfo.descriptorSetCount = 1;
                allocInfo.pSetLayouts = dsLayout.data();

                VkDescriptorSet dSet;
                auto result = vkAllocateDescriptorSets(context.get_device(), &allocInfo, &dSet);
                if (result != VK_SUCCESS)
                {
                    std::cerr << "Descriptor Set allocation failed with " << std::to_string(result) << std::endl;
                }
                uboDescriptorSet = dSet;

                {
                    VkDescriptorBufferInfo info = {};
                    info.buffer = viewsUBO.get_buffer();
                    info.offset = 0;
                    info.range = sizeof(ShadowUniformBufferObject);

                    VkWriteDescriptorSet write = {};
                    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                    write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
                    write.descriptorCount = 1;
                    write.dstSet = uboDescriptorSet.get();
                    write.dstBinding = 0;
                    write.dstArrayElement = 0;
                    write.pBufferInfo = &info;

                    vkUpdateDescriptorSets(context.get_device(), 1, &write, 0, nullptr);
                    viewsUBO.write(context, createOmniShadowUBO());
                }

                {
                    VkDescriptorBufferInfo info = {};
                    info.buffer = csmUBO.get_buffer();
                    info.offset = 0;
                    info.range = sizeof(CascadeUniformBufferObject);

                    VkWriteDescriptorSet write = {};
                    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                    write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
                    write.descriptorCount = 1;
                    write.dstSet = uboDescriptorSet.get();
                    write.dstBinding = 1;
                    write.dstArrayElement = 0;
                    write.pBufferInfo = &info;

                    vkUpdateDescriptorSets(context.get_device(), 1, &write, 0, nullptr);
                    // csmUBO.write(context, createOmniShadowUBO());
                }
            }

            for (uint32_t i = 0; i < MAX_POINT_LIGHTS; i++)
            {
                pointShadows.emplace_back(context, renderPassOmni.get());
            }
            for (uint32_t i = 0; i < MAX_DIR_LIGHTS; i++)
            {
                dirShadows.emplace_back(context, renderPassDirectional.get());
            }

            {
                VkDescriptorSetAllocateInfo allocInfo = {};
                allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
                allocInfo.descriptorPool = dsPool.get();
                allocInfo.descriptorSetCount = 1;
                allocInfo.pSetLayouts = &shadowLayout.get();

                VkDescriptorSet descriptorSet;
                auto result = vkAllocateDescriptorSets(context.get_device(), &allocInfo, &descriptorSet);
                if (result != VK_SUCCESS)
                {
                    throw std::runtime_error("Descriptor Set allocation failed with " + std::to_string(result));
                }
                shadowDescriptorSet = descriptorSet;

                std::vector<VkDescriptorImageInfo> pointImageInfos;
                for (auto& shade : pointShadows)
                {
                    pointImageInfos.push_back(shade.get_shadowMap().get_imageInfo());
                }

                std::array<VkWriteDescriptorSet, 2> writes;

                writes[0] = {};
                writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                writes[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
                writes[0].descriptorCount = MAX_POINT_LIGHTS;
                writes[0].dstSet = descriptorSet;
                writes[0].dstBinding = 0;
                writes[0].dstArrayElement = 0;
                writes[0].pImageInfo = pointImageInfos.data();

                std::vector<VkDescriptorImageInfo> dirImageInfos;
                for (auto& shade : dirShadows)
                {
                    dirImageInfos.push_back(shade.get_shadowMap().get_imageInfo());
                }

                writes[1] = {};
                writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                writes[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
                writes[1].descriptorCount = MAX_DIR_LIGHTS;
                writes[1].dstSet = descriptorSet;
                writes[1].dstBinding = 1;
                writes[1].dstArrayElement = 0;
                writes[1].pImageInfo = dirImageInfos.data();

                vkUpdateDescriptorSets(context.get_device(), static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
            }
        }
        catch (std::exception& e)
        {
            std::cerr << e.what() << std::endl;
        }
    }

    LightSystem::LightSystem(LightSystem&& other) noexcept
        : renderPassOmni(std::move(other.renderPassOmni)),
        renderPassDirectional(std::move(other.renderPassDirectional)),
        pipelineLayout(std::move(other.pipelineLayout)),
        pipelineOmni(std::move(other.pipelineOmni)),
        pipelineDirectional(std::move(other.pipelineDirectional)),
        dsPool(std::move(other.dsPool)),
        dsLayout(std::move(other.dsLayout)),
        uboDescriptorSet(std::move(other.uboDescriptorSet)),
        shadowDescriptorSet(std::move(other.shadowDescriptorSet)),
        shadowLayout(std::move(other.shadowLayout)),
        viewsUBO(std::move(other.viewsUBO)),
        pointShadows(std::move(other.pointShadows)),
        pointShadowFreeStack(std::move(other.pointShadowFreeStack)),
        pointShadowHandleValidity(std::move(other.pointShadowHandleValidity)),
        dirShadows(std::move(other.dirShadows)),
        dirShadowFreeStack(std::move(other.dirShadowFreeStack)),
        dirShadowHandleValidity(std::move(other.dirShadowHandleValidity))
    {
        memcpy(&lightsData, &other.lightsData, sizeof(LightsUniformBufferObject));
    }

    LightSystem& LightSystem::operator=(LightSystem&& other) noexcept
    {
        if (this == &other)
        {
            return *this;
        }
        renderPassOmni = std::move(other.renderPassOmni);
        renderPassDirectional = std::move(other.renderPassDirectional);
        pipelineLayout = std::move(other.pipelineLayout);
        pipelineOmni = std::move(other.pipelineOmni);
        pipelineDirectional = std::move(other.pipelineDirectional);
        dsPool = std::move(other.dsPool);
        dsLayout = std::move(other.dsLayout);
        uboDescriptorSet = std::move(other.uboDescriptorSet);
        shadowDescriptorSet = std::move(other.shadowDescriptorSet);
        shadowLayout = std::move(other.shadowLayout);
        viewsUBO = std::move(other.viewsUBO);
        pointShadows = std::move(other.pointShadows);
        pointShadowFreeStack = std::move(other.pointShadowFreeStack);
        pointShadowHandleValidity = std::move(other.pointShadowHandleValidity);
        dirShadows = std::move(other.dirShadows);
        dirShadowFreeStack = std::move(other.dirShadowFreeStack);
        dirShadowHandleValidity = std::move(other.dirShadowHandleValidity);
        memcpy(&lightsData, &other.lightsData, sizeof(LightsUniformBufferObject));
        return *this;
    }

    LightSystem::LightHandle LightSystem::addPointLight(const glm::vec3& position, float brightness, bool hasShadow)
    {
        if (lightsData.numPointLights >= (int)MAX_POINT_LIGHTS)
        {
            throw std::runtime_error("Max Light Count Reached.");
        }
        auto handle = lightsData.numPointLights;
        lightsData.lightPos[handle] = glm::vec4(position, brightness);
        if (hasShadow)
        {
            lightsData.shadowIdx[handle] = createPointShadow(position);
        }
        else
        {
            lightsData.shadowIdx[handle] = -1;
        }
        lightsData.numPointLights++;
        return handle | LIGHT_TYPE_POINT;
    }

    LightSystem::LightHandle LightSystem::addDirLight(const glm::vec3& direction, float brightness, bool hasShadow)
    {
        if (lightsData.numDirLights >= (int)MAX_DIR_LIGHTS)
        {
            throw std::runtime_error("Max Light Count Reached.");
        }
        auto handle = lightsData.numDirLights;
        lightsData.lightDir[handle] = glm::vec4(direction, brightness);
        if (hasShadow)
        {
            auto shade = createDirShadow(direction);
            assert(shade == handle);
        }
        else
        {
            lightsData.shadowIdx[handle] = -1;
        }
        lightsData.numDirLights++;
        return handle | LIGHT_TYPE_DIR;
    }

    void LightSystem::setLightPosition(LightHandle handle, const glm::vec3& position)
    {
        assert((handle & LIGHT_MASK_TYPE) == LIGHT_TYPE_POINT);
        int handleIdx = handle & LIGHT_MASK_INDEX;
        if (handleIdx >= lightsData.numPointLights)
        {
            throw std::out_of_range("Invalid Light Handle.");
        }
        memcpy(&lightsData.lightPos[handleIdx], &position[0], sizeof(glm::vec3));
        ShadowHandle shade = lightsData.shadowIdx[handleIdx];
        if (shade >= 0)
        {
            pointShadows[shade].position = lightsData.lightPos[handleIdx];
        }
    }

    void LightSystem::setLightDirection(LightHandle handle, const glm::vec3& direction)
    {
        assert((handle & LIGHT_MASK_TYPE) == LIGHT_TYPE_DIR);
        int handleIdx = handle & LIGHT_MASK_INDEX;
        if (handleIdx >= lightsData.numDirLights)
        {
            throw std::out_of_range("Invalid Light Handle.");
        }
        memcpy(&lightsData.lightDir[handleIdx], &direction[0], sizeof(glm::vec3));
        ShadowHandle shade = handleIdx;
        if (shade >= 0)
        {
            dirShadows[shade].direction = lightsData.lightDir[handleIdx];
        }
    }

    void LightSystem::setLightBrightness(LightHandle handle, float brightness)
    {
        int handleIdx = handle & LIGHT_MASK_INDEX;
        switch (handle & LIGHT_MASK_TYPE)
        {
        case LIGHT_TYPE_POINT:
        {
            if (handleIdx >= lightsData.numPointLights)
            {
                throw std::out_of_range("Invalid Light Handle.");
            }
            lightsData.lightPos[handleIdx][3] = brightness;
        };
        break;
        case LIGHT_TYPE_DIR:
        {
            if (handleIdx >= lightsData.numDirLights)
            {
                throw std::out_of_range("Invalid Light Handle.");
            }
            lightsData.lightDir[handleIdx][3] = brightness;
        };
        break;
        }
    }

    void LightSystem::cast(const Context& context, Camera* camera, VkCommandBuffer cmdBuffer, const std::vector<Drawable*>& drawables)
    {
        for (ShadowHandle handle : lightsData.shadowIdx)
        {
            if (handle < 0) continue;
            cast(context, handle, pointShadows[handle], cmdBuffer, drawables);
        }

        for (ShadowHandle i = 0; i < lightsData.numDirLights; i++)
        {
            cast(context, i, dirShadows[i], camera, cmdBuffer, drawables);
        }
    }
}

