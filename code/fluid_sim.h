#pragma once

struct fluid_sim_inputs
{
    f32 FrameTime;
    f32 Density;
    f32 Epsilon;
    u32 Dim;
};

struct diffusion_sim
{
    
};

struct smoke_sim
{
};

struct fire_sim
{
};

struct water_sim
{
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
    
    VkDescriptorSetLayout GlobalDescLayout;
    VkDescriptorSet GlobalDescriptor;

    VkDescriptorSet RenderDescriptors[2];
    
    VkDescriptorSetLayout InitDescLayout;
    VkDescriptorSet InitDescriptor;
    vk_pipeline* InitPipeline;
    
    VkDescriptorSetLayout AdvectionDescLayout;
    VkDescriptorSet AdvectionDescriptors[2];
    vk_pipeline* AdvectionVelPipeline;
    
    VkDescriptorSetLayout DivergenceDescLayout;
    VkDescriptorSet DivergenceDescriptors[2];
    vk_pipeline* DivergencePipeline;

    VkDescriptorSetLayout PressureDescLayout;
    VkDescriptorSet PressureDescriptors[2];
    vk_pipeline* PressureIterationPipeline;

    VkDescriptorSetLayout PressureApplyDescLayout;
    VkDescriptorSet PressureApplyDescriptors[2];
    vk_pipeline* PressureApplyPipeline;
    
    vk_pipeline* CopyToRtPipeline;
};
