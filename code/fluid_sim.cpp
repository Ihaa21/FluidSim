
/*

  NOTE: Sources:

    - http://jamie-wong.com/2016/08/05/webgl-fluid-simulation/ (IMPLEMENTED)
    - http://developer.download.nvidia.com/books/HTML/gpugems/gpugems_ch38.html
    - https://developer.nvidia.com/gpugems/gpugems3/part-v-physics-simulation/chapter-30-real-time-simulation-and-rendering-3d-fluids
    - https://www.shahinrabbani.ca/torch2pd.html
    - 
  
*/

inline void FluidSimInitBarriers(vk_commands Commands, vk_image Images[2], u32 InputId)
{
    VkBarrierImageAdd(&RenderState->BarrierManager, VK_ACCESS_MEMORY_READ_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                      VK_IMAGE_LAYOUT_UNDEFINED, VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                      VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT, Images[InputId].Image);
    VkBarrierImageAdd(&RenderState->BarrierManager, VK_ACCESS_MEMORY_READ_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                      VK_IMAGE_LAYOUT_UNDEFINED, VK_ACCESS_SHADER_WRITE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                      VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_ASPECT_COLOR_BIT, Images[!InputId].Image);
}

inline void FluidSimInitBarriersUav(vk_commands Commands, vk_image Images[2], u32 InputId)
{
    VkBarrierImageAdd(&RenderState->BarrierManager, VK_ACCESS_MEMORY_READ_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                      VK_IMAGE_LAYOUT_UNDEFINED, VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                      VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_ASPECT_COLOR_BIT, Images[InputId].Image);
    VkBarrierImageAdd(&RenderState->BarrierManager, VK_ACCESS_MEMORY_READ_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                      VK_IMAGE_LAYOUT_UNDEFINED, VK_ACCESS_SHADER_WRITE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                      VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_ASPECT_COLOR_BIT, Images[!InputId].Image);
}

inline void FluidSimSwapBarriers(vk_commands Commands, vk_image Images[2], u32 InputId)
{
    VkBarrierImageAdd(&RenderState->BarrierManager, VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                      VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_SHADER_WRITE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                      VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_ASPECT_COLOR_BIT, Images[InputId].Image);
    
    VkBarrierImageAdd(&RenderState->BarrierManager, VK_ACCESS_SHADER_WRITE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                      VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                      VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT, Images[!InputId].Image);
}

inline void FluidSimSwapBarriersUav(vk_commands Commands, vk_image Images[2], u32 InputId)
{
    VkBarrierImageAdd(&RenderState->BarrierManager, VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                      VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_SHADER_WRITE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                      VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_ASPECT_COLOR_BIT, Images[InputId].Image);
    
    VkBarrierImageAdd(&RenderState->BarrierManager, VK_ACCESS_SHADER_WRITE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                      VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                      VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_ASPECT_COLOR_BIT, Images[!InputId].Image);
}

//
// NOTE: Diffusion
// 

