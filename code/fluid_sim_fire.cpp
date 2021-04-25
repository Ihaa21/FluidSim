
/*

  NOTE: https://github.com/andrewkchan/fire-simulation
  
*/

inline void FireResize(fire_sim* Fire, vk_commands* Commands, u32 Width, u32 Height)
{
    Fire->Width = Width;
    Fire->Height = Height;
    Fire->InputId = 0;
    Fire->PressureInputId = 0;
    
    Assert(Width == Height);
    Fire->Inputs.Epsilon = 1.0f / f32(Width);
    Fire->Inputs.Dim = Width;

    VkImageUsageFlags ImageFlags = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    
    Fire->ColorImages[0] = VkImageCreate(RenderState->Device, &DemoState->GpuArena, Width, Height, VK_FORMAT_R32G32B32A32_SFLOAT,
                                         ImageFlags, VK_IMAGE_ASPECT_COLOR_BIT);
    Fire->ColorImages[1] = VkImageCreate(RenderState->Device, &DemoState->GpuArena, Width, Height, VK_FORMAT_R32G32B32A32_SFLOAT,
                                         ImageFlags, VK_IMAGE_ASPECT_COLOR_BIT);
    Fire->TimerImages[0] = VkImageCreate(RenderState->Device, &DemoState->GpuArena, Width, Height, VK_FORMAT_R32_SFLOAT,
                                         ImageFlags, VK_IMAGE_ASPECT_COLOR_BIT);
    Fire->TimerImages[1] = VkImageCreate(RenderState->Device, &DemoState->GpuArena, Width, Height, VK_FORMAT_R32_SFLOAT,
                                         ImageFlags, VK_IMAGE_ASPECT_COLOR_BIT);
    Fire->TemperatureImages[0] = VkImageCreate(RenderState->Device, &DemoState->GpuArena, Width, Height, VK_FORMAT_R32_SFLOAT,
                                               ImageFlags, VK_IMAGE_ASPECT_COLOR_BIT);
    Fire->TemperatureImages[1] = VkImageCreate(RenderState->Device, &DemoState->GpuArena, Width, Height, VK_FORMAT_R32_SFLOAT,
                                               ImageFlags, VK_IMAGE_ASPECT_COLOR_BIT);
    Fire->VelocityImages[0] = VkImageCreate(RenderState->Device, &DemoState->GpuArena, Width, Height, VK_FORMAT_R32G32_SFLOAT,
                                            ImageFlags, VK_IMAGE_ASPECT_COLOR_BIT);
    Fire->VelocityImages[1] = VkImageCreate(RenderState->Device, &DemoState->GpuArena, Width, Height, VK_FORMAT_R32G32_SFLOAT,
                                            ImageFlags, VK_IMAGE_ASPECT_COLOR_BIT);
    Fire->DivergenceImage = VkImageCreate(RenderState->Device, &DemoState->GpuArena, Width, Height, VK_FORMAT_R32_SFLOAT,
                                          ImageFlags, VK_IMAGE_ASPECT_COLOR_BIT);
    Fire->PressureImages[0] = VkImageCreate(RenderState->Device, &DemoState->GpuArena, Width, Height, VK_FORMAT_R32_SFLOAT,
                                            ImageFlags, VK_IMAGE_ASPECT_COLOR_BIT);
    Fire->PressureImages[1] = VkImageCreate(RenderState->Device, &DemoState->GpuArena, Width, Height, VK_FORMAT_R32_SFLOAT,
                                            ImageFlags, VK_IMAGE_ASPECT_COLOR_BIT);

    // NOTE: Global Descriptor
    {
        VkDescriptorImageWrite(&RenderState->DescriptorManager, Fire->GlobalDescriptor, 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                               Fire->DivergenceImage.View, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_GENERAL);
    }
            
    // NOTE: Splat Descriptors (ping pong)
    {
        VkDescriptorImageWrite(&RenderState->DescriptorManager, Fire->SplatDescriptors[0], 0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                               Fire->TemperatureImages[0].View, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_GENERAL);
        VkDescriptorImageWrite(&RenderState->DescriptorManager, Fire->SplatDescriptors[0], 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                               Fire->VelocityImages[0].View, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_GENERAL);
        VkDescriptorImageWrite(&RenderState->DescriptorManager, Fire->SplatDescriptors[0], 2, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                               Fire->TimerImages[0].View, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_GENERAL);

        VkDescriptorImageWrite(&RenderState->DescriptorManager, Fire->SplatDescriptors[1], 0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                               Fire->TemperatureImages[1].View, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_GENERAL);
        VkDescriptorImageWrite(&RenderState->DescriptorManager, Fire->SplatDescriptors[1], 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                               Fire->VelocityImages[1].View, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_GENERAL);
        VkDescriptorImageWrite(&RenderState->DescriptorManager, Fire->SplatDescriptors[1], 2, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                               Fire->TimerImages[1].View, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_GENERAL);
    }

    // NOTE: Advection Descriptor (ping pong)
    {
        VkDescriptorImageWrite(&RenderState->DescriptorManager, Fire->AdvectionDescriptors[0], 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                               Fire->VelocityImages[0].View, DemoState->ClampLinearSampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        VkDescriptorImageWrite(&RenderState->DescriptorManager, Fire->AdvectionDescriptors[0], 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                               Fire->VelocityImages[1].View, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_GENERAL);
        VkDescriptorImageWrite(&RenderState->DescriptorManager, Fire->AdvectionDescriptors[0], 2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                               Fire->TemperatureImages[0].View, DemoState->ClampLinearSampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

        VkDescriptorImageWrite(&RenderState->DescriptorManager, Fire->AdvectionDescriptors[1], 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                               Fire->VelocityImages[1].View, DemoState->ClampLinearSampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        VkDescriptorImageWrite(&RenderState->DescriptorManager, Fire->AdvectionDescriptors[1], 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                               Fire->VelocityImages[0].View, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_GENERAL);
        VkDescriptorImageWrite(&RenderState->DescriptorManager, Fire->AdvectionDescriptors[1], 2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                               Fire->TemperatureImages[1].View, DemoState->ClampLinearSampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    }

    // NOTE: Divergence Descriptor (ping pong)
    {
        VkDescriptorImageWrite(&RenderState->DescriptorManager, Fire->DivergenceDescriptors[0], 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                               Fire->VelocityImages[1].View, DemoState->MirrorLinearSampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

        VkDescriptorImageWrite(&RenderState->DescriptorManager, Fire->DivergenceDescriptors[1], 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                               Fire->VelocityImages[0].View, DemoState->MirrorLinearSampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    }

    // NOTE: Pressure Descriptor (ping pong)
    {
        VkDescriptorImageWrite(&RenderState->DescriptorManager, Fire->PressureDescriptors[0], 0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                               Fire->PressureImages[0].View, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_GENERAL);
        VkDescriptorImageWrite(&RenderState->DescriptorManager, Fire->PressureDescriptors[0], 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                               Fire->PressureImages[1].View, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_GENERAL);

        VkDescriptorImageWrite(&RenderState->DescriptorManager, Fire->PressureDescriptors[1], 0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                               Fire->PressureImages[1].View, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_GENERAL);
        VkDescriptorImageWrite(&RenderState->DescriptorManager, Fire->PressureDescriptors[1], 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                               Fire->PressureImages[0].View, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_GENERAL);
    }

    // NOTE: Pressure Apply Descriptor
    {
        VkDescriptorImageWrite(&RenderState->DescriptorManager, Fire->PressureApplyDescriptors[0], 0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                               Fire->VelocityImages[1].View, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_GENERAL);
        VkDescriptorImageWrite(&RenderState->DescriptorManager, Fire->PressureApplyDescriptors[0], 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                               Fire->TemperatureImages[0].View, DemoState->ClampLinearSampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        VkDescriptorImageWrite(&RenderState->DescriptorManager, Fire->PressureApplyDescriptors[0], 2, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                               Fire->TemperatureImages[1].View, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_GENERAL);
        VkDescriptorImageWrite(&RenderState->DescriptorManager, Fire->PressureApplyDescriptors[0], 3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                               Fire->TimerImages[0].View, DemoState->ClampLinearSampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        VkDescriptorImageWrite(&RenderState->DescriptorManager, Fire->PressureApplyDescriptors[0], 4, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                               Fire->TimerImages[1].View, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_GENERAL);
        VkDescriptorImageWrite(&RenderState->DescriptorManager, Fire->PressureApplyDescriptors[0], 5, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                               Fire->ColorImages[1].View, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_GENERAL);

        VkDescriptorImageWrite(&RenderState->DescriptorManager, Fire->PressureApplyDescriptors[1], 0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                               Fire->VelocityImages[0].View, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_GENERAL);
        VkDescriptorImageWrite(&RenderState->DescriptorManager, Fire->PressureApplyDescriptors[1], 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                               Fire->TemperatureImages[1].View, DemoState->ClampLinearSampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        VkDescriptorImageWrite(&RenderState->DescriptorManager, Fire->PressureApplyDescriptors[1], 2, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                               Fire->TemperatureImages[0].View, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_GENERAL);
        VkDescriptorImageWrite(&RenderState->DescriptorManager, Fire->PressureApplyDescriptors[1], 3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                               Fire->TimerImages[1].View, DemoState->ClampLinearSampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        VkDescriptorImageWrite(&RenderState->DescriptorManager, Fire->PressureApplyDescriptors[1], 4, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                               Fire->TimerImages[0].View, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_GENERAL);
        VkDescriptorImageWrite(&RenderState->DescriptorManager, Fire->PressureApplyDescriptors[1], 5, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                               Fire->ColorImages[0].View, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_GENERAL);
    }
        
    // NOTE: Render Descriptors
    {
        VkDescriptorImageWrite(&RenderState->DescriptorManager, Fire->RenderDescriptors[0], 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                               Fire->ColorImages[0].View, DemoState->PointSampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
            
        VkDescriptorImageWrite(&RenderState->DescriptorManager, Fire->RenderDescriptors[1], 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                               Fire->ColorImages[1].View, DemoState->PointSampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    }

    VkDescriptorManagerFlush(RenderState->Device, &RenderState->DescriptorManager);

    FluidSimInitBarriers(Commands, Fire->TimerImages, 1);
    FluidSimInitBarriers(Commands, Fire->VelocityImages, 1);
    FluidSimInitBarriersUav(Commands, Fire->PressureImages, 1);
    FluidSimInitBarriers(Commands, Fire->TemperatureImages, 1);
    FluidSimInitBarriers(Commands, Fire->ColorImages, 1);
    VkBarrierImageAdd(Commands, Fire->DivergenceImage.Image, VK_IMAGE_ASPECT_COLOR_BIT, 
                      VK_ACCESS_MEMORY_READ_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
                      VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_IMAGE_LAYOUT_GENERAL);
    VkCommandsBarrierFlush(Commands);

    // NOTE: Clear all images
    {
        FireSetInputs(Fire, Commands, 0, V2(0), V2(0));
        VkCommandsTransferFlush(Commands, RenderState->Device);

        VkClearColorValue ClearValue = VkClearColorCreate(0, 0, 0, 0).color;
        VkClearColorValue ClearTemperatureValue = VkClearColorCreate(100, 0, 0, 0).color;
        VkImageSubresourceRange Range = {};
        Range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        Range.baseMipLevel = 0;
        Range.levelCount = 1;
        Range.baseArrayLayer = 0;
        Range.layerCount = 1;
        
        vkCmdClearColorImage(Commands->Buffer, Fire->TimerImages[0].Image, VK_IMAGE_LAYOUT_GENERAL, &ClearValue, 1, &Range);
        vkCmdClearColorImage(Commands->Buffer, Fire->VelocityImages[0].Image, VK_IMAGE_LAYOUT_GENERAL, &ClearValue, 1, &Range);
        vkCmdClearColorImage(Commands->Buffer, Fire->PressureImages[0].Image, VK_IMAGE_LAYOUT_GENERAL, &ClearValue, 1, &Range);
        vkCmdClearColorImage(Commands->Buffer, Fire->TemperatureImages[0].Image, VK_IMAGE_LAYOUT_GENERAL, &ClearTemperatureValue, 1, &Range);

        FluidSimSwapBarriers(Commands, Fire->TimerImages, 1);
        FluidSimSwapBarriers(Commands, Fire->VelocityImages, 1);
        FluidSimSwapBarriersUav(Commands, Fire->PressureImages, 1);
        FluidSimSwapBarriers(Commands, Fire->TemperatureImages, 1);
        VkCommandsBarrierFlush(Commands);
    }
}

inline fire_sim FireInit(vk_commands* Commands, u32 Width, u32 Height)
{
    fire_sim Result = {};

    Result.UniformBuffer = VkBufferCreate(RenderState->Device, &DemoState->GpuArena, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                          sizeof(fire_inputs));

    // NOTE: Global Descriptor Layout
    {
        vk_descriptor_layout_builder Builder = VkDescriptorLayoutBegin(&Result.GlobalDescLayout);
        VkDescriptorLayoutAdd(&Builder, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT);
        VkDescriptorLayoutAdd(&Builder, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT);
        VkDescriptorLayoutEnd(RenderState->Device, &Builder);
    }
    
    // NOTE: Fire Splat Pipeline
    {
        {
            vk_descriptor_layout_builder Builder = VkDescriptorLayoutBegin(&Result.SplatDescLayout);
            VkDescriptorLayoutAdd(&Builder, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT);
            VkDescriptorLayoutAdd(&Builder, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT);
            VkDescriptorLayoutAdd(&Builder, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT);
            VkDescriptorLayoutEnd(RenderState->Device, &Builder);
        }

        VkDescriptorSetLayout Layouts[] = 
            {
                Result.GlobalDescLayout,
                Result.SplatDescLayout,
            };
            
        Result.SplatPipeline = VkPipelineComputeCreate(RenderState->Device, &RenderState->PipelineManager, &DemoState->TempArena,
                                                       "fire_splat.spv", "main", Layouts, ArrayCount(Layouts));
    }
        
    // NOTE: Fire Advection Pipeline
    {
        {
            vk_descriptor_layout_builder Builder = VkDescriptorLayoutBegin(&Result.AdvectionDescLayout);
            VkDescriptorLayoutAdd(&Builder, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT);
            VkDescriptorLayoutAdd(&Builder, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT);
            VkDescriptorLayoutAdd(&Builder, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT);
            VkDescriptorLayoutEnd(RenderState->Device, &Builder);
        }

        VkDescriptorSetLayout Layouts[] = 
            {
                Result.GlobalDescLayout,
                Result.AdvectionDescLayout,
            };
            
        Result.AdvectionPipeline = VkPipelineComputeCreate(RenderState->Device, &RenderState->PipelineManager, &DemoState->TempArena,
                                                           "fire_advection.spv", "main", Layouts, ArrayCount(Layouts));
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
                                                                "fire_divergence.spv", "main", Layouts, ArrayCount(Layouts));
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
                Result.PressureDescLayout,
            };
            
        Result.PressureIterationPipeline = VkPipelineComputeCreate(RenderState->Device, &RenderState->PipelineManager, &DemoState->TempArena,
                                                                   "fire_pressure_iteration.spv", "main", Layouts, ArrayCount(Layouts));
    }

    // NOTE: Fire Pressure Apply Pipeline
    {
        {
            vk_descriptor_layout_builder Builder = VkDescriptorLayoutBegin(&Result.PressureApplyDescLayout);
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
                Result.GlobalDescLayout,
                Result.PressureApplyDescLayout,
                Result.PressureDescLayout,
            };
            
        Result.PressureApplyPipeline = VkPipelineComputeCreate(RenderState->Device, &RenderState->PipelineManager, &DemoState->TempArena,
                                                               "fire_pressure_apply.spv", "main", Layouts, ArrayCount(Layouts));
    }
    
    // NOTE: Populate descriptors
    {    
        Result.SplatDescriptors[0] = VkDescriptorSetAllocate(RenderState->Device, RenderState->DescriptorPool, Result.SplatDescLayout);
        Result.SplatDescriptors[1] = VkDescriptorSetAllocate(RenderState->Device, RenderState->DescriptorPool, Result.SplatDescLayout);
        Result.AdvectionDescriptors[0] = VkDescriptorSetAllocate(RenderState->Device, RenderState->DescriptorPool, Result.AdvectionDescLayout);
        Result.AdvectionDescriptors[1] = VkDescriptorSetAllocate(RenderState->Device, RenderState->DescriptorPool, Result.AdvectionDescLayout);
        Result.DivergenceDescriptors[0] = VkDescriptorSetAllocate(RenderState->Device, RenderState->DescriptorPool, Result.DivergenceDescLayout);
        Result.DivergenceDescriptors[1] = VkDescriptorSetAllocate(RenderState->Device, RenderState->DescriptorPool, Result.DivergenceDescLayout);
        Result.PressureDescriptors[0] = VkDescriptorSetAllocate(RenderState->Device, RenderState->DescriptorPool, Result.PressureDescLayout);
        Result.PressureDescriptors[1] = VkDescriptorSetAllocate(RenderState->Device, RenderState->DescriptorPool, Result.PressureDescLayout);
        Result.PressureApplyDescriptors[0] = VkDescriptorSetAllocate(RenderState->Device, RenderState->DescriptorPool, Result.PressureApplyDescLayout);
        Result.PressureApplyDescriptors[1] = VkDescriptorSetAllocate(RenderState->Device, RenderState->DescriptorPool, Result.PressureApplyDescLayout);
        Result.RenderDescriptors[0] = VkDescriptorSetAllocate(RenderState->Device, RenderState->DescriptorPool, RenderState->CopyImageDescLayout);
        Result.RenderDescriptors[1] = VkDescriptorSetAllocate(RenderState->Device, RenderState->DescriptorPool, RenderState->CopyImageDescLayout);

        // NOTE: Global Descriptor
        {
            Result.GlobalDescriptor = VkDescriptorSetAllocate(RenderState->Device, RenderState->DescriptorPool, Result.GlobalDescLayout);
            VkDescriptorBufferWrite(&RenderState->DescriptorManager, Result.GlobalDescriptor, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, Result.UniformBuffer);
        }
    }

    // NOTE: Set uniform defaults
    {
        Result.Inputs.Density = 1;
        Result.Inputs.SplatCenter = V2(0.5f, 0.1f);
        Result.Inputs.SplatRadius = 0.02f;
        Result.Inputs.RoomTemperature = 100.0f;
        Result.Inputs.Gravity = 9.81f;
        Result.Inputs.MolarMass = 332.1099f; // NOTE: https://www.webqc.org/molecular-weight-of-FIRe.html
        Result.Inputs.R = 8.3145f; // NOTE: https://en.wikipedia.org/wiki/Gas_constant
    }

    FireResize(&Result, Commands, Width, Height);

    return Result;
}

inline void FireSetInputs(fire_sim* Fire, vk_commands* Commands, f32 FrameTime, v2 MousePos, v2 PrevMousePos)
{
    Fire->Inputs.MousePos = MousePos;
    Fire->Inputs.DeltaMousePos = MousePos - PrevMousePos;
    Fire->Inputs.FrameTime = FrameTime;
    
    fire_inputs* GpuPtr = VkCommandsPushWriteStruct(Commands, Fire->UniformBuffer, fire_inputs,
                                                    BarrierMask(VkAccessFlagBits(0), VK_PIPELINE_STAGE_ALL_COMMANDS_BIT),
                                                    BarrierMask(VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT));
    Copy(&Fire->Inputs, GpuPtr, sizeof(fire_inputs));
}

inline void FireSwap(vk_commands* Commands, fire_sim* Fire)
{
    // NOTE: Convert input images to output
    VkPipelineStageFlags StageFlags = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
    FluidSimSwapBarriers(Commands, Fire->TimerImages, Fire->InputId);
    FluidSimSwapBarriers(Commands, Fire->TemperatureImages, Fire->InputId);
    FluidSimSwapBarriers(Commands, Fire->ColorImages, Fire->InputId);
    
    // NOTE: Swap Velocities
    FluidSimSwapBarriers(Commands, Fire->VelocityImages, Fire->InputId);
    Fire->InputId = !Fire->InputId;

    VkCommandsBarrierFlush(Commands);
}

inline void FireSimulate(vk_commands* Commands, fire_sim* Fire)
{
    u32 DispatchX = CeilU32(f32(Fire->Width) / 8.0f);
    u32 DispatchY = CeilU32(f32(Fire->Height) / 8.0f);
    
    // NOTE: Splats
    {
        VkBarrierImageAdd(Commands, Fire->TemperatureImages[Fire->InputId].Image, VK_IMAGE_ASPECT_COLOR_BIT, 
                          VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                          VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_IMAGE_LAYOUT_GENERAL);
        VkBarrierImageAdd(Commands, Fire->TimerImages[Fire->InputId].Image, VK_IMAGE_ASPECT_COLOR_BIT, 
                          VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                          VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_IMAGE_LAYOUT_GENERAL);
        VkBarrierImageAdd(Commands, Fire->VelocityImages[Fire->InputId].Image, VK_IMAGE_ASPECT_COLOR_BIT, 
                          VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                          VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_IMAGE_LAYOUT_GENERAL);
        VkCommandsBarrierFlush(Commands);

        local_global u32 FrameId = 0;

        //if (FrameId == 0)
        {
            VkDescriptorSet DescriptorSets[] =
                {
                    Fire->GlobalDescriptor,
                    Fire->SplatDescriptors[Fire->InputId],
                };

            VkComputeDispatch(Commands, Fire->SplatPipeline, DescriptorSets, ArrayCount(DescriptorSets), DispatchX, DispatchY, 1);
            FrameId = 1;
        }
            
        VkBarrierImageAdd(Commands, Fire->TemperatureImages[Fire->InputId].Image, VK_IMAGE_ASPECT_COLOR_BIT, 
                          VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_IMAGE_LAYOUT_GENERAL,
                          VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_IMAGE_LAYOUT_GENERAL);
        VkBarrierImageAdd(Commands, Fire->TimerImages[Fire->InputId].Image, VK_IMAGE_ASPECT_COLOR_BIT, 
                          VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_IMAGE_LAYOUT_GENERAL,
                          VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        VkBarrierImageAdd(Commands, Fire->VelocityImages[Fire->InputId].Image, VK_IMAGE_ASPECT_COLOR_BIT, 
                          VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_IMAGE_LAYOUT_GENERAL,
                          VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        VkCommandsBarrierFlush(Commands);
            
        VkBarrierImageAdd(Commands, Fire->TemperatureImages[Fire->InputId].Image, VK_IMAGE_ASPECT_COLOR_BIT, 
                          VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_IMAGE_LAYOUT_GENERAL,
                          VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        VkCommandsBarrierFlush(Commands);
    }
        
    // NOTE: Advection
    {
        VkDescriptorSet DescriptorSets[] =
            {
                Fire->GlobalDescriptor,
                Fire->AdvectionDescriptors[Fire->InputId],
            };

        VkComputeDispatch(Commands, Fire->AdvectionPipeline, DescriptorSets, ArrayCount(DescriptorSets), DispatchX, DispatchY, 1);
    }
    
    VkBarrierImageAdd(Commands, Fire->VelocityImages[!Fire->InputId].Image, VK_IMAGE_ASPECT_COLOR_BIT, 
                      VK_ACCESS_SHADER_WRITE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_IMAGE_LAYOUT_GENERAL,
                      VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    VkCommandsBarrierFlush(Commands);
    
    // NOTE: Divergence
    {
        VkDescriptorSet DescriptorSets[] =
        {
            Fire->GlobalDescriptor,
            Fire->DivergenceDescriptors[Fire->InputId],
        };

        VkComputeDispatch(Commands, Fire->DivergencePipeline, DescriptorSets, ArrayCount(DescriptorSets), DispatchX, DispatchY, 1);
    }
    
    VkBarrierImageAdd(Commands, Fire->DivergenceImage.Image, VK_IMAGE_ASPECT_COLOR_BIT, 
                      VK_ACCESS_SHADER_WRITE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_IMAGE_LAYOUT_GENERAL,
                      VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_IMAGE_LAYOUT_GENERAL);
    VkCommandsBarrierFlush(Commands);

    // NOTE: Pressure Iteration
    for (u32 IterationId = 0; IterationId < 20; ++IterationId)
    {
        VkDescriptorSet DescriptorSets[] =
        {
            Fire->GlobalDescriptor,
            Fire->PressureDescriptors[Fire->PressureInputId],
        };

        VkComputeDispatch(Commands, Fire->PressureIterationPipeline, DescriptorSets, ArrayCount(DescriptorSets), DispatchX, DispatchY, 1);
        
        FluidSimSwapBarriersUav(Commands, Fire->PressureImages, Fire->PressureInputId);
        Fire->PressureInputId = !Fire->PressureInputId;
        VkCommandsBarrierFlush(Commands);
    }
    
    VkBarrierImageAdd(Commands, Fire->VelocityImages[!Fire->InputId].Image, VK_IMAGE_ASPECT_COLOR_BIT,
                      VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                      VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_IMAGE_LAYOUT_GENERAL);
    VkCommandsBarrierFlush(Commands);
    
    // NOTE: Pressure Apply and Advect Color
    {
        VkDescriptorSet DescriptorSets[] =
            {
                Fire->GlobalDescriptor,
                Fire->PressureApplyDescriptors[Fire->InputId],
                Fire->PressureDescriptors[Fire->PressureInputId],
            };
        VkComputeDispatch(Commands, Fire->PressureApplyPipeline, DescriptorSets, ArrayCount(DescriptorSets), DispatchX, DispatchY, 1);
    }
    
    // NOTE: Convert to default states
    VkBarrierImageAdd(Commands, Fire->DivergenceImage.Image, VK_IMAGE_ASPECT_COLOR_BIT,
                      VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_IMAGE_LAYOUT_GENERAL,
                      VK_ACCESS_SHADER_WRITE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_IMAGE_LAYOUT_GENERAL);
    VkCommandsBarrierFlush(Commands);

    FireSwap(Commands, Fire);    
}
