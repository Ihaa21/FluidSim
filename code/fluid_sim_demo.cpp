
#include "fluid_sim_demo.h"
#include "fluid_sim_diffusion.cpp"
#include "fluid_sim_smoke.cpp"
#include "fluid_sim_fire.cpp"

inline void FluidSimInitBarriers(vk_commands* Commands, vk_image Images[2], u32 InputId)
{
    VkBarrierImageAdd(Commands, Images[InputId].Image, VK_IMAGE_ASPECT_COLOR_BIT, 
                      VK_ACCESS_MEMORY_READ_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
                      VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    VkBarrierImageAdd(Commands, Images[!InputId].Image, VK_IMAGE_ASPECT_COLOR_BIT,
                      VK_ACCESS_MEMORY_READ_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
                      VK_ACCESS_SHADER_WRITE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_IMAGE_LAYOUT_GENERAL);
}

inline void FluidSimInitBarriersUav(vk_commands* Commands, vk_image Images[2], u32 InputId)
{
    VkBarrierImageAdd(Commands, Images[InputId].Image, VK_IMAGE_ASPECT_COLOR_BIT,
                      VK_ACCESS_MEMORY_READ_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
                      VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_IMAGE_LAYOUT_GENERAL);
    VkBarrierImageAdd(Commands, Images[!InputId].Image, VK_IMAGE_ASPECT_COLOR_BIT,
                      VK_ACCESS_MEMORY_READ_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
                      VK_ACCESS_SHADER_WRITE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_IMAGE_LAYOUT_GENERAL);
}

inline void FluidSimSwapBarriers(vk_commands* Commands, vk_image Images[2], u32 InputId)
{
    VkBarrierImageAdd(Commands, Images[InputId].Image, VK_IMAGE_ASPECT_COLOR_BIT,
                      VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                      VK_ACCESS_SHADER_WRITE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_IMAGE_LAYOUT_GENERAL);
    
    VkBarrierImageAdd(Commands, Images[!InputId].Image, VK_IMAGE_ASPECT_COLOR_BIT, 
                      VK_ACCESS_SHADER_WRITE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_IMAGE_LAYOUT_GENERAL,
                      VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
}

inline void FluidSimSwapBarriersUav(vk_commands* Commands, vk_image Images[2], u32 InputId)
{
    VkBarrierImageAdd(Commands, Images[InputId].Image, VK_IMAGE_ASPECT_COLOR_BIT,
                      VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_IMAGE_LAYOUT_GENERAL,
                      VK_ACCESS_SHADER_WRITE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_IMAGE_LAYOUT_GENERAL);
    
    VkBarrierImageAdd(Commands, Images[!InputId].Image, VK_IMAGE_ASPECT_COLOR_BIT,
                      VK_ACCESS_SHADER_WRITE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_IMAGE_LAYOUT_GENERAL,
                      VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_IMAGE_LAYOUT_GENERAL);
}

//
// NOTE: Demo Code
//

inline void DemoAllocGlobals(linear_arena* Arena)
{
    // IMPORTANT: These are always the top of the program memory
    DemoState = PushStruct(Arena, demo_state);
    RenderState = PushStruct(Arena, render_state);
}

DEMO_INIT(Init)
{
    // NOTE: Init Memory
    {
        linear_arena Arena = LinearArenaCreate(ProgramMemory, ProgramMemorySize);
        DemoAllocGlobals(&Arena);
        *DemoState = {};
        *RenderState = {};
        DemoState->Arena = Arena;
        DemoState->TempArena = LinearSubArena(&DemoState->Arena, MegaBytes(10));
    }

    // NOTE: Init Vulkan
    {
        const char* DeviceExtensions[] =
            {
                "VK_EXT_shader_viewport_index_layer",
                "VK_EXT_conservative_rasterization",
                "VK_EXT_descriptor_indexing",
            };
            
        render_init_params InitParams = {};
        InitParams.ValidationEnabled = true;
        //InitParams.PresentMode = VK_PRESENT_MODE_FIFO_KHR;
        InitParams.WindowWidth = WindowWidth;
        InitParams.WindowHeight = WindowHeight;
        InitParams.GpuLocalSize = MegaBytes(800);
        InitParams.DeviceExtensionCount = ArrayCount(DeviceExtensions);
        InitParams.DeviceExtensions = DeviceExtensions;
        VkInit(VulkanLib, hInstance, WindowHandle, &DemoState->Arena, &DemoState->TempArena, InitParams);
    }
    
    // NOTE: Create samplers
    DemoState->PointSampler = VkSamplerCreate(RenderState->Device, VK_FILTER_NEAREST, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK, 0.0f);
    DemoState->LinearSampler = VkSamplerCreate(RenderState->Device, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK, 0.0f);
    DemoState->AnisoSampler = VkSamplerMipMapCreate(RenderState->Device, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, 16.0f,
                                                    VK_SAMPLER_MIPMAP_MODE_LINEAR, 0, 0, 5);    
        
    // NOTE: Init render target entries
    DemoState->SwapChainEntry = RenderTargetSwapChainEntryCreate(RenderState->WindowWidth, RenderState->WindowHeight,
                                                                 RenderState->SwapChainFormat);

    // NOTE: Copy To Swap RT
    {
        render_target_builder Builder = RenderTargetBuilderBegin(&DemoState->Arena, &DemoState->TempArena, RenderState->WindowWidth,
                                                                 RenderState->WindowHeight);
        RenderTargetAddTarget(&Builder, &DemoState->SwapChainEntry, VkClearColorCreate(1, 0, 0, 1));
                            
        vk_render_pass_builder RpBuilder = VkRenderPassBuilderBegin(&DemoState->TempArena);

        u32 ColorId = VkRenderPassAttachmentAdd(&RpBuilder, RenderState->SwapChainFormat, VK_ATTACHMENT_LOAD_OP_CLEAR,
                                                VK_ATTACHMENT_STORE_OP_STORE, VK_IMAGE_LAYOUT_UNDEFINED,
                                                VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

        VkRenderPassSubPassBegin(&RpBuilder, VK_PIPELINE_BIND_POINT_GRAPHICS);
        VkRenderPassColorRefAdd(&RpBuilder, ColorId, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
        VkRenderPassSubPassEnd(&RpBuilder);

        DemoState->CopyToSwapTarget = RenderTargetBuilderEnd(&Builder, VkRenderPassBuilderEnd(&RpBuilder, RenderState->Device));
        DemoState->CopyToSwapPipeline = FullScreenCopyImageCreate(DemoState->CopyToSwapTarget.RenderPass, 0);
    }

    // NOTE: Create Fluid Sim Globals
    {
        DemoState->Type = FluidSimType_Fire;
        DemoState->FluidWidth = 512;
        DemoState->FluidHeight = 512;
        DemoState->GpuArena = VkLinearArenaCreate(RenderState->Device, RenderState->LocalMemoryId, MegaBytes(128));

        DemoState->ClampPointSampler = VkSamplerCreate(RenderState->Device, VK_FILTER_NEAREST, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER, VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK, 0.0f);
        DemoState->ClampLinearSampler = VkSamplerCreate(RenderState->Device, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER, VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK, 0.0f);
    
        DemoState->MirrorPointSampler = VkSamplerCreate(RenderState->Device, VK_FILTER_NEAREST, VK_SAMPLER_ADDRESS_MODE_REPEAT, VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK, 0.0f);
        DemoState->MirrorLinearSampler = VkSamplerCreate(RenderState->Device, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_REPEAT, VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK, 0.0f);
    }
    
    // NOTE: Upload assets
    vk_commands* Commands = &RenderState->Commands;
    VkCommandsBegin(Commands, RenderState->Device);
    {
        // NOTE: Create the fluid sim
        DemoState->DiffusionSim = DiffusionInit(Commands, DemoState->FluidWidth, DemoState->FluidHeight);
        DemoState->SmokeSim = SmokeInit(Commands, DemoState->FluidWidth, DemoState->FluidHeight);
        DemoState->FireSim = FireInit(Commands, DemoState->FluidWidth, DemoState->FluidHeight);
        
        // NOTE: Create UI
        UiStateCreate(RenderState->Device, &DemoState->Arena, &DemoState->TempArena, RenderState->LocalMemoryId,
                      &RenderState->DescriptorManager, &RenderState->PipelineManager, Commands,
                      RenderState->SwapChainFormat, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, &DemoState->UiState);
        
        VkDescriptorManagerFlush(RenderState->Device, &RenderState->DescriptorManager);
        VkCommandsTransferFlush(Commands, RenderState->Device);
    }
    
    VkCommandsSubmit(Commands, RenderState->Device, RenderState->GraphicsQueue);
}

DEMO_DESTROY(Destroy)
{
}

DEMO_SWAPCHAIN_CHANGE(SwapChainChange)
{
    VkCheckResult(vkDeviceWaitIdle(RenderState->Device));
    VkSwapChainReCreate(&DemoState->TempArena, WindowWidth, WindowHeight, RenderState->PresentMode);

    DemoState->SwapChainEntry.Width = RenderState->WindowWidth;
    DemoState->SwapChainEntry.Height = RenderState->WindowHeight;
}

DEMO_CODE_RELOAD(CodeReload)
{
    linear_arena Arena = LinearArenaCreate(ProgramMemory, ProgramMemorySize);
    // IMPORTANT: We are relying on the memory being the same here since we have the same base ptr with the VirtualAlloc so we just need
    // to patch our global pointers here
    DemoAllocGlobals(&Arena);

    VkGetGlobalFunctionPointers(VulkanLib);
    VkGetInstanceFunctionPointers();
    VkGetDeviceFunctionPointers();
}

DEMO_MAIN_LOOP(MainLoop)
{
    u32 ImageIndex;
    VkCheckResult(vkAcquireNextImageKHR(RenderState->Device, RenderState->SwapChain, UINT64_MAX, RenderState->ImageAvailableSemaphore,
                                        VK_NULL_HANDLE, &ImageIndex));
    DemoState->SwapChainEntry.View = RenderState->SwapChainViews[ImageIndex];

    vk_commands* Commands = &RenderState->Commands;
    VkCommandsBegin(Commands, RenderState->Device);

    // NOTE: Update pipelines
    VkPipelineUpdateShaders(RenderState->Device, &RenderState->CpuArena, &RenderState->PipelineManager);

    RenderTargetUpdateEntries(&DemoState->TempArena, &DemoState->CopyToSwapTarget);
    
    // NOTE: Update Ui State
    {
        ui_state* UiState = &DemoState->UiState;
        
        ui_frame_input UiCurrInput = {};
        UiCurrInput.MouseDown = CurrInput->MouseDown;
        UiCurrInput.MousePixelPos = V2(CurrInput->MousePixelPos);
        UiCurrInput.MouseScroll = CurrInput->MouseScroll;
        Copy(CurrInput->KeysDown, UiCurrInput.KeysDown, sizeof(UiCurrInput.KeysDown));
        UiStateBegin(UiState, FrameTime, RenderState->WindowWidth, RenderState->WindowHeight, UiCurrInput);
        local_global v2 PanelPos = V2(100, 800);

        switch (DemoState->Type)
        {
            case FluidSimType_Fire:
            {
                fire_inputs* Inputs = &DemoState->FireSim.Inputs;
                ui_panel Panel = UiPanelBegin(UiState, &PanelPos, "Fire Panel");
                UiPanelText(&Panel, "Fire Data:");

                UiPanelNextRowIndent(&Panel);
                b32 ResetSim = UiPanelButton(&Panel, "Reset Sim");
                b32 RenderColor = UiPanelButton(&Panel, "Render Color");
                UiPanelNextRow(&Panel);

                UiPanelNextRowIndent(&Panel);
                b32 RenderVel = UiPanelButton(&Panel, "Render Vel");
                b32 RenderTemp = UiPanelButton(&Panel, "Render Temperature");
                UiPanelNextRow(&Panel);

                b32 DiffusionEnabled = false;
                UiPanelNextRowIndent(&Panel);
                UiPanelText(&Panel, "Enable Diffusion:");
                UiPanelCheckBox(&Panel, &DiffusionEnabled);
                UiPanelNextRow(&Panel);

                b32 Pressure1StrideEnabled = false;
                UiPanelNextRowIndent(&Panel);
                UiPanelText(&Panel, "Enable Pressure 1 Stride:");
                UiPanelCheckBox(&Panel, &Pressure1StrideEnabled);
                UiPanelNextRow(&Panel);
            
                UiPanelNextRowIndent(&Panel);
                UiPanelText(&Panel, "Density:");
                UiPanelHorizontalSlider(&Panel, 0.0f, 10.0f, &Inputs->Density);
                UiPanelNumberBox(&Panel, 0.0f, 10.0f, &Inputs->Density);
                UiPanelNextRow(&Panel);

#if 0
                u32 OldResolution = Inputs->Dim;
                UiPanelNextRowIndent(&Panel);
                UiPanelText(&Panel, "Resolution:");
                UiPanelHorizontalSlider(&Panel, 0, 1024, &Inputs->Dim);
                UiPanelNumberBox(&Panel, 0, 1024, &Inputs->Dim);
                UiPanelNextRow(&Panel);
#endif
                
                b32 ResolutionChanged = false; //OldResolution != Inputs->Dim;
                
                UiPanelNextRowIndent(&Panel);
                UiPanelText(&Panel, "Gravity:");
                UiPanelHorizontalSlider(&Panel, 0.0f, 40.0f, &Inputs->Gravity);
                UiPanelNumberBox(&Panel, 0.0f, 40.0f, &Inputs->Gravity);
                UiPanelNextRow(&Panel);
            
                UiPanelNextRowIndent(&Panel);
                UiPanelText(&Panel, "RoomTemperature:");
                UiPanelHorizontalSlider(&Panel, 0.0f, 200.0f, &Inputs->RoomTemperature);
                UiPanelNumberBox(&Panel, 0.0f, 200.0f, &Inputs->RoomTemperature);
                UiPanelNextRow(&Panel);
            
                UiPanelNextRowIndent(&Panel);
                UiPanelText(&Panel, "MolarMass:");
                UiPanelHorizontalSlider(&Panel, 0.0f, 500.0f, &Inputs->MolarMass);
                UiPanelNumberBox(&Panel, 0.0f, 500.0f, &Inputs->MolarMass);
                UiPanelNextRow(&Panel);
            
                UiPanelNextRowIndent(&Panel);
                UiPanelText(&Panel, "R:");
                UiPanelHorizontalSlider(&Panel, 0.0f, 40.0f, &Inputs->R);
                UiPanelNumberBox(&Panel, 0.0f, 40.0f, &Inputs->R);
                UiPanelNextRow(&Panel);
            
                UiPanelNextRowIndent(&Panel);
                UiPanelText(&Panel, "Buoyancy:");
                UiPanelHorizontalSlider(&Panel, 0.0f, 5.0f, &Inputs->Buoyancy);
                UiPanelNumberBox(&Panel, 0.0f, 5.0f, &Inputs->Buoyancy);
                UiPanelNextRow(&Panel);
            
                UiPanelNextRowIndent(&Panel);
                UiPanelText(&Panel, "SplatRadius:");
                UiPanelHorizontalSlider(&Panel, 0.0f, 1.0f, &Inputs->SplatRadius);
                UiPanelNumberBox(&Panel, 0.0f, 1.0f, &Inputs->SplatRadius);
                UiPanelNextRow(&Panel);
        
                UiPanelEnd(&Panel);

                if (ResolutionChanged || ResetSim)
                {
                    FireResize(&DemoState->FireSim, Commands, Inputs->Dim, Inputs->Dim);
                }
            } break;
        }
        
        UiStateEnd(UiState, &RenderState->DescriptorManager);
    }

    // NOTE: Upload scene data
    {
        switch (DemoState->Type)
        {
            case FluidSimType_Diffusion:
            {
                DiffusionSetInputs(&DemoState->DiffusionSim, Commands, FrameTime, CurrInput->MouseNormalizedPos, PrevInput->MouseNormalizedPos);
            } break;
            
            case FluidSimType_Smoke:
            {
                SmokeSetInputs(&DemoState->SmokeSim, Commands, FrameTime, CurrInput->MouseNormalizedPos, PrevInput->MouseNormalizedPos);
            } break;
            
            case FluidSimType_Fire:
            {
                b32 MouseDown = CurrInput->MouseDown && !DemoState->UiState.MouseTouchingUi && !DemoState->UiState.ProcessedInteraction;
                FireSetInputs(&DemoState->FireSim, Commands, FrameTime, CurrInput->MouseNormalizedPos, PrevInput->MouseNormalizedPos, MouseDown);
            } break;
            
            case FluidSimType_Water2d:
            {
            } break;
        }
        
        VkCommandsTransferFlush(Commands, RenderState->Device);
    }

    // NOTE: Simulate fluid
    switch (DemoState->Type)
    {
        case FluidSimType_Diffusion:
        {
            DiffusionSimulate(Commands, &DemoState->DiffusionSim);
        } break;
            
        case FluidSimType_Smoke:
        {
            SmokeSimulate(Commands, &DemoState->SmokeSim);
        } break;
            
        case FluidSimType_Fire:
        {
            FireSimulate(Commands, &DemoState->FireSim);
        } break;
            
        case FluidSimType_Water2d:
        {
        } break;
    }

    // NOTE: Render simulation
    RenderTargetPassBegin(&DemoState->CopyToSwapTarget, Commands, RenderTargetRenderPass_SetViewPort | RenderTargetRenderPass_SetScissor);
    {
        VkDescriptorSet RenderSet = {};
        switch (DemoState->Type)
        {
            case FluidSimType_Diffusion:
            {
                RenderSet = DemoState->DiffusionSim.RenderDescriptors[DemoState->DiffusionSim.InputId];
            } break;

            case FluidSimType_Smoke:
            {
                RenderSet = DemoState->SmokeSim.RenderDescriptors[DemoState->SmokeSim.InputId];
            } break;

            case FluidSimType_Fire:
            {
                RenderSet = DemoState->FireSim.RenderDescriptors[DemoState->FireSim.InputId];
            } break;
        }

        FullScreenPassRender(Commands, DemoState->CopyToSwapPipeline, 1, &RenderSet);
    }
    RenderTargetPassEnd(Commands);
    
    UiStateRender(&DemoState->UiState, RenderState->Device, Commands, DemoState->SwapChainEntry.View);
    VkCommandsEnd(Commands, RenderState->Device);
                    
    // NOTE: Render to our window surface
    // NOTE: Tell queue where we render to surface to wait
    VkPipelineStageFlags WaitDstMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    VkSubmitInfo SubmitInfo = {};
    SubmitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    SubmitInfo.waitSemaphoreCount = 1;
    SubmitInfo.pWaitSemaphores = &RenderState->ImageAvailableSemaphore;
    SubmitInfo.pWaitDstStageMask = &WaitDstMask;
    SubmitInfo.commandBufferCount = 1;
    SubmitInfo.pCommandBuffers = &Commands->Buffer;
    SubmitInfo.signalSemaphoreCount = 1;
    SubmitInfo.pSignalSemaphores = &RenderState->FinishedRenderingSemaphore;
    VkCheckResult(vkQueueSubmit(RenderState->GraphicsQueue, 1, &SubmitInfo, Commands->Fence));
    
    VkPresentInfoKHR PresentInfo = {};
    PresentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    PresentInfo.waitSemaphoreCount = 1;
    PresentInfo.pWaitSemaphores = &RenderState->FinishedRenderingSemaphore;
    PresentInfo.swapchainCount = 1;
    PresentInfo.pSwapchains = &RenderState->SwapChain;
    PresentInfo.pImageIndices = &ImageIndex;
    VkResult Result = vkQueuePresentKHR(RenderState->PresentQueue, &PresentInfo);

    switch (Result)
    {
        case VK_SUCCESS:
        {
        } break;

        case VK_ERROR_OUT_OF_DATE_KHR:
        case VK_SUBOPTIMAL_KHR:
        {
            // NOTE: Window size changed
            InvalidCodePath;
        } break;

        default:
        {
            InvalidCodePath;
        } break;
    }
}
