#pragma once

#define TILE_SIZE_IN_PIXELS 8
#define MAX_LIGHTS_PER_TILE 1024

struct tiled_deferred_globals
{
    // TODO: Move to camera?
    m4 InverseProjection;
    v2 ScreenSize;
    u32 GridSizeX;
    u32 GridSizeY;
};

struct directional_shadow
{
    vk_linear_arena Arena;
    
    u32 Width;
    u32 Height;
    VkSampler Sampler;
    VkImage Image;
    render_target_entry Entry;
    render_target Target;
    vk_pipeline* Pipeline;
};

struct tiled_deferred_state
{
    vk_linear_arena RenderTargetArena;

    directional_shadow Shadow;
    
    // NOTE: GBuffer
    VkImage GBufferPositionImage;
    render_target_entry GBufferPositionEntry;
    VkImage GBufferNormalImage;
    render_target_entry GBufferNormalEntry;
    VkImage GBufferMaterialImage;
    render_target_entry GBufferMaterialEntry;
    VkImage DepthImage;
    render_target_entry DepthEntry;
    VkImage OutColorImage;
    render_target_entry OutColorEntry;
    render_target GBufferPass;
    render_target LightingPass;

    // NOTE: Global data
    VkBuffer TiledDeferredGlobals;
    VkBuffer GridFrustums;
    VkBuffer LightIndexList_O;
    VkBuffer LightIndexCounter_O;
    vk_image LightGrid_O;
    VkBuffer LightIndexList_T;
    VkBuffer LightIndexCounter_T;
    vk_image LightGrid_T;
    VkDescriptorSetLayout TiledDeferredDescLayout;
    VkDescriptorSet TiledDeferredDescriptor;

    render_mesh* QuadMesh;
    
    vk_pipeline* GridFrustumPipeline;
    vk_pipeline* GBufferPipeline;
    vk_pipeline* LightCullPipeline;
    vk_pipeline* LightingPipeline;
};
