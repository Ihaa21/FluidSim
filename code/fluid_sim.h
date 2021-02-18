#pragma once

struct fluid_sim_inputs
{
    f32 FrameTime;
    f32 Density;
    f32 Epsilon;
    u32 Dim;
};

struct fluid_sim
{
    vk_linear_arena Arena;

    u32 Width;
    u32 Height;
    u32 InputId;

    fluid_sim_inputs UniformsCpu;
    VkBuffer UniformBuffer;
    VkSampler PointSampler;
    VkSampler LinearSampler;
    vk_image ColorImages[2];
    vk_image VelocityImages[2];
    vk_image DivergenceImage;
    vk_image PressureImages[2];
    
    VkDescriptorSetLayout DescLayout;
    VkDescriptorSet Descriptors[2];
    VkDescriptorSetLayout PressureDescLayout;
    VkDescriptorSet PressureDescriptors[2];
    VkDescriptorSet RenderDescriptors[2];

    vk_pipeline* InitPipeline;
    vk_pipeline* AdvectionVelPipeline;
    vk_pipeline* DivergencePipeline;
    vk_pipeline* TestDivergencePipeline; // TODO: REMOVE
    vk_pipeline* PressureIterationPipeline;
    vk_pipeline* PressureApplyPipeline;
    vk_pipeline* CopyToRtPipeline;
};
