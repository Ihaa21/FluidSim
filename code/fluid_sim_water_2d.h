#pragma once

struct water_2d_inputs
{
    
};

struct water_2d_sim
{
    vk_image ColorImages[2];
    vk_image DistanceImages[2];
    vk_image VelocityImages[2];
    vk_image DivergenceImage;
    vk_image PressureImages[2];

    water_2d_inputs WaterInputs;
    VkBuffer UniformBuffer;

    VkDescriptorSet InitDescriptors[2];
    VkDescriptorSet AdvectionDescriptors[2];
    VkDescriptorSet DivergenceDescriptors[2];
    VkDescriptorSet PressureDescriptors[2];
    VkDescriptorSet PressureApplyDescriptors[2];
    VkDescriptorSet RenderDescriptors[2];

    VkDescriptorSetLayout WaterSplatDescLayout;
    vk_pipeline* WaterSplatPipeline;

    VkDescriptorSetLayout WaterAdvectionDescLayout;
    vk_pipeline* WaterAdvectionPipeline;

    VkDescriptorSetLayout DivergenceDescLayout;
    vk_pipeline* DivergencePipeline;

    VkDescriptorSetLayout PressureDescLayout;
    vk_pipeline* PressureMirrorIterationPipeline;
    vk_pipeline* PressureClampIterationPipeline;

    VkDescriptorSetLayout WaterPressureApplyDescLayout;
    vk_pipeline* WaterPressureApplyPipeline;
};

