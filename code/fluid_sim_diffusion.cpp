
inline void DiffusionResize(diffusion_sim* Diffusion, vk_commands* Commands, u32 Width, u32 Height)
{
    Diffusion->Width = Width;
    Diffusion->Height = Height;
    Diffusion->InputId = 0;
    Diffusion->PressureInputId = 0;
    
    Assert(Width == Height);
    Diffusion->Inputs.Epsilon = 1.0f / f32(Width);
    Diffusion->Inputs.Dim = Width;

    VkImageUsageFlags ImageFlags = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    Diffusion->ColorImages[0] = VkImageCreate(RenderState->Device, &DemoState->GpuArena, Width, Height, VK_FORMAT_R32G32B32A32_SFLOAT,
                                              VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_IMAGE_ASPECT_COLOR_BIT);
    Diffusion->ColorImages[1] = VkImageCreate(RenderState->Device, &DemoState->GpuArena, Width, Height, VK_FORMAT_R32G32B32A32_SFLOAT,
                                              VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_IMAGE_ASPECT_COLOR_BIT);
    Diffusion->VelocityImages[0] = VkImageCreate(RenderState->Device, &DemoState->GpuArena, Width, Height, VK_FORMAT_R32G32_SFLOAT,
                                                 ImageFlags, VK_IMAGE_ASPECT_COLOR_BIT);
    Diffusion->VelocityImages[1] = VkImageCreate(RenderState->Device, &DemoState->GpuArena, Width, Height, VK_FORMAT_R32G32_SFLOAT,
                                                 ImageFlags, VK_IMAGE_ASPECT_COLOR_BIT);
    Diffusion->DivergenceImage = VkImageCreate(RenderState->Device, &DemoState->GpuArena, Width, Height, VK_FORMAT_R32_SFLOAT,
                                               ImageFlags, VK_IMAGE_ASPECT_COLOR_BIT);
    Diffusion->PressureImages[0] = VkImageCreate(RenderState->Device, &DemoState->GpuArena, Width, Height, VK_FORMAT_R32_SFLOAT,
                                                 ImageFlags, VK_IMAGE_ASPECT_COLOR_BIT);
    Diffusion->PressureImages[1] = VkImageCreate(RenderState->Device, &DemoState->GpuArena, Width, Height, VK_FORMAT_R32_SFLOAT,
                                                 ImageFlags, VK_IMAGE_ASPECT_COLOR_BIT);

    // NOTE: Global Descriptor
    {
        VkDescriptorImageWrite(&RenderState->DescriptorManager, Diffusion->GlobalDescriptor, 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                               Diffusion->DivergenceImage.View, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_GENERAL);
    }
    
    // NOTE: Init Descriptor
    {
        VkDescriptorImageWrite(&RenderState->DescriptorManager, Diffusion->InitDescriptor, 0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                               Diffusion->ColorImages[0].View, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_GENERAL);
        VkDescriptorImageWrite(&RenderState->DescriptorManager, Diffusion->InitDescriptor, 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                               Diffusion->VelocityImages[0].View, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_GENERAL);
    }
    
    // NOTE: Advection Descriptor (ping pong)
    {
        VkDescriptorImageWrite(&RenderState->DescriptorManager, Diffusion->AdvectionDescriptors[0], 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                               Diffusion->VelocityImages[0].View, DemoState->MirrorLinearSampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        VkDescriptorImageWrite(&RenderState->DescriptorManager, Diffusion->AdvectionDescriptors[0], 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                               Diffusion->VelocityImages[1].View, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_GENERAL);

        VkDescriptorImageWrite(&RenderState->DescriptorManager, Diffusion->AdvectionDescriptors[1], 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                               Diffusion->VelocityImages[1].View, DemoState->MirrorLinearSampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        VkDescriptorImageWrite(&RenderState->DescriptorManager, Diffusion->AdvectionDescriptors[1], 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                               Diffusion->VelocityImages[0].View, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_GENERAL);
    }

    // NOTE: Divergence Descriptor (ping pong)
    {
        VkDescriptorImageWrite(&RenderState->DescriptorManager, Diffusion->DivergenceDescriptors[0], 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                               Diffusion->VelocityImages[1].View, DemoState->MirrorLinearSampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

        VkDescriptorImageWrite(&RenderState->DescriptorManager, Diffusion->DivergenceDescriptors[1], 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                               Diffusion->VelocityImages[0].View, DemoState->MirrorLinearSampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    }

    // NOTE: Pressure Descriptor (ping pong)
    {
        VkDescriptorImageWrite(&RenderState->DescriptorManager, Diffusion->PressureDescriptors[0], 0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                               Diffusion->PressureImages[0].View, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_GENERAL);
        VkDescriptorImageWrite(&RenderState->DescriptorManager, Diffusion->PressureDescriptors[0], 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                               Diffusion->PressureImages[1].View, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_GENERAL);

        VkDescriptorImageWrite(&RenderState->DescriptorManager, Diffusion->PressureDescriptors[1], 0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                               Diffusion->PressureImages[1].View, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_GENERAL);
        VkDescriptorImageWrite(&RenderState->DescriptorManager, Diffusion->PressureDescriptors[1], 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                               Diffusion->PressureImages[0].View, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_GENERAL);
    }

    // NOTE: Pressure Apply Descriptor
    {
        VkDescriptorImageWrite(&RenderState->DescriptorManager, Diffusion->PressureApplyDescriptors[0], 0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                               Diffusion->VelocityImages[1].View, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_GENERAL);
        VkDescriptorImageWrite(&RenderState->DescriptorManager, Diffusion->PressureApplyDescriptors[0], 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                               Diffusion->ColorImages[0].View, DemoState->MirrorLinearSampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        VkDescriptorImageWrite(&RenderState->DescriptorManager, Diffusion->PressureApplyDescriptors[0], 2, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                               Diffusion->ColorImages[1].View, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_GENERAL);

        VkDescriptorImageWrite(&RenderState->DescriptorManager, Diffusion->PressureApplyDescriptors[1], 0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                               Diffusion->VelocityImages[0].View, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_GENERAL);
        VkDescriptorImageWrite(&RenderState->DescriptorManager, Diffusion->PressureApplyDescriptors[1], 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                               Diffusion->ColorImages[1].View, DemoState->MirrorLinearSampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        VkDescriptorImageWrite(&RenderState->DescriptorManager, Diffusion->PressureApplyDescriptors[1], 2, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                               Diffusion->ColorImages[0].View, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_GENERAL);
    }
        
    // NOTE: Render Descriptors
    {
        VkDescriptorImageWrite(&RenderState->DescriptorManager, Diffusion->RenderDescriptors[0], 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                               Diffusion->ColorImages[0].View, DemoState->LinearSampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
            
        VkDescriptorImageWrite(&RenderState->DescriptorManager, Diffusion->RenderDescriptors[1], 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                               Diffusion->ColorImages[1].View, DemoState->LinearSampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    }

    VkDescriptorManagerFlush(RenderState->Device, &RenderState->DescriptorManager);

    FluidSimInitBarriers(Commands, Diffusion->VelocityImages, 1);
    FluidSimInitBarriersUav(Commands, Diffusion->PressureImages, 1);
    FluidSimInitBarriers(Commands, Diffusion->ColorImages, 1);
    VkBarrierImageAdd(Commands, Diffusion->DivergenceImage.Image, VK_IMAGE_ASPECT_COLOR_BIT, 
                      VK_ACCESS_MEMORY_READ_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
                      VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_IMAGE_LAYOUT_GENERAL);
    VkCommandsBarrierFlush(Commands);

    // NOTE: Populate color images
    {
        DiffusionSetInputs(Diffusion, Commands, 0, V2(0), V2(0));
        VkCommandsTransferFlush(Commands, RenderState->Device);
        
        VkClearColorValue ClearValue = VkClearColorCreate(0, 0, 0, 0).color;
        VkImageSubresourceRange Range = {};
        Range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        Range.baseMipLevel = 0;
        Range.levelCount = 1;
        Range.baseArrayLayer = 0;
        Range.layerCount = 1;

        VkDescriptorSet DescriptorSets[] =
            {
                Diffusion->GlobalDescriptor,
                Diffusion->InitDescriptor,
            };
        u32 DispatchX = CeilU32(f32(Width) / 8.0f);
        u32 DispatchY = CeilU32(f32(Height) / 8.0f);
        VkComputeDispatch(Commands, Diffusion->InitPipeline, DescriptorSets, ArrayCount(DescriptorSets), DispatchX, DispatchY, 1);

        vkCmdClearColorImage(Commands->Buffer, Diffusion->PressureImages[0].Image, VK_IMAGE_LAYOUT_GENERAL, &ClearValue, 1, &Range);
        
        FluidSimSwapBarriers(Commands, Diffusion->ColorImages, 1);
        FluidSimSwapBarriers(Commands, Diffusion->VelocityImages, 1);
        FluidSimSwapBarriersUav(Commands, Diffusion->PressureImages, 1);
        VkCommandsBarrierFlush(Commands);
    }
}

inline diffusion_sim DiffusionInit(vk_commands* Commands, u32 Width, u32 Height)
{
    diffusion_sim Result = {};

    Result.UniformBuffer = VkBufferCreate(RenderState->Device, &DemoState->GpuArena, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                          sizeof(diffusion_inputs));

    // NOTE: Global Descriptor Layout
    {
        vk_descriptor_layout_builder Builder = VkDescriptorLayoutBegin(&Result.GlobalDescLayout);
        VkDescriptorLayoutAdd(&Builder, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT);
        VkDescriptorLayoutAdd(&Builder, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT);
        VkDescriptorLayoutEnd(RenderState->Device, &Builder);
    }

    // NOTE: Diffusion Init Pipeline
    {
        {
            vk_descriptor_layout_builder Builder = VkDescriptorLayoutBegin(&Result.InitDescLayout);
            VkDescriptorLayoutAdd(&Builder, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT);
            VkDescriptorLayoutAdd(&Builder, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT);
            VkDescriptorLayoutEnd(RenderState->Device, &Builder);
        }

        VkDescriptorSetLayout Layouts[] = 
            {
                Result.GlobalDescLayout,
                Result.InitDescLayout,
            };
            
        Result.InitPipeline = VkPipelineComputeCreate(RenderState->Device, &RenderState->PipelineManager, &DemoState->TempArena,
                                                      "diffusion_init.spv", "main", Layouts, ArrayCount(Layouts));
    }
    
    // NOTE: Diffusion Advection Pipeline
    {
        {
            vk_descriptor_layout_builder Builder = VkDescriptorLayoutBegin(&Result.AdvectionDescLayout);
            VkDescriptorLayoutAdd(&Builder, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT);
            VkDescriptorLayoutAdd(&Builder, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT);
            VkDescriptorLayoutEnd(RenderState->Device, &Builder);
        }

        VkDescriptorSetLayout Layouts[] = 
            {
                Result.GlobalDescLayout,
                Result.AdvectionDescLayout,
            };
            
        Result.AdvectionPipeline = VkPipelineComputeCreate(RenderState->Device, &RenderState->PipelineManager, &DemoState->TempArena,
                                                              "diffusion_advection.spv", "main", Layouts, ArrayCount(Layouts));
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
                                                                "diffusion_divergence.spv", "main", Layouts, ArrayCount(Layouts));
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
                                                                   "diffusion_pressure_iteration.spv", "main", Layouts, ArrayCount(Layouts));
    }

    // NOTE: Diffusion Pressure Apply Pipeline
    {
        {
            vk_descriptor_layout_builder Builder = VkDescriptorLayoutBegin(&Result.PressureApplyDescLayout);
            VkDescriptorLayoutAdd(&Builder, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT);
            VkDescriptorLayoutAdd(&Builder, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT);
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
                                                                  "diffusion_pressure_apply.spv", "main", Layouts, ArrayCount(Layouts));
    }
    
    // NOTE: Populate descriptors
    {    
        Result.InitDescriptor = VkDescriptorSetAllocate(RenderState->Device, RenderState->DescriptorPool, Result.InitDescLayout);
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
    }

    DiffusionResize(&Result, Commands, Width, Height);
    
    return Result;
}

inline void DiffusionSetInputs(diffusion_sim* Diffusion, vk_commands* Commands, f32 FrameTime, v2 MousePos, v2 PrevMousePos)
{
    Diffusion->Inputs.MousePos = MousePos;
    Diffusion->Inputs.DeltaMousePos = MousePos - PrevMousePos;
    Diffusion->Inputs.FrameTime = FrameTime;
    
    diffusion_inputs* GpuPtr = VkCommandsPushWriteStruct(Commands, Diffusion->UniformBuffer, diffusion_inputs,
                                                         BarrierMask(VkAccessFlagBits(0), VK_PIPELINE_STAGE_ALL_COMMANDS_BIT),
                                                         BarrierMask(VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT));
    Copy(&Diffusion->Inputs, GpuPtr, sizeof(diffusion_inputs));
}

inline void DiffusionSwap(vk_commands* Commands, diffusion_sim* Diffusion)
{
    // NOTE: Convert input images to output
    VkPipelineStageFlags StageFlags = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
    FluidSimSwapBarriers(Commands, Diffusion->ColorImages, Diffusion->InputId);
    
    // NOTE: Swap Velocities
    FluidSimSwapBarriers(Commands, Diffusion->VelocityImages, Diffusion->InputId);
    Diffusion->InputId = !Diffusion->InputId;

    VkCommandsBarrierFlush(Commands);
}

inline void DiffusionSimulate(vk_commands* Commands, diffusion_sim* Diffusion)
{
    u32 DispatchX = CeilU32(f32(Diffusion->Width) / 8.0f);
    u32 DispatchY = CeilU32(f32(Diffusion->Height) / 8.0f);

    // NOTE: Advection
    {
        VkDescriptorSet DescriptorSets[] =
            {
                Diffusion->GlobalDescriptor,
                Diffusion->AdvectionDescriptors[Diffusion->InputId],
            };

        VkComputeDispatch(Commands, Diffusion->AdvectionPipeline, DescriptorSets, ArrayCount(DescriptorSets), DispatchX, DispatchY, 1);
    }
    
    VkBarrierImageAdd(Commands, Diffusion->VelocityImages[!Diffusion->InputId].Image, VK_IMAGE_ASPECT_COLOR_BIT, 
                      VK_ACCESS_SHADER_WRITE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_IMAGE_LAYOUT_GENERAL,
                      VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    VkCommandsBarrierFlush(Commands);
    
    // NOTE: Divergence
    {
        VkDescriptorSet DescriptorSets[] =
            {
                Diffusion->GlobalDescriptor,
                Diffusion->DivergenceDescriptors[Diffusion->InputId],
            };

        VkComputeDispatch(Commands, Diffusion->DivergencePipeline, DescriptorSets, ArrayCount(DescriptorSets), DispatchX, DispatchY, 1);
    }
    
    VkBarrierImageAdd(Commands, Diffusion->DivergenceImage.Image, VK_IMAGE_ASPECT_COLOR_BIT, 
                      VK_ACCESS_SHADER_WRITE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_IMAGE_LAYOUT_GENERAL,
                      VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_IMAGE_LAYOUT_GENERAL);
    VkCommandsBarrierFlush(Commands);

    // NOTE: Pressure Iteration
    for (u32 IterationId = 0; IterationId < 20; ++IterationId)
    {
        VkDescriptorSet DescriptorSets[] =
            {
                Diffusion->GlobalDescriptor,
                Diffusion->PressureDescriptors[Diffusion->PressureInputId],
            };

        VkComputeDispatch(Commands, Diffusion->PressureIterationPipeline, DescriptorSets, ArrayCount(DescriptorSets), DispatchX, DispatchY, 1);
        
        FluidSimSwapBarriersUav(Commands, Diffusion->PressureImages, Diffusion->PressureInputId);
        Diffusion->PressureInputId = !Diffusion->PressureInputId;
        VkCommandsBarrierFlush(Commands);
    }
    
    VkBarrierImageAdd(Commands, Diffusion->VelocityImages[!Diffusion->InputId].Image, VK_IMAGE_ASPECT_COLOR_BIT,
                      VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                      VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_IMAGE_LAYOUT_GENERAL);
    VkCommandsBarrierFlush(Commands);
    
    // NOTE: Pressure Apply and Advect Color
    VkDescriptorSet DescriptorSets[] =
        {
            Diffusion->GlobalDescriptor,
            Diffusion->PressureApplyDescriptors[Diffusion->InputId],
            Diffusion->PressureDescriptors[Diffusion->PressureInputId],
        };
    VkComputeDispatch(Commands, Diffusion->PressureApplyPipeline, DescriptorSets, ArrayCount(DescriptorSets), DispatchX, DispatchY, 1);
    
    // NOTE: Convert to default states
    VkBarrierImageAdd(Commands, Diffusion->DivergenceImage.Image, VK_IMAGE_ASPECT_COLOR_BIT,
                      VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_IMAGE_LAYOUT_GENERAL,
                      VK_ACCESS_SHADER_WRITE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_IMAGE_LAYOUT_GENERAL);
    VkCommandsBarrierFlush(Commands);

    DiffusionSwap(Commands, Diffusion);
}