inline diffusion_sim DiffusionInit(vk_commands Commands, fluid_sim* FluidSim)
{
    diffusion_sim Result = {};
    
    Result.ColorImages[0] = VkImageCreate(RenderState->Device, &FluidSim->Arena, FluidSim->Width, FluidSim->Height, VK_FORMAT_R32G32B32A32_SFLOAT,
                                          VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_IMAGE_ASPECT_COLOR_BIT);
    Result.ColorImages[1] = VkImageCreate(RenderState->Device, &FluidSim->Arena, FluidSim->Width, FluidSim->Height, VK_FORMAT_R32G32B32A32_SFLOAT,
                                          VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_IMAGE_ASPECT_COLOR_BIT);
    
    // NOTE: Diffusion Init Pipeline
    {
        {
            vk_descriptor_layout_builder Builder = VkDescriptorLayoutBegin(&FluidSim->DiffusionInitDescLayout);
            VkDescriptorLayoutAdd(&Builder, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT);
            VkDescriptorLayoutAdd(&Builder, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT);
            VkDescriptorLayoutEnd(RenderState->Device, &Builder);
        }

        VkDescriptorSetLayout Layouts[] = 
            {
                FluidSim->GlobalDescLayout,
                FluidSim->DiffusionInitDescLayout,
            };
            
        FluidSim->DiffusionInitPipeline = VkPipelineComputeCreate(RenderState->Device, &RenderState->PipelineManager, &DemoState->TempArena,
                                                                  "fluid_diffusion_init.spv", "main", Layouts, ArrayCount(Layouts));
    }
    
    // NOTE: Diffusion Advection Pipeline
    {
        {
            vk_descriptor_layout_builder Builder = VkDescriptorLayoutBegin(&FluidSim->DiffusionAdvectionDescLayout);
            VkDescriptorLayoutAdd(&Builder, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT);
            VkDescriptorLayoutAdd(&Builder, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT);
            VkDescriptorLayoutEnd(RenderState->Device, &Builder);
        }

        VkDescriptorSetLayout Layouts[] = 
            {
                FluidSim->GlobalDescLayout,
                FluidSim->DiffusionAdvectionDescLayout,
            };
            
        FluidSim->DiffusionAdvectionPipeline = VkPipelineComputeCreate(RenderState->Device, &RenderState->PipelineManager, &DemoState->TempArena,
                                                                       "fluid_diffusion_advection.spv", "main", Layouts, ArrayCount(Layouts));
    }

    // NOTE: Diffusion Pressure Apply Pipeline
    {
        {
            vk_descriptor_layout_builder Builder = VkDescriptorLayoutBegin(&FluidSim->DiffusionPressureApplyDescLayout);
            VkDescriptorLayoutAdd(&Builder, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT);
            VkDescriptorLayoutAdd(&Builder, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT);
            VkDescriptorLayoutAdd(&Builder, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT);
            VkDescriptorLayoutEnd(RenderState->Device, &Builder);
        }
        
        VkDescriptorSetLayout Layouts[] = 
            {
                FluidSim->GlobalDescLayout,
                FluidSim->DiffusionPressureApplyDescLayout,
                FluidSim->PressureDescLayout,
            };
            
        FluidSim->DiffusionPressureApplyPipeline = VkPipelineComputeCreate(RenderState->Device, &RenderState->PipelineManager, &DemoState->TempArena,
                                                                           "fluid_diffusion_pressure_apply.spv", "main", Layouts, ArrayCount(Layouts));
    }
    
    // NOTE: Populate descriptors
    {    
        // NOTE: Init Descriptor
        {
            Result.InitDescriptor = VkDescriptorSetAllocate(RenderState->Device, RenderState->DescriptorPool, FluidSim->DiffusionInitDescLayout);
            VkDescriptorImageWrite(&RenderState->DescriptorManager, Result.InitDescriptor, 0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                                   Result.ColorImages[0].View, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_GENERAL);
            VkDescriptorImageWrite(&RenderState->DescriptorManager, Result.InitDescriptor, 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                                   FluidSim->VelocityImages[0].View, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_GENERAL);
        }

        // NOTE: Advection Descriptor (ping pong)
        {
            Result.AdvectionDescriptors[0] = VkDescriptorSetAllocate(RenderState->Device, RenderState->DescriptorPool, FluidSim->DiffusionAdvectionDescLayout);
            VkDescriptorImageWrite(&RenderState->DescriptorManager, Result.AdvectionDescriptors[0], 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                   FluidSim->VelocityImages[0].View, FluidSim->MirrorLinearSampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
            VkDescriptorImageWrite(&RenderState->DescriptorManager, Result.AdvectionDescriptors[0], 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                                   FluidSim->VelocityImages[1].View, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_GENERAL);

            Result.AdvectionDescriptors[1] = VkDescriptorSetAllocate(RenderState->Device, RenderState->DescriptorPool, FluidSim->DiffusionAdvectionDescLayout);
            VkDescriptorImageWrite(&RenderState->DescriptorManager, Result.AdvectionDescriptors[1], 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                   FluidSim->VelocityImages[1].View, FluidSim->MirrorLinearSampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
            VkDescriptorImageWrite(&RenderState->DescriptorManager, Result.AdvectionDescriptors[1], 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                                   FluidSim->VelocityImages[0].View, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_GENERAL);
        }

        // NOTE: Pressure Apply Descriptor
        {
            Result.PressureApplyDescriptors[0] = VkDescriptorSetAllocate(RenderState->Device, RenderState->DescriptorPool, FluidSim->DiffusionPressureApplyDescLayout);
            VkDescriptorImageWrite(&RenderState->DescriptorManager, Result.PressureApplyDescriptors[0], 0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                                   FluidSim->VelocityImages[1].View, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_GENERAL);
            VkDescriptorImageWrite(&RenderState->DescriptorManager, Result.PressureApplyDescriptors[0], 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                   Result.ColorImages[0].View, FluidSim->MirrorLinearSampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
            VkDescriptorImageWrite(&RenderState->DescriptorManager, Result.PressureApplyDescriptors[0], 2, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                                   Result.ColorImages[1].View, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_GENERAL);

            Result.PressureApplyDescriptors[1] = VkDescriptorSetAllocate(RenderState->Device, RenderState->DescriptorPool, FluidSim->DiffusionPressureApplyDescLayout);
            VkDescriptorImageWrite(&RenderState->DescriptorManager, Result.PressureApplyDescriptors[1], 0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                                   FluidSim->VelocityImages[0].View, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_GENERAL);
            VkDescriptorImageWrite(&RenderState->DescriptorManager, Result.PressureApplyDescriptors[1], 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                   Result.ColorImages[1].View, FluidSim->MirrorLinearSampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
            VkDescriptorImageWrite(&RenderState->DescriptorManager, Result.PressureApplyDescriptors[1], 2, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                                   Result.ColorImages[0].View, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_GENERAL);
        }
        
        // NOTE: Render Descriptors
        {
            Result.RenderDescriptors[0] = VkDescriptorSetAllocate(RenderState->Device, RenderState->DescriptorPool, RenderState->CopyImageDescLayout);
            VkDescriptorImageWrite(&RenderState->DescriptorManager, Result.RenderDescriptors[0], 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                   Result.ColorImages[0].View, DemoState->LinearSampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
            
            Result.RenderDescriptors[1] = VkDescriptorSetAllocate(RenderState->Device, RenderState->DescriptorPool, RenderState->CopyImageDescLayout);
            VkDescriptorImageWrite(&RenderState->DescriptorManager, Result.RenderDescriptors[1], 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                   Result.ColorImages[1].View, DemoState->LinearSampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        }

        VkDescriptorManagerFlush(RenderState->Device, &RenderState->DescriptorManager);
    }
    
    FluidSimInitBarriers(Commands, FluidSim->VelocityImages, 1);
    FluidSimInitBarriersUav(Commands, FluidSim->PressureImages, 1);
    FluidSimInitBarriers(Commands, Result.ColorImages, 1);
    VkBarrierManagerFlush(&RenderState->BarrierManager, Commands.Buffer);

    // NOTE: Populate color images
    {
        VkClearColorValue ClearValue = VkClearColorCreate(0, 0, 0, 0).color;
        VkImageSubresourceRange Range = {};
        Range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        Range.baseMipLevel = 0;
        Range.levelCount = 1;
        Range.baseArrayLayer = 0;
        Range.layerCount = 1;

        VkDescriptorSet DescriptorSets[] =
            {
                FluidSim->GlobalDescriptor,
                Result.InitDescriptor,
            };
        u32 DispatchX = CeilU32(f32(FluidSim->Width) / 8.0f);
        u32 DispatchY = CeilU32(f32(FluidSim->Height) / 8.0f);
        VkComputeDispatch(Commands, FluidSim->DiffusionInitPipeline, DescriptorSets, ArrayCount(DescriptorSets), DispatchX, DispatchY, 1);

        vkCmdClearColorImage(Commands.Buffer, FluidSim->PressureImages[0].Image, VK_IMAGE_LAYOUT_GENERAL, &ClearValue, 1, &Range);
        
        FluidSimSwapBarriers(Commands, Result.ColorImages, 1);
        FluidSimSwapBarriers(Commands, FluidSim->VelocityImages, 1);
        FluidSimSwapBarriersUav(Commands, FluidSim->PressureImages, 1);
        VkBarrierManagerFlush(&RenderState->BarrierManager, Commands.Buffer);
    }

    return Result;
}

//
// NOTE: Smoke
//

inline smoke_sim SmokeInit(vk_commands Commands, fluid_sim* FluidSim)
{
    smoke_sim Result = {};

    VkImageUsageFlags ImageFlags = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    
    Result.ColorImages[0] = VkImageCreate(RenderState->Device, &FluidSim->Arena, FluidSim->Width, FluidSim->Height, VK_FORMAT_R32G32B32A32_SFLOAT,
                                          ImageFlags, VK_IMAGE_ASPECT_COLOR_BIT);
    Result.ColorImages[1] = VkImageCreate(RenderState->Device, &FluidSim->Arena, FluidSim->Width, FluidSim->Height, VK_FORMAT_R32G32B32A32_SFLOAT,
                                          ImageFlags, VK_IMAGE_ASPECT_COLOR_BIT);
    
    Result.TemperatureImages[0] = VkImageCreate(RenderState->Device, &FluidSim->Arena, FluidSim->Width, FluidSim->Height, VK_FORMAT_R32_SFLOAT,
                                                ImageFlags, VK_IMAGE_ASPECT_COLOR_BIT);
    Result.TemperatureImages[1] = VkImageCreate(RenderState->Device, &FluidSim->Arena, FluidSim->Width, FluidSim->Height, VK_FORMAT_R32_SFLOAT,
                                                ImageFlags, VK_IMAGE_ASPECT_COLOR_BIT);
    
    // NOTE: Smoke Splat Pipeline
    {
        {
            vk_descriptor_layout_builder Builder = VkDescriptorLayoutBegin(&FluidSim->SmokeSplatDescLayout);
            VkDescriptorLayoutAdd(&Builder, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT);
            VkDescriptorLayoutAdd(&Builder, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT);
            VkDescriptorLayoutAdd(&Builder, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT);
            VkDescriptorLayoutEnd(RenderState->Device, &Builder);
        }

        VkDescriptorSetLayout Layouts[] = 
            {
                FluidSim->GlobalDescLayout,
                FluidSim->SmokeSplatDescLayout,
            };
            
        FluidSim->SmokeSplatPipeline = VkPipelineComputeCreate(RenderState->Device, &RenderState->PipelineManager, &DemoState->TempArena,
                                                               "fluid_smoke_splat.spv", "main", Layouts, ArrayCount(Layouts));
    }
        
    // NOTE: Smoke Advection Pipeline
    {
        {
            vk_descriptor_layout_builder Builder = VkDescriptorLayoutBegin(&FluidSim->SmokeAdvectionDescLayout);
            VkDescriptorLayoutAdd(&Builder, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT);
            VkDescriptorLayoutAdd(&Builder, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT);
            VkDescriptorLayoutAdd(&Builder, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT);
            VkDescriptorLayoutEnd(RenderState->Device, &Builder);
        }

        VkDescriptorSetLayout Layouts[] = 
            {
                FluidSim->GlobalDescLayout,
                FluidSim->SmokeAdvectionDescLayout,
                FluidSim->PressureDescLayout,
            };
            
        FluidSim->SmokeAdvectionPipeline = VkPipelineComputeCreate(RenderState->Device, &RenderState->PipelineManager, &DemoState->TempArena,
                                                                   "fluid_smoke_advection.spv", "main", Layouts, ArrayCount(Layouts));
    }

    // NOTE: Smoke Pressure Apply Pipeline
    {
        {
            vk_descriptor_layout_builder Builder = VkDescriptorLayoutBegin(&FluidSim->SmokePressureApplyDescLayout);
            VkDescriptorLayoutAdd(&Builder, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT);
            VkDescriptorLayoutAdd(&Builder, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT);
            VkDescriptorLayoutAdd(&Builder, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT);
            VkDescriptorLayoutAdd(&Builder, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT);
            VkDescriptorLayoutAdd(&Builder, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT);
            VkDescriptorLayoutEnd(RenderState->Device, &Builder);
        }
        
        VkDescriptorSetLayout Layouts[] = 
            {
                FluidSim->GlobalDescLayout,
                FluidSim->SmokePressureApplyDescLayout,
                FluidSim->PressureDescLayout,
            };
            
        FluidSim->SmokePressureApplyPipeline = VkPipelineComputeCreate(RenderState->Device, &RenderState->PipelineManager, &DemoState->TempArena,
                                                                       "fluid_smoke_pressure_apply.spv", "main", Layouts, ArrayCount(Layouts));
    }
    
    // NOTE: Populate descriptors
    {    
        // NOTE: Splat Descriptors (ping pong)
        {
            Result.SplatDescriptors[0] = VkDescriptorSetAllocate(RenderState->Device, RenderState->DescriptorPool, FluidSim->SmokeSplatDescLayout);
            VkDescriptorImageWrite(&RenderState->DescriptorManager, Result.SplatDescriptors[0], 0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                                   Result.TemperatureImages[0].View, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_GENERAL);
            VkDescriptorImageWrite(&RenderState->DescriptorManager, Result.SplatDescriptors[0], 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                                   Result.ColorImages[0].View, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_GENERAL);
            VkDescriptorImageWrite(&RenderState->DescriptorManager, Result.SplatDescriptors[0], 2, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                                   FluidSim->VelocityImages[0].View, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_GENERAL);

            Result.SplatDescriptors[1] = VkDescriptorSetAllocate(RenderState->Device, RenderState->DescriptorPool, FluidSim->SmokeSplatDescLayout);
            VkDescriptorImageWrite(&RenderState->DescriptorManager, Result.SplatDescriptors[1], 0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                                   Result.TemperatureImages[1].View, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_GENERAL);
            VkDescriptorImageWrite(&RenderState->DescriptorManager, Result.SplatDescriptors[1], 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                                   Result.ColorImages[1].View, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_GENERAL);
            VkDescriptorImageWrite(&RenderState->DescriptorManager, Result.SplatDescriptors[1], 2, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                                   FluidSim->VelocityImages[1].View, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_GENERAL);
        }

        // NOTE: Advection Descriptor (ping pong)
        {
            Result.AdvectionDescriptors[0] = VkDescriptorSetAllocate(RenderState->Device, RenderState->DescriptorPool, FluidSim->SmokeAdvectionDescLayout);
            VkDescriptorImageWrite(&RenderState->DescriptorManager, Result.AdvectionDescriptors[0], 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                   FluidSim->VelocityImages[0].View, FluidSim->ClampLinearSampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
            VkDescriptorImageWrite(&RenderState->DescriptorManager, Result.AdvectionDescriptors[0], 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                                   FluidSim->VelocityImages[1].View, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_GENERAL);
            VkDescriptorImageWrite(&RenderState->DescriptorManager, Result.AdvectionDescriptors[0], 2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                   Result.TemperatureImages[0].View, FluidSim->ClampLinearSampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

            Result.AdvectionDescriptors[1] = VkDescriptorSetAllocate(RenderState->Device, RenderState->DescriptorPool, FluidSim->SmokeAdvectionDescLayout);
            VkDescriptorImageWrite(&RenderState->DescriptorManager, Result.AdvectionDescriptors[1], 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                   FluidSim->VelocityImages[1].View, FluidSim->ClampLinearSampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
            VkDescriptorImageWrite(&RenderState->DescriptorManager, Result.AdvectionDescriptors[1], 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                                   FluidSim->VelocityImages[0].View, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_GENERAL);
            VkDescriptorImageWrite(&RenderState->DescriptorManager, Result.AdvectionDescriptors[1], 2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                   Result.TemperatureImages[1].View, FluidSim->ClampLinearSampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        }

        // NOTE: Pressure Apply Descriptor
        {
            Result.PressureApplyDescriptors[0] = VkDescriptorSetAllocate(RenderState->Device, RenderState->DescriptorPool, FluidSim->SmokePressureApplyDescLayout);
            VkDescriptorImageWrite(&RenderState->DescriptorManager, Result.PressureApplyDescriptors[0], 0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                                   FluidSim->VelocityImages[1].View, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_GENERAL);
            VkDescriptorImageWrite(&RenderState->DescriptorManager, Result.PressureApplyDescriptors[0], 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                   Result.ColorImages[0].View, FluidSim->ClampLinearSampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
            VkDescriptorImageWrite(&RenderState->DescriptorManager, Result.PressureApplyDescriptors[0], 2, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                                   Result.ColorImages[1].View, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_GENERAL);
            VkDescriptorImageWrite(&RenderState->DescriptorManager, Result.PressureApplyDescriptors[0], 3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                   Result.TemperatureImages[0].View, FluidSim->ClampLinearSampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
            VkDescriptorImageWrite(&RenderState->DescriptorManager, Result.PressureApplyDescriptors[0], 4, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                                   Result.TemperatureImages[1].View, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_GENERAL);

            Result.PressureApplyDescriptors[1] = VkDescriptorSetAllocate(RenderState->Device, RenderState->DescriptorPool, FluidSim->SmokePressureApplyDescLayout);
            VkDescriptorImageWrite(&RenderState->DescriptorManager, Result.PressureApplyDescriptors[1], 0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                                   FluidSim->VelocityImages[0].View, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_GENERAL);
            VkDescriptorImageWrite(&RenderState->DescriptorManager, Result.PressureApplyDescriptors[1], 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                   Result.ColorImages[1].View, FluidSim->ClampLinearSampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
            VkDescriptorImageWrite(&RenderState->DescriptorManager, Result.PressureApplyDescriptors[1], 2, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                                   Result.ColorImages[0].View, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_GENERAL);
            VkDescriptorImageWrite(&RenderState->DescriptorManager, Result.PressureApplyDescriptors[1], 3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                   Result.TemperatureImages[1].View, FluidSim->ClampLinearSampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
            VkDescriptorImageWrite(&RenderState->DescriptorManager, Result.PressureApplyDescriptors[1], 4, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                                   Result.TemperatureImages[0].View, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_GENERAL);
        }
        
        // NOTE: Render Descriptors
        {
            Result.RenderDescriptors[0] = VkDescriptorSetAllocate(RenderState->Device, RenderState->DescriptorPool, RenderState->CopyImageDescLayout);
            VkDescriptorImageWrite(&RenderState->DescriptorManager, Result.RenderDescriptors[0], 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                   Result.ColorImages[0].View, DemoState->LinearSampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
            
            Result.RenderDescriptors[1] = VkDescriptorSetAllocate(RenderState->Device, RenderState->DescriptorPool, RenderState->CopyImageDescLayout);
            VkDescriptorImageWrite(&RenderState->DescriptorManager, Result.RenderDescriptors[1], 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                   Result.ColorImages[1].View, DemoState->LinearSampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        }

        VkDescriptorManagerFlush(RenderState->Device, &RenderState->DescriptorManager);
    }

    FluidSimInitBarriers(Commands, Result.ColorImages, 1);
    FluidSimInitBarriers(Commands, FluidSim->VelocityImages, 1);
    FluidSimInitBarriersUav(Commands, FluidSim->PressureImages, 1);
    FluidSimInitBarriers(Commands, Result.TemperatureImages, 1);
    VkBarrierManagerFlush(&RenderState->BarrierManager, Commands.Buffer);

    // NOTE: Clear all images
    {
        FluidSim->UniformsCpu.RoomTemperature = 1.0f;
        
        VkClearColorValue ClearValue = VkClearColorCreate(0, 0, 0, 0).color;
        VkClearColorValue TemperatureValue = VkClearColorCreate(FluidSim->UniformsCpu.RoomTemperature, 0, 0, 0).color;
        VkImageSubresourceRange Range = {};
        Range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        Range.baseMipLevel = 0;
        Range.levelCount = 1;
        Range.baseArrayLayer = 0;
        Range.layerCount = 1;
        
        vkCmdClearColorImage(Commands.Buffer, Result.ColorImages[0].Image, VK_IMAGE_LAYOUT_GENERAL, &ClearValue, 1, &Range);
        vkCmdClearColorImage(Commands.Buffer, FluidSim->VelocityImages[0].Image, VK_IMAGE_LAYOUT_GENERAL, &ClearValue, 1, &Range);
        vkCmdClearColorImage(Commands.Buffer, FluidSim->PressureImages[0].Image, VK_IMAGE_LAYOUT_GENERAL, &ClearValue, 1, &Range);
        vkCmdClearColorImage(Commands.Buffer, Result.TemperatureImages[0].Image, VK_IMAGE_LAYOUT_GENERAL, &TemperatureValue, 1, &Range);

        FluidSimSwapBarriers(Commands, Result.ColorImages, 1);
        FluidSimSwapBarriers(Commands, FluidSim->VelocityImages, 1);
        FluidSimSwapBarriersUav(Commands, FluidSim->PressureImages, 1);
        FluidSimSwapBarriers(Commands, Result.TemperatureImages, 1);
        VkBarrierManagerFlush(&RenderState->BarrierManager, Commands.Buffer);

        // NOTE: Init smoke
    }

    return Result;
}

//
// NOTE: Fire
//

inline fire_sim FireInit(vk_commands Commands, fluid_sim* FluidSim)
{
    fire_sim Result = {};

    VkImageUsageFlags ImageFlags = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    
    Result.ColorImages[0] = VkImageCreate(RenderState->Device, &FluidSim->Arena, FluidSim->Width, FluidSim->Height, VK_FORMAT_R32G32B32A32_SFLOAT,
                                          ImageFlags, VK_IMAGE_ASPECT_COLOR_BIT);
    Result.ColorImages[1] = VkImageCreate(RenderState->Device, &FluidSim->Arena, FluidSim->Width, FluidSim->Height, VK_FORMAT_R32G32B32A32_SFLOAT,
                                          ImageFlags, VK_IMAGE_ASPECT_COLOR_BIT);
    
    Result.TimerImages[0] = VkImageCreate(RenderState->Device, &FluidSim->Arena, FluidSim->Width, FluidSim->Height, VK_FORMAT_R32_SFLOAT,
                                          ImageFlags, VK_IMAGE_ASPECT_COLOR_BIT);
    Result.TimerImages[1] = VkImageCreate(RenderState->Device, &FluidSim->Arena, FluidSim->Width, FluidSim->Height, VK_FORMAT_R32_SFLOAT,
                                          ImageFlags, VK_IMAGE_ASPECT_COLOR_BIT);
    
    Result.TemperatureImages[0] = VkImageCreate(RenderState->Device, &FluidSim->Arena, FluidSim->Width, FluidSim->Height, VK_FORMAT_R32_SFLOAT,
                                                ImageFlags, VK_IMAGE_ASPECT_COLOR_BIT);
    Result.TemperatureImages[1] = VkImageCreate(RenderState->Device, &FluidSim->Arena, FluidSim->Width, FluidSim->Height, VK_FORMAT_R32_SFLOAT,
                                                ImageFlags, VK_IMAGE_ASPECT_COLOR_BIT);
    
    // NOTE: Fire Splat Pipeline
    {
        {
            vk_descriptor_layout_builder Builder = VkDescriptorLayoutBegin(&FluidSim->FireSplatDescLayout);
            VkDescriptorLayoutAdd(&Builder, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT);
            VkDescriptorLayoutAdd(&Builder, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT);
            VkDescriptorLayoutAdd(&Builder, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT);
            VkDescriptorLayoutEnd(RenderState->Device, &Builder);
        }

        VkDescriptorSetLayout Layouts[] = 
            {
                FluidSim->GlobalDescLayout,
                FluidSim->FireSplatDescLayout,
            };
            
        FluidSim->FireSplatPipeline = VkPipelineComputeCreate(RenderState->Device, &RenderState->PipelineManager, &DemoState->TempArena,
                                                              "fluid_fire_splat.spv", "main", Layouts, ArrayCount(Layouts));
    }

    // NOTE: Fire Fuel Pipeline
    {
        {
            vk_descriptor_layout_builder Builder = VkDescriptorLayoutBegin(&FluidSim->FireFuelDescLayout);
            VkDescriptorLayoutAdd(&Builder, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT);
            VkDescriptorLayoutAdd(&Builder, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT);
            VkDescriptorLayoutEnd(RenderState->Device, &Builder);
        }

        VkDescriptorSetLayout Layouts[] = 
            {
                FluidSim->GlobalDescLayout,
                FluidSim->FireFuelDescLayout,
            };
            
        FluidSim->FireFuelPipeline = VkPipelineComputeCreate(RenderState->Device, &RenderState->PipelineManager, &DemoState->TempArena,
                                                             "fluid_fire_burn_fuel.spv", "main", Layouts, ArrayCount(Layouts));
    }
        
    // NOTE: Fire Advection Pipeline
    {
        {
            vk_descriptor_layout_builder Builder = VkDescriptorLayoutBegin(&FluidSim->FireAdvectionDescLayout);
            VkDescriptorLayoutAdd(&Builder, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT);
            VkDescriptorLayoutAdd(&Builder, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT);
            VkDescriptorLayoutAdd(&Builder, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT);
            VkDescriptorLayoutEnd(RenderState->Device, &Builder);
        }

        VkDescriptorSetLayout Layouts[] = 
            {
                FluidSim->GlobalDescLayout,
                FluidSim->FireAdvectionDescLayout,
                FluidSim->PressureDescLayout,
            };
            
        FluidSim->FireAdvectionPipeline = VkPipelineComputeCreate(RenderState->Device, &RenderState->PipelineManager, &DemoState->TempArena,
                                                                  "fluid_fire_advection.spv", "main", Layouts, ArrayCount(Layouts));
    }

    // NOTE: Fire Pressure Apply Pipeline
    {
        {
            vk_descriptor_layout_builder Builder = VkDescriptorLayoutBegin(&FluidSim->FirePressureApplyDescLayout);
            VkDescriptorLayoutAdd(&Builder, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT);
            VkDescriptorLayoutAdd(&Builder, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT);
            VkDescriptorLayoutAdd(&Builder, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT);
            VkDescriptorLayoutAdd(&Builder, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT);
            VkDescriptorLayoutAdd(&Builder, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT);
            VkDescriptorLayoutAdd(&Builder, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT);
            VkDescriptorLayoutEnd(RenderState->Device, &Builder);
        }
        
        VkDescriptorSetLayout Layouts[] = 
            {
                FluidSim->GlobalDescLayout,
                FluidSim->FirePressureApplyDescLayout,
                FluidSim->PressureDescLayout,
            };
            
        FluidSim->FirePressureApplyPipeline = VkPipelineComputeCreate(RenderState->Device, &RenderState->PipelineManager, &DemoState->TempArena,
                                                                       "fluid_fire_pressure_apply.spv", "main", Layouts, ArrayCount(Layouts));
    }
    
    // NOTE: Populate descriptors
    {    
        // NOTE: Splat Descriptors (ping pong)
        {
            Result.SplatDescriptors[0] = VkDescriptorSetAllocate(RenderState->Device, RenderState->DescriptorPool, FluidSim->FireSplatDescLayout);
            VkDescriptorImageWrite(&RenderState->DescriptorManager, Result.SplatDescriptors[0], 0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                                   Result.TemperatureImages[0].View, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_GENERAL);
            VkDescriptorImageWrite(&RenderState->DescriptorManager, Result.SplatDescriptors[0], 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                                   FluidSim->VelocityImages[0].View, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_GENERAL);
            VkDescriptorImageWrite(&RenderState->DescriptorManager, Result.SplatDescriptors[0], 2, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                                   Result.TimerImages[0].View, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_GENERAL);

            Result.SplatDescriptors[1] = VkDescriptorSetAllocate(RenderState->Device, RenderState->DescriptorPool, FluidSim->FireSplatDescLayout);
            VkDescriptorImageWrite(&RenderState->DescriptorManager, Result.SplatDescriptors[1], 0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                                   Result.TemperatureImages[1].View, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_GENERAL);
            VkDescriptorImageWrite(&RenderState->DescriptorManager, Result.SplatDescriptors[1], 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                                   FluidSim->VelocityImages[1].View, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_GENERAL);
            VkDescriptorImageWrite(&RenderState->DescriptorManager, Result.SplatDescriptors[1], 2, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                                   Result.TimerImages[1].View, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_GENERAL);
        }

        // NOTE: Fuel Descriptor (ping pong)
        {
            Result.FuelDescriptors[0] = VkDescriptorSetAllocate(RenderState->Device, RenderState->DescriptorPool, FluidSim->FireFuelDescLayout);
            VkDescriptorImageWrite(&RenderState->DescriptorManager, Result.FuelDescriptors[0], 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                   Result.TimerImages[0].View, FluidSim->ClampLinearSampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
            VkDescriptorImageWrite(&RenderState->DescriptorManager, Result.FuelDescriptors[0], 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                                   Result.TemperatureImages[0].View, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_GENERAL);

            Result.FuelDescriptors[1] = VkDescriptorSetAllocate(RenderState->Device, RenderState->DescriptorPool, FluidSim->FireFuelDescLayout);
            VkDescriptorImageWrite(&RenderState->DescriptorManager, Result.FuelDescriptors[1], 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                   Result.TimerImages[1].View, FluidSim->ClampLinearSampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
            VkDescriptorImageWrite(&RenderState->DescriptorManager, Result.FuelDescriptors[1], 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                                   Result.TemperatureImages[1].View, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_GENERAL);
        }

        // NOTE: Advection Descriptor (ping pong)
        {
            Result.AdvectionDescriptors[0] = VkDescriptorSetAllocate(RenderState->Device, RenderState->DescriptorPool, FluidSim->FireAdvectionDescLayout);
            VkDescriptorImageWrite(&RenderState->DescriptorManager, Result.AdvectionDescriptors[0], 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                   FluidSim->VelocityImages[0].View, FluidSim->ClampLinearSampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
            VkDescriptorImageWrite(&RenderState->DescriptorManager, Result.AdvectionDescriptors[0], 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                                   FluidSim->VelocityImages[1].View, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_GENERAL);
            VkDescriptorImageWrite(&RenderState->DescriptorManager, Result.AdvectionDescriptors[0], 2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                   Result.TemperatureImages[0].View, FluidSim->ClampLinearSampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

            Result.AdvectionDescriptors[1] = VkDescriptorSetAllocate(RenderState->Device, RenderState->DescriptorPool, FluidSim->FireAdvectionDescLayout);
            VkDescriptorImageWrite(&RenderState->DescriptorManager, Result.AdvectionDescriptors[1], 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                   FluidSim->VelocityImages[1].View, FluidSim->ClampLinearSampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
            VkDescriptorImageWrite(&RenderState->DescriptorManager, Result.AdvectionDescriptors[1], 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                                   FluidSim->VelocityImages[0].View, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_GENERAL);
            VkDescriptorImageWrite(&RenderState->DescriptorManager, Result.AdvectionDescriptors[1], 2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                   Result.TemperatureImages[1].View, FluidSim->ClampLinearSampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        }

        // NOTE: Pressure Apply Descriptor
        {
            Result.PressureApplyDescriptors[0] = VkDescriptorSetAllocate(RenderState->Device, RenderState->DescriptorPool, FluidSim->FirePressureApplyDescLayout);
            VkDescriptorImageWrite(&RenderState->DescriptorManager, Result.PressureApplyDescriptors[0], 0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                                   FluidSim->VelocityImages[1].View, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_GENERAL);
            VkDescriptorImageWrite(&RenderState->DescriptorManager, Result.PressureApplyDescriptors[0], 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                   Result.TemperatureImages[0].View, FluidSim->ClampLinearSampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
            VkDescriptorImageWrite(&RenderState->DescriptorManager, Result.PressureApplyDescriptors[0], 2, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                                   Result.TemperatureImages[1].View, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_GENERAL);
            VkDescriptorImageWrite(&RenderState->DescriptorManager, Result.PressureApplyDescriptors[0], 3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                   Result.TimerImages[0].View, FluidSim->ClampLinearSampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
            VkDescriptorImageWrite(&RenderState->DescriptorManager, Result.PressureApplyDescriptors[0], 4, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                                   Result.TimerImages[1].View, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_GENERAL);
            VkDescriptorImageWrite(&RenderState->DescriptorManager, Result.PressureApplyDescriptors[0], 5, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                                   Result.ColorImages[1].View, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_GENERAL);

            Result.PressureApplyDescriptors[1] = VkDescriptorSetAllocate(RenderState->Device, RenderState->DescriptorPool, FluidSim->FirePressureApplyDescLayout);
            VkDescriptorImageWrite(&RenderState->DescriptorManager, Result.PressureApplyDescriptors[1], 0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                                   FluidSim->VelocityImages[0].View, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_GENERAL);
            VkDescriptorImageWrite(&RenderState->DescriptorManager, Result.PressureApplyDescriptors[1], 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                   Result.TemperatureImages[1].View, FluidSim->ClampLinearSampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
            VkDescriptorImageWrite(&RenderState->DescriptorManager, Result.PressureApplyDescriptors[1], 2, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                                   Result.TemperatureImages[0].View, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_GENERAL);
            VkDescriptorImageWrite(&RenderState->DescriptorManager, Result.PressureApplyDescriptors[1], 3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                   Result.TimerImages[1].View, FluidSim->ClampLinearSampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
            VkDescriptorImageWrite(&RenderState->DescriptorManager, Result.PressureApplyDescriptors[1], 4, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                                   Result.TimerImages[0].View, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_GENERAL);
            VkDescriptorImageWrite(&RenderState->DescriptorManager, Result.PressureApplyDescriptors[1], 5, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                                   Result.ColorImages[0].View, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_GENERAL);
        }
        
        // NOTE: Render Descriptors
        {
            Result.RenderDescriptors[0] = VkDescriptorSetAllocate(RenderState->Device, RenderState->DescriptorPool, RenderState->CopyImageDescLayout);
            VkDescriptorImageWrite(&RenderState->DescriptorManager, Result.RenderDescriptors[0], 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                   Result.ColorImages[0].View, DemoState->PointSampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
            
            Result.RenderDescriptors[1] = VkDescriptorSetAllocate(RenderState->Device, RenderState->DescriptorPool, RenderState->CopyImageDescLayout);
            VkDescriptorImageWrite(&RenderState->DescriptorManager, Result.RenderDescriptors[1], 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                   Result.ColorImages[1].View, DemoState->PointSampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        }

        VkDescriptorManagerFlush(RenderState->Device, &RenderState->DescriptorManager);
    }

    FluidSimInitBarriers(Commands, Result.TimerImages, 1);
    FluidSimInitBarriers(Commands, FluidSim->VelocityImages, 1);
    FluidSimInitBarriersUav(Commands, FluidSim->PressureImages, 1);
    FluidSimInitBarriers(Commands, Result.TemperatureImages, 1);
    FluidSimInitBarriers(Commands, Result.ColorImages, 1);
    VkBarrierManagerFlush(&RenderState->BarrierManager, Commands.Buffer);

    // NOTE: Clear all images
    {
        VkClearColorValue ClearValue = VkClearColorCreate(0, 0, 0, 0).color;
        VkClearColorValue ClearTemperatureValue = VkClearColorCreate(100, 0, 0, 0).color;
        VkImageSubresourceRange Range = {};
        Range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        Range.baseMipLevel = 0;
        Range.levelCount = 1;
        Range.baseArrayLayer = 0;
        Range.layerCount = 1;
        
        vkCmdClearColorImage(Commands.Buffer, Result.TimerImages[0].Image, VK_IMAGE_LAYOUT_GENERAL, &ClearValue, 1, &Range);
        vkCmdClearColorImage(Commands.Buffer, FluidSim->VelocityImages[0].Image, VK_IMAGE_LAYOUT_GENERAL, &ClearValue, 1, &Range);
        vkCmdClearColorImage(Commands.Buffer, FluidSim->PressureImages[0].Image, VK_IMAGE_LAYOUT_GENERAL, &ClearValue, 1, &Range);
        vkCmdClearColorImage(Commands.Buffer, Result.TemperatureImages[0].Image, VK_IMAGE_LAYOUT_GENERAL, &ClearTemperatureValue, 1, &Range);

        FluidSimSwapBarriers(Commands, Result.TimerImages, 1);
        FluidSimSwapBarriers(Commands, FluidSim->VelocityImages, 1);
        FluidSimSwapBarriersUav(Commands, FluidSim->PressureImages, 1);
        FluidSimSwapBarriers(Commands, Result.TemperatureImages, 1);
        VkBarrierManagerFlush(&RenderState->BarrierManager, Commands.Buffer);
    }

    return Result;
}

//
// NOTE: Fluid Sim Main
//

inline fluid_sim FluidSimCreate(fluid_sim_type Type, u32 Width, u32 Height, VkRenderPass RenderPass, u32 SubPassId)
{
    fluid_sim Result = {};

    Result.Type = Type;
    Result.InputId = 0;
    Result.Width = Width;
    Result.Height = Height;
    Result.Arena = VkLinearArenaCreate(RenderState->Device, RenderState->LocalMemoryId, MegaBytes(64));
    Result.UniformBuffer = VkBufferCreate(RenderState->Device, &Result.Arena, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                          sizeof(fluid_sim_inputs));

    VkImageUsageFlags ImageFlags = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;

    Result.VelocityImages[0] = VkImageCreate(RenderState->Device, &Result.Arena, Width, Height, VK_FORMAT_R32G32_SFLOAT,
                                             ImageFlags, VK_IMAGE_ASPECT_COLOR_BIT);
    Result.VelocityImages[1] = VkImageCreate(RenderState->Device, &Result.Arena, Width, Height, VK_FORMAT_R32G32_SFLOAT,
                                             ImageFlags, VK_IMAGE_ASPECT_COLOR_BIT);

    Result.DivergenceImage = VkImageCreate(RenderState->Device, &Result.Arena, Width, Height, VK_FORMAT_R32_SFLOAT,
                                          ImageFlags, VK_IMAGE_ASPECT_COLOR_BIT);
    Result.PressureImages[0] = VkImageCreate(RenderState->Device, &Result.Arena, Width, Height, VK_FORMAT_R32_SFLOAT,
                                             ImageFlags, VK_IMAGE_ASPECT_COLOR_BIT);
    Result.PressureImages[1] = VkImageCreate(RenderState->Device, &Result.Arena, Width, Height, VK_FORMAT_R32_SFLOAT,
                                             ImageFlags, VK_IMAGE_ASPECT_COLOR_BIT);

    Result.ClampPointSampler = VkSamplerCreate(RenderState->Device, VK_FILTER_NEAREST, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER, VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK, 0.0f);
    Result.ClampLinearSampler = VkSamplerCreate(RenderState->Device, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER, VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK, 0.0f);
    
    Result.MirrorPointSampler = VkSamplerCreate(RenderState->Device, VK_FILTER_NEAREST, VK_SAMPLER_ADDRESS_MODE_REPEAT, VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK, 0.0f);
    Result.MirrorLinearSampler = VkSamplerCreate(RenderState->Device, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_REPEAT, VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK, 0.0f);

    {
        vk_descriptor_layout_builder Builder = VkDescriptorLayoutBegin(&Result.GlobalDescLayout);
        VkDescriptorLayoutAdd(&Builder, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT);
        VkDescriptorLayoutAdd(&Builder, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT);
        VkDescriptorLayoutEnd(RenderState->Device, &Builder);
    }

    // NOTE: Divergence Pipelines
    {
        {
            vk_descriptor_layout_builder Builder = VkDescriptorLayoutBegin(&Result.DivergenceDescLayout);
            VkDescriptorLayoutAdd(&Builder, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT);
            VkDescriptorLayoutEnd(RenderState->Device, &Builder);
        }

        {
            VkDescriptorSetLayout Layouts[] = 
                {
                    Result.GlobalDescLayout,
                    Result.DivergenceDescLayout,
                };
            
            Result.DivergencePipeline = VkPipelineComputeCreate(RenderState->Device, &RenderState->PipelineManager, &DemoState->TempArena,
                                                                "fluid_divergence.spv", "main", Layouts, ArrayCount(Layouts));
        }
    }

    // NOTE: Pressure Iteration Pipeline
    {
        {
            vk_descriptor_layout_builder Builder = VkDescriptorLayoutBegin(&Result.PressureDescLayout);
            VkDescriptorLayoutAdd(&Builder, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT);
            VkDescriptorLayoutAdd(&Builder, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT);
            VkDescriptorLayoutEnd(RenderState->Device, &Builder);
        }

        VkDescriptorSetLayout Layouts[] = 
            {
                Result.GlobalDescLayout,
                Result.PressureDescLayout, // NOTE: This is a null descriptor layout really
                Result.PressureDescLayout,
            };
            
        Result.PressureMirrorIterationPipeline = VkPipelineComputeCreate(RenderState->Device, &RenderState->PipelineManager, &DemoState->TempArena,
                                                                         "fluid_pressure_mirror_iteration.spv", "main", Layouts, ArrayCount(Layouts));
            
        Result.PressureClampIterationPipeline = VkPipelineComputeCreate(RenderState->Device, &RenderState->PipelineManager, &DemoState->TempArena,
                                                                        "fluid_pressure_clamp_iteration.spv", "main", Layouts, ArrayCount(Layouts));
    }

    // NOTE: Render Pipeline
    {
        Result.CopyToRtPipeline = FullScreenCopyImageCreate(RenderPass, SubPassId);
    }

    // NOTE: Populate descirptors
    {
        // NOTE: Global Descriptor
        {
            Result.GlobalDescriptor = VkDescriptorSetAllocate(RenderState->Device, RenderState->DescriptorPool, Result.GlobalDescLayout);
            VkDescriptorBufferWrite(&RenderState->DescriptorManager, Result.GlobalDescriptor, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, Result.UniformBuffer);
            VkDescriptorImageWrite(&RenderState->DescriptorManager, Result.GlobalDescriptor, 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                                   Result.DivergenceImage.View, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_GENERAL);
        }

        // NOTE: Divergence Descriptor (ping pong)
        {
            VkSampler Sampler = Type == FluidSimType_Diffusion ? Result.MirrorPointSampler : Result.ClampPointSampler;
            
            Result.DivergenceDescriptors[0] = VkDescriptorSetAllocate(RenderState->Device, RenderState->DescriptorPool, Result.DivergenceDescLayout);
            VkDescriptorImageWrite(&RenderState->DescriptorManager, Result.DivergenceDescriptors[0], 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                   Result.VelocityImages[1].View, Sampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

            Result.DivergenceDescriptors[1] = VkDescriptorSetAllocate(RenderState->Device, RenderState->DescriptorPool, Result.DivergenceDescLayout);
            VkDescriptorImageWrite(&RenderState->DescriptorManager, Result.DivergenceDescriptors[1], 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                   Result.VelocityImages[0].View, Sampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        }

        // NOTE: Pressure Descriptor
        {
            Result.PressureDescriptors[0] = VkDescriptorSetAllocate(RenderState->Device, RenderState->DescriptorPool, Result.PressureDescLayout);
            VkDescriptorImageWrite(&RenderState->DescriptorManager, Result.PressureDescriptors[0], 0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                                   Result.PressureImages[0].View, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_GENERAL);
            VkDescriptorImageWrite(&RenderState->DescriptorManager, Result.PressureDescriptors[0], 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                                   Result.PressureImages[1].View, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_GENERAL);

            Result.PressureDescriptors[1] = VkDescriptorSetAllocate(RenderState->Device, RenderState->DescriptorPool, Result.PressureDescLayout);
            VkDescriptorImageWrite(&RenderState->DescriptorManager, Result.PressureDescriptors[1], 0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                                   Result.PressureImages[1].View, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_GENERAL);
            VkDescriptorImageWrite(&RenderState->DescriptorManager, Result.PressureDescriptors[1], 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                                   Result.PressureImages[0].View, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_GENERAL);
        }
    }
    
    VkDescriptorManagerFlush(RenderState->Device, &RenderState->DescriptorManager);
    
    return Result;
}

inline void FluidSimSwap(vk_commands Commands, fluid_sim* FluidSim)
{
    // NOTE: Convert input images to output
    VkPipelineStageFlags StageFlags = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;

    switch (FluidSim->Type)
    {
        case FluidSimType_Diffusion:
        {
            diffusion_sim* DiffusionSim = &FluidSim->DiffusionSim;
            
            FluidSimSwapBarriers(Commands, DiffusionSim->ColorImages, FluidSim->InputId);
        } break;

        case FluidSimType_Smoke:
        {
            smoke_sim* SmokeSim = &FluidSim->SmokeSim;

            FluidSimSwapBarriers(Commands, SmokeSim->ColorImages, FluidSim->InputId);
            FluidSimSwapBarriers(Commands, SmokeSim->TemperatureImages, FluidSim->InputId);
        } break;

        case FluidSimType_Fire:
        {
            fire_sim* FireSim = &FluidSim->FireSim;

            FluidSimSwapBarriers(Commands, FireSim->TimerImages, FluidSim->InputId);
            FluidSimSwapBarriers(Commands, FireSim->TemperatureImages, FluidSim->InputId);
            FluidSimSwapBarriers(Commands, FireSim->ColorImages, FluidSim->InputId);
        } break;
    }
    
    // NOTE: Swap Velocities
    FluidSimSwapBarriers(Commands, FluidSim->VelocityImages, FluidSim->InputId);
    FluidSim->InputId = !FluidSim->InputId;

    VkBarrierManagerFlush(&RenderState->BarrierManager, Commands.Buffer);
}

inline void FluidSimInit(vk_commands Commands, fluid_sim* FluidSim)
{
    switch (FluidSim->Type)
    {
        case FluidSimType_Diffusion:
        {
            FluidSim->DiffusionSim = DiffusionInit(Commands, FluidSim);
        } break;

        case FluidSimType_Smoke:
        {
            FluidSim->SmokeSim = SmokeInit(Commands, FluidSim);
        } break;

        case FluidSimType_Fire:
        {
            FluidSim->FireSim = FireInit(Commands, FluidSim);
        } break;
    }
        
    // NOTE: Init divergence image
    VkBarrierImageAdd(&RenderState->BarrierManager, VK_ACCESS_MEMORY_READ_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                      VK_IMAGE_LAYOUT_UNDEFINED, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                      VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_ASPECT_COLOR_BIT, FluidSim->DivergenceImage.Image);
    
    VkBarrierManagerFlush(&RenderState->BarrierManager, Commands.Buffer);
}

inline void FluidSimFrameBegin(fluid_sim* FluidSim, f32 FrameTime, v2 MousePos, v2 PrevMousePos)
{
    {
        char Text[512];
        snprintf(Text, sizeof(Text), "Mouse: %f, %f\n", MousePos.x, MousePos.y);
        OutputDebugStringA(Text);
    }
    
    fluid_sim_inputs* CpuUniforms = &FluidSim->UniformsCpu;
    
    CpuUniforms->MousePos = MousePos;
    CpuUniforms->DeltaMousePos = MousePos - PrevMousePos;
    CpuUniforms->FrameTime = FrameTime;
    // TODO: Make this configurable
    CpuUniforms->Density = 1;
    Assert(FluidSim->Width == FluidSim->Height);
    CpuUniforms->Epsilon = 1.0f / f32(FluidSim->Width);
    CpuUniforms->Dim = FluidSim->Width;

    // NOTE: Smoke data
    //CpuUniforms->RoomTemperature = 1.0f;
    CpuUniforms->RoomTemperature = 100.0f;
    CpuUniforms->Gravity = 9.81f;
    //CpuUniforms->MolarMass = 53.4915f; // NOTE: https://www.webqc.org/molecular-weight-of-NH4Cl%28smoke%29.html
    CpuUniforms->MolarMass = 332.1099f; // NOTE: https://www.webqc.org/molecular-weight-of-FIRe.html
    CpuUniforms->R = 8.3145f; // NOTE: https://en.wikipedia.org/wiki/Gas_constant
    
    // NOTE: Splat data
    CpuUniforms->SplatCenter = V2(0.5f, 0.1f);
    CpuUniforms->SplatRadius = 0.02f;
    
    fluid_sim_inputs* GpuUniforms = VkTransferPushWriteStruct(&RenderState->TransferManager, FluidSim->UniformBuffer, fluid_sim_inputs,
                                                              BarrierMask(VkAccessFlagBits(0), VK_PIPELINE_STAGE_ALL_COMMANDS_BIT),
                                                              BarrierMask(VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT));
    Copy(&FluidSim->UniformsCpu, GpuUniforms, sizeof(fluid_sim_inputs));
}

inline void FluidSimSimulate(vk_commands Commands, fluid_sim* FluidSim)
{
    u32 DispatchX = CeilU32(f32(FluidSim->Width) / 8.0f);
    u32 DispatchY = CeilU32(f32(FluidSim->Height) / 8.0f);
    
    // NOTE: Splats
    switch (FluidSim->Type)
    {
        case FluidSimType_Smoke:
        {
            smoke_sim* SmokeSim = &FluidSim->SmokeSim;
            VkDescriptorSet DescriptorSets[] =
                {
                    FluidSim->GlobalDescriptor,
                    SmokeSim->SplatDescriptors[FluidSim->InputId],
                };

            VkBarrierImageAdd(&RenderState->BarrierManager,
                              VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                              VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_IMAGE_LAYOUT_GENERAL,
                              VK_IMAGE_ASPECT_COLOR_BIT, SmokeSim->TemperatureImages[FluidSim->InputId].Image);
            VkBarrierImageAdd(&RenderState->BarrierManager,
                              VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                              VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_IMAGE_LAYOUT_GENERAL,
                              VK_IMAGE_ASPECT_COLOR_BIT, SmokeSim->ColorImages[FluidSim->InputId].Image);
            VkBarrierImageAdd(&RenderState->BarrierManager,
                              VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                              VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_IMAGE_LAYOUT_GENERAL,
                              VK_IMAGE_ASPECT_COLOR_BIT, FluidSim->VelocityImages[FluidSim->InputId].Image);
            VkBarrierManagerFlush(&RenderState->BarrierManager, Commands.Buffer);

            local_global u32 FrameId = 0;

            //if ((FrameId % 20) == 0)
            {
                VkComputeDispatch(Commands, FluidSim->SmokeSplatPipeline, DescriptorSets, ArrayCount(DescriptorSets), DispatchX, DispatchY, 1);
            }
                FrameId += 1;
            
            VkBarrierImageAdd(&RenderState->BarrierManager,
                              VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_IMAGE_LAYOUT_GENERAL,
                              VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                              VK_IMAGE_ASPECT_COLOR_BIT, SmokeSim->TemperatureImages[FluidSim->InputId].Image);
            VkBarrierImageAdd(&RenderState->BarrierManager,
                              VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_IMAGE_LAYOUT_GENERAL,
                              VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                              VK_IMAGE_ASPECT_COLOR_BIT, SmokeSim->ColorImages[FluidSim->InputId].Image);
            VkBarrierImageAdd(&RenderState->BarrierManager,
                              VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_IMAGE_LAYOUT_GENERAL,
                              VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                              VK_IMAGE_ASPECT_COLOR_BIT, FluidSim->VelocityImages[FluidSim->InputId].Image);
            VkBarrierManagerFlush(&RenderState->BarrierManager, Commands.Buffer);
        } break;

        case FluidSimType_Fire:
        {
            fire_sim* FireSim = &FluidSim->FireSim;

            VkBarrierImageAdd(&RenderState->BarrierManager,
                              VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                              VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_IMAGE_LAYOUT_GENERAL,
                              VK_IMAGE_ASPECT_COLOR_BIT, FireSim->TemperatureImages[FluidSim->InputId].Image);
            VkBarrierImageAdd(&RenderState->BarrierManager,
                              VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                              VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_IMAGE_LAYOUT_GENERAL,
                              VK_IMAGE_ASPECT_COLOR_BIT, FireSim->TimerImages[FluidSim->InputId].Image);
            VkBarrierImageAdd(&RenderState->BarrierManager,
                              VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                              VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_IMAGE_LAYOUT_GENERAL,
                              VK_IMAGE_ASPECT_COLOR_BIT, FluidSim->VelocityImages[FluidSim->InputId].Image);
            VkBarrierManagerFlush(&RenderState->BarrierManager, Commands.Buffer);

            local_global u32 FrameId = 0;

            //if (FrameId == 0)
            {
                VkDescriptorSet DescriptorSets[] =
                    {
                        FluidSim->GlobalDescriptor,
                        FireSim->SplatDescriptors[FluidSim->InputId],
                    };

                VkComputeDispatch(Commands, FluidSim->FireSplatPipeline, DescriptorSets, ArrayCount(DescriptorSets), DispatchX, DispatchY, 1);
                FrameId = 1;
            }
            
            VkBarrierImageAdd(&RenderState->BarrierManager,
                              VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_IMAGE_LAYOUT_GENERAL,
                              VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_IMAGE_LAYOUT_GENERAL,
                              VK_IMAGE_ASPECT_COLOR_BIT, FireSim->TemperatureImages[FluidSim->InputId].Image);
            VkBarrierImageAdd(&RenderState->BarrierManager,
                              VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_IMAGE_LAYOUT_GENERAL,
                              VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                              VK_IMAGE_ASPECT_COLOR_BIT, FireSim->TimerImages[FluidSim->InputId].Image);
            VkBarrierImageAdd(&RenderState->BarrierManager,
                              VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_IMAGE_LAYOUT_GENERAL,
                              VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                              VK_IMAGE_ASPECT_COLOR_BIT, FluidSim->VelocityImages[FluidSim->InputId].Image);
            VkBarrierManagerFlush(&RenderState->BarrierManager, Commands.Buffer);

            // NOTE: Burn Fuel
            {
                VkDescriptorSet DescriptorSets[] =
                    {
                        FluidSim->GlobalDescriptor,
                        FireSim->FuelDescriptors[FluidSim->InputId],
                    };

                VkComputeDispatch(Commands, FluidSim->FireFuelPipeline, DescriptorSets, ArrayCount(DescriptorSets), DispatchX, DispatchY, 1);
            }
            
            VkBarrierImageAdd(&RenderState->BarrierManager,
                              VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_IMAGE_LAYOUT_GENERAL,
                              VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                              VK_IMAGE_ASPECT_COLOR_BIT, FireSim->TemperatureImages[FluidSim->InputId].Image);
            VkBarrierManagerFlush(&RenderState->BarrierManager, Commands.Buffer);
        } break;
    }

    // NOTE: Advection
    switch (FluidSim->Type)
    {
        case FluidSimType_Diffusion:
        {
            diffusion_sim* DiffusionSim = &FluidSim->DiffusionSim;
            VkDescriptorSet DescriptorSets[] =
                {
                    FluidSim->GlobalDescriptor,
                    DiffusionSim->AdvectionDescriptors[FluidSim->InputId],
                };

            VkComputeDispatch(Commands, FluidSim->DiffusionAdvectionPipeline, DescriptorSets, ArrayCount(DescriptorSets), DispatchX, DispatchY, 1);
        } break;

        case FluidSimType_Smoke:
        {
            smoke_sim* SmokeSim = &FluidSim->SmokeSim;
            VkDescriptorSet DescriptorSets[] =
                {
                    FluidSim->GlobalDescriptor,
                    SmokeSim->AdvectionDescriptors[FluidSim->InputId],
                    FluidSim->PressureDescriptors[FluidSim->PressureInputId],
                };

            VkComputeDispatch(Commands, FluidSim->SmokeAdvectionPipeline, DescriptorSets, ArrayCount(DescriptorSets), DispatchX, DispatchY, 1);
        } break;

        case FluidSimType_Fire:
        {
            fire_sim* FireSim = &FluidSim->FireSim;
            VkDescriptorSet DescriptorSets[] =
                {
                    FluidSim->GlobalDescriptor,
                    FireSim->AdvectionDescriptors[FluidSim->InputId],
                    FluidSim->PressureDescriptors[FluidSim->PressureInputId],
                };

            VkComputeDispatch(Commands, FluidSim->FireAdvectionPipeline, DescriptorSets, ArrayCount(DescriptorSets), DispatchX, DispatchY, 1);
        } break;
    }
    
    VkBarrierImageAdd(&RenderState->BarrierManager, VK_ACCESS_SHADER_WRITE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                      VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                      VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT, FluidSim->VelocityImages[!FluidSim->InputId].Image);
    VkBarrierManagerFlush(&RenderState->BarrierManager, Commands.Buffer);
    
    // NOTE: Divergence
    {
        VkDescriptorSet DescriptorSets[] =
        {
            FluidSim->GlobalDescriptor,
            FluidSim->DivergenceDescriptors[FluidSim->InputId],
        };

        VkComputeDispatch(Commands, FluidSim->DivergencePipeline, DescriptorSets, ArrayCount(DescriptorSets), DispatchX, DispatchY, 1);
    }
    
    VkBarrierImageAdd(&RenderState->BarrierManager, VK_ACCESS_SHADER_WRITE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                      VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                      VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_ASPECT_COLOR_BIT, FluidSim->DivergenceImage.Image);
    VkBarrierManagerFlush(&RenderState->BarrierManager, Commands.Buffer);

    // NOTE: Pressure Iteration
    for (u32 IterationId = 0; IterationId < 20; ++IterationId)
    {
        VkDescriptorSet DescriptorSets[] =
        {
            FluidSim->GlobalDescriptor,
            FluidSim->PressureDescriptors[FluidSim->PressureInputId], // NOTE: This is really a null entry
            FluidSim->PressureDescriptors[FluidSim->PressureInputId],
        };

        switch (FluidSim->Type)
        {
            case FluidSimType_Diffusion:
            {
                VkComputeDispatch(Commands, FluidSim->PressureMirrorIterationPipeline, DescriptorSets, ArrayCount(DescriptorSets), DispatchX, DispatchY, 1);
            } break;

            default:
            {
                VkComputeDispatch(Commands, FluidSim->PressureClampIterationPipeline, DescriptorSets, ArrayCount(DescriptorSets), DispatchX, DispatchY, 1);
            } break;
        }
        
        FluidSimSwapBarriersUav(Commands, FluidSim->PressureImages, FluidSim->PressureInputId);
        FluidSim->PressureInputId = !FluidSim->PressureInputId;
        VkBarrierManagerFlush(&RenderState->BarrierManager, Commands.Buffer);
    }
    
    VkBarrierImageAdd(&RenderState->BarrierManager, VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                      VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                      VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_ASPECT_COLOR_BIT, FluidSim->VelocityImages[!FluidSim->InputId].Image);
    VkBarrierManagerFlush(&RenderState->BarrierManager, Commands.Buffer);
    
    // NOTE: Pressure Apply and Advect Color
    switch (FluidSim->Type)
    {
        case FluidSimType_Diffusion:
        {
            diffusion_sim* DiffusionSim = &FluidSim->DiffusionSim;
            VkDescriptorSet DescriptorSets[] =
                {
                    FluidSim->GlobalDescriptor,
                    DiffusionSim->PressureApplyDescriptors[FluidSim->InputId],
                    FluidSim->PressureDescriptors[FluidSim->PressureInputId],
                };
            VkComputeDispatch(Commands, FluidSim->DiffusionPressureApplyPipeline, DescriptorSets, ArrayCount(DescriptorSets), DispatchX, DispatchY, 1);
        } break;

        case FluidSimType_Smoke:
        {
            smoke_sim* SmokeSim = &FluidSim->SmokeSim;
            VkDescriptorSet DescriptorSets[] =
                {
                    FluidSim->GlobalDescriptor,
                    SmokeSim->PressureApplyDescriptors[FluidSim->InputId],
                    FluidSim->PressureDescriptors[FluidSim->PressureInputId],
                };
            VkComputeDispatch(Commands, FluidSim->SmokePressureApplyPipeline, DescriptorSets, ArrayCount(DescriptorSets), DispatchX, DispatchY, 1);
        } break;

        case FluidSimType_Fire:
        {
            fire_sim* FireSim = &FluidSim->FireSim;
            VkDescriptorSet DescriptorSets[] =
                {
                    FluidSim->GlobalDescriptor,
                    FireSim->PressureApplyDescriptors[FluidSim->InputId],
                    FluidSim->PressureDescriptors[FluidSim->PressureInputId],
                };
            VkComputeDispatch(Commands, FluidSim->FirePressureApplyPipeline, DescriptorSets, ArrayCount(DescriptorSets), DispatchX, DispatchY, 1);
        } break;
    }
    
    // NOTE: Convert to default states
    VkBarrierImageAdd(&RenderState->BarrierManager, VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                      VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_SHADER_WRITE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                      VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_ASPECT_COLOR_BIT, FluidSim->DivergenceImage.Image);
    VkBarrierManagerFlush(&RenderState->BarrierManager, Commands.Buffer);

    FluidSimSwap(Commands, FluidSim);
}
