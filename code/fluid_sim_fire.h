#pragma once

struct fire_inputs
{
    v2 MousePos;
    v2 DeltaMousePos;
    
    f32 FrameTime;
    f32 Density;
    f32 Epsilon;
    u32 Dim;

    f32 Gravity;
    f32 RoomTemperature;
    f32 MolarMass;
    f32 R;
    f32 Buoyancy;
    
    f32 SplatRadius;
    v2 SplatCenter;
};

struct fire_sim
{
    u32 InputId;
    u32 PressureInputId;
    u32 Width;
    u32 Height;
    
    vk_image ColorImages[2];
    vk_image TemperatureImages[2];
    vk_image TimerImages[2];
    vk_image VelocityImages[2];
    vk_image DivergenceImage;
    vk_image PressureImages[2];

    VkDescriptorSetLayout GlobalDescLayout;
    VkDescriptorSet GlobalDescriptor;
    fire_inputs Inputs;
    VkBuffer UniformBuffer;

    VkDescriptorSet SplatDescriptors[2];
    VkDescriptorSet AdvectionDescriptors[2];
    VkDescriptorSet DivergenceDescriptors[2];
    VkDescriptorSet PressureDescriptors[2];
    VkDescriptorSet PressureApplyDescriptors[2];
    VkDescriptorSet RenderDescriptors[2];

    VkDescriptorSetLayout SplatDescLayout;
    vk_pipeline* SplatPipeline;

    VkDescriptorSetLayout AdvectionDescLayout;
    vk_pipeline* AdvectionPipeline;

    VkDescriptorSetLayout DivergenceDescLayout;
    vk_pipeline* DivergencePipeline;

    VkDescriptorSetLayout PressureDescLayout;
    vk_pipeline* PressureIterationPipeline;

    VkDescriptorSetLayout PressureApplyDescLayout;
    vk_pipeline* PressureApplyPipeline;
};

inline void FireSetInputs(fire_sim* Fire, vk_commands* Commands, f32 FrameTime, v2 MousePos, v2 PrevMousePos, b32 MouseDown);
