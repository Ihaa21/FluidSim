#pragma once

#define VALIDATION 1

#include "framework_vulkan\framework_vulkan.h"
#include "fluid_sim_diffusion.h"
#include "fluid_sim_smoke.h"
#include "fluid_sim_fire.h"
#include "fluid_sim_water_2d.h"

enum fluid_sim_type
{
    FluidSimType_None,

    FluidSimType_Diffusion,
    FluidSimType_Smoke,
    FluidSimType_Fire,
    FluidSimType_Water2d,
};

struct demo_state
{
    linear_arena Arena;
    linear_arena TempArena;

    // NOTE: Samplers
    VkSampler PointSampler;
    VkSampler LinearSampler;
    VkSampler AnisoSampler;

    // NOTE: Render Target Entries
    render_target_entry SwapChainEntry;
    render_target CopyToSwapTarget;
    vk_pipeline* CopyToSwapPipeline;
    
    ui_state UiState;

    // NOTE: Fluid Sim Data
    vk_linear_arena GpuArena;

    u32 FluidWidth;
    u32 FluidHeight;
    u32 PressureInputId;

    fluid_sim_type Type;
    diffusion_sim DiffusionSim;
    smoke_sim SmokeSim;
    fire_sim FireSim;
    water_2d_sim WaterSim2d;

    VkSampler MirrorPointSampler;
    VkSampler MirrorLinearSampler;
    VkSampler ClampPointSampler;
    VkSampler ClampLinearSampler;
};

global demo_state* DemoState;

inline void FluidSimInitBarriers(vk_commands* Commands, vk_image Images[2], u32 InputId);
inline void FluidSimInitBarriersUav(vk_commands* Commands, vk_image Images[2], u32 InputId);
inline void FluidSimSwapBarriers(vk_commands* Commands, vk_image Images[2], u32 InputId);
inline void FluidSimSwapBarriersUav(vk_commands* Commands, vk_image Images[2], u32 InputId);
