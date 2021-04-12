#pragma once

struct fluid_sim_inputs
{
    v2 MousePos;
    v2 DeltaMousePos;
    
    f32 FrameTime;
    f32 Density;
    f32 Epsilon;
    u32 Dim;

    // NOTE: Smoke Globals
    f32 Gravity;
    f32 RoomTemperature;
    f32 MolarMass;
    f32 R;
    
    // NOTE: Splat data
    v2 SplatCenter;
    f32 SplatRadius;
    u32 Pad0;
};

struct diffusion_sim
{
    vk_image ColorImages[2];

    VkDescriptorSet InitDescriptor;
    VkDescriptorSet AdvectionDescriptors[2];
    VkDescriptorSet PressureApplyDescriptors[2];
    VkDescriptorSet RenderDescriptors[2];
};

struct smoke_sim
{
    vk_image ColorImages[2];
    vk_image TemperatureImages[2];

    VkDescriptorSet SplatDescriptors[2];
    VkDescriptorSet AdvectionDescriptors[2];
    VkDescriptorSet PressureApplyDescriptors[2];
    VkDescriptorSet RenderDescriptors[2];
};

struct fire_sim
{
    vk_image ColorImages[2];
    vk_image TemperatureImages[2];
    vk_image TimerImages[2];

    VkDescriptorSet SplatDescriptors[2];
    VkDescriptorSet FuelDescriptors[2];
    VkDescriptorSet AdvectionDescriptors[2];
    VkDescriptorSet PressureApplyDescriptors[2];
    VkDescriptorSet RenderDescriptors[2];
};

struct water_sim
{
    vk_image ColorImages[2];
    vk_image DistanceImages[2];

    VkDescriptorSet InitDescriptors[2];
    VkDescriptorSet AdvectionDescriptors[2];
    VkDescriptorSet PressureApplyDescriptors[2];
    VkDescriptorSet RenderDescriptors[2];
};

enum fluid_sim_type
{
    FluidSimType_None,

    FluidSimType_Diffusion,
    FluidSimType_Smoke,
    FluidSimType_Fire,
    FluidSimType_Water,
};

struct fluid_sim
{
    vk_linear_arena Arena;

    u32 Width;
    u32 Height;
    u32 InputId;
    u32 PressureInputId;

    vk_image VelocityImages[2];
    vk_image DivergenceImage;
    vk_image PressureImages[2];
    
    VkDescriptorSet DivergenceDescriptors[2];
    VkDescriptorSet PressureDescriptors[2];

    fluid_sim_type Type;
    union
    {
        diffusion_sim DiffusionSim;
        smoke_sim SmokeSim;
        fire_sim FireSim;
        water_sim WaterSim;
    };
    
    fluid_sim_inputs UniformsCpu;
    VkBuffer UniformBuffer;
    VkSampler MirrorPointSampler;
    VkSampler MirrorLinearSampler;
    
    VkSampler ClampPointSampler;
    VkSampler ClampLinearSampler;
    
    VkDescriptorSetLayout GlobalDescLayout;
    VkDescriptorSet GlobalDescriptor;

    // NOTE: Shared PSOs
    VkDescriptorSetLayout DivergenceDescLayout;
    vk_pipeline* DivergencePipeline;

    VkDescriptorSetLayout PressureDescLayout;
    vk_pipeline* PressureMirrorIterationPipeline;
    vk_pipeline* PressureClampIterationPipeline;

    // NOTE: Diffusion specific data
    VkDescriptorSetLayout DiffusionInitDescLayout;
    vk_pipeline* DiffusionInitPipeline;

    VkDescriptorSetLayout DiffusionAdvectionDescLayout;
    vk_pipeline* DiffusionAdvectionPipeline;

    VkDescriptorSetLayout DiffusionPressureApplyDescLayout;
    vk_pipeline* DiffusionPressureApplyPipeline;

    // NOTE: Smoke specific data
    VkDescriptorSetLayout SmokeSplatDescLayout;
    vk_pipeline* SmokeSplatPipeline;

    VkDescriptorSetLayout SmokeAdvectionDescLayout;
    vk_pipeline* SmokeAdvectionPipeline;

    VkDescriptorSetLayout SmokePressureApplyDescLayout;
    vk_pipeline* SmokePressureApplyPipeline;

    // NOTE: Fire specific data
    VkDescriptorSetLayout FireSplatDescLayout;
    vk_pipeline* FireSplatPipeline;

    VkDescriptorSetLayout FireFuelDescLayout;
    vk_pipeline* FireFuelPipeline;

    VkDescriptorSetLayout FireAdvectionDescLayout;
    vk_pipeline* FireAdvectionPipeline;

    VkDescriptorSetLayout FirePressureApplyDescLayout;
    vk_pipeline* FirePressureApplyPipeline;

    // NOTE: Water specific data
    VkDescriptorSetLayout WaterSplatDescLayout;
    vk_pipeline* WaterSplatPipeline;

    VkDescriptorSetLayout WaterAdvectionDescLayout;
    vk_pipeline* WaterAdvectionPipeline;

    VkDescriptorSetLayout WaterPressureApplyDescLayout;
    vk_pipeline* WaterPressureApplyPipeline;
    
    vk_pipeline* CopyToRtPipeline;
};
