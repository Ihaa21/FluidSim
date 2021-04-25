
inline void SmokeResize(smoke_sim* Smoke, vk_commands* Commands, u32 Width, u32 Height)
{
    Smoke->Width = Width;
    Smoke->Height = Height;
    Smoke->InputId = 0;
    Smoke->PressureInputId = 0;
    
    Assert(Width == Height);
    Smoke->Inputs.Epsilon = 1.0f / f32(Width);
    Smoke->Inputs.Dim = Width;

    VkImageUsageFlags ImageFlags = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    
    Smoke->ColorImages[0] = VkImageCreate(RenderState->Device, &DemoState->GpuArena, Width, Height, VK_FORMAT_R32G32B32A32_SFLOAT,
                                          ImageFlags, VK_IMAGE_ASPECT_COLOR_BIT);
    Smoke->ColorImages[1] = VkImageCreate(RenderState->Device, &DemoState->GpuArena, Width, Height, VK_FORMAT_R32G32B32A32_SFLOAT,
                                          ImageFlags, VK_IMAGE_ASPECT_COLOR_BIT);
    Smoke->TemperatureImages[0] = VkImageCreate(RenderState->Device, &DemoState->GpuArena, Width, Height, VK_FORMAT_R32_SFLOAT,
                                                ImageFlags, VK_IMAGE_ASPECT_COLOR_BIT);
    Smoke->TemperatureImages[1] = VkImageCreate(RenderState->Device, &DemoState->GpuArena, Width, Height, VK_FORMAT_R32_SFLOAT,
                                                ImageFlags, VK_IMAGE_ASPECT_COLOR_BIT);
    Smoke->VelocityImages[0] = VkImageCreate(RenderState->Device, &DemoState->GpuArena, Width, Height, VK_FORMAT_R32G32_SFLOAT,
                                             ImageFlags, VK_IMAGE_ASPECT_COLOR_BIT);
    Smoke->VelocityImages[1] = VkImageCreate(RenderState->Device, &DemoState->GpuArena, Width, Height, VK_FORMAT_R32G32_SFLOAT,
                                             ImageFlags, VK_IMAGE_ASPECT_COLOR_BIT);
    Smoke->DivergenceImage = VkImageCreate(RenderState->Device, &DemoState->GpuArena, Width, Height, VK_FORMAT_R32_SFLOAT,
                                           ImageFlags, VK_IMAGE_ASPECT_COLOR_BIT);
    Smoke->PressureImages[0] = VkImageCreate(RenderState->Device, &DemoState->GpuArena, Width, Height, VK_FORMAT_R32_SFLOAT,
                                             ImageFlags, VK_IMAGE_ASPECT_COLOR_BIT);
    Smoke->PressureImages[1] = VkImageCreate(RenderState->Device, &DemoState->GpuArena, Width, Height, VK_FORMAT_R32_SFLOAT,
                                             ImageFlags, VK_IMAGE_ASPECT_COLOR_BIT);

    // NOTE: Global Descriptor
    {
        VkDescriptorImageWrite(&RenderState->DescriptorManager, Smoke->GlobalDescriptor, 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                               Smoke->DivergenceImage.View, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_GENERAL);
    }
    
    // NOTE: Splat Descriptors (ping pong)
    {
        VkDescriptorImageWrite(&RenderState->DescriptorManager, Smoke->SplatDescriptors[0], 0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                               Smoke->TemperatureImages[0].View, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_GENERAL);
        VkDescriptorImageWrite(&RenderState->DescriptorManager, Smoke->SplatDescriptors[0], 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                               Smoke->ColorImages[0].View, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_GENERAL);
        VkDescriptorImageWrite(&RenderState->DescriptorManager, Smoke->SplatDescriptors[0], 2, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                               Smoke->VelocityImages[0].View, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_GENERAL);

        VkDescriptorImageWrite(&RenderState->DescriptorManager, Smoke->SplatDescriptors[1], 0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                               Smoke->TemperatureImages[1].View, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_GENERAL);
        VkDescriptorImageWrite(&RenderState->DescriptorManager, Smoke->SplatDescriptors[1], 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                               Smoke->ColorImages[1].View, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_GENERAL);
        VkDescriptorImageWrite(&RenderState->DescriptorManager, Smoke->SplatDescriptors[1], 2, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                               Smoke->VelocityImages[1].View, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_GENERAL);
    }

    // NOTE: Advection Descriptor (ping pong)
    {
        VkDescriptorImageWrite(&RenderState->DescriptorManager, Smoke->AdvectionDescriptors[0], 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                               Smoke->VelocityImages[0].View, DemoState->ClampLinearSampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        VkDescriptorImageWrite(&RenderState->DescriptorManager, Smoke->AdvectionDescriptors[0], 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                               Smoke->VelocityImages[1].View, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_GENERAL);
        VkDescriptorImageWrite(&RenderState->DescriptorManager, Smoke->AdvectionDescriptors[0], 2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                               Smoke->TemperatureImages[0].View, DemoState->ClampLinearSampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

        VkDescriptorImageWrite(&RenderState->DescriptorManager, Smoke->AdvectionDescriptors[1], 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                               Smoke->VelocityImages[1].View, DemoState->ClampLinearSampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        VkDescriptorImageWrite(&RenderState->DescriptorManager, Smoke->AdvectionDescriptors[1], 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                               Smoke->VelocityImages[0].View, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_GENERAL);
        VkDescriptorImageWrite(&RenderState->DescriptorManager, Smoke->AdvectionDescriptors[1], 2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                               Smoke->TemperatureImages[1].View, DemoState->ClampLinearSampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    }

    // NOTE: Divergence Descriptor (ping pong)
    {
        VkDescriptorImageWrite(&RenderState->DescriptorManager, Smoke->DivergenceDescriptors[0], 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                               Smoke->VelocityImages[1].View, DemoState->MirrorLinearSampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

        VkDescriptorImageWrite(&RenderState->DescriptorManager, Smoke->DivergenceDescriptors[1], 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                               Smoke->VelocityImages[0].View, DemoState->MirrorLinearSampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    }

    // NOTE: Pressure Descriptor (ping pong)
    {
        VkDescriptorImageWrite(&RenderState->DescriptorManager, Smoke->PressureDescriptors[0], 0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                               Smoke->PressureImages[0].View, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_GENERAL);
        VkDescriptorImageWrite(&RenderState->DescriptorManager, Smoke->PressureDescriptors[0], 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                               Smoke->PressureImages[1].View, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_GENERAL);

        VkDescriptorImageWrite(&RenderState->DescriptorManager, Smoke->PressureDescriptors[1], 0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                               Smoke->PressureImages[1].View, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_GENERAL);
        VkDescriptorImageWrite(&RenderState->DescriptorManager, Smoke->PressureDescriptors[1], 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                               Smoke->PressureImages[0].View, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_GENERAL);
    }

    // NOTE: Pressure Apply Descriptor
    {
        VkDescriptorImageWrite(&RenderState->DescriptorManager, Smoke->PressureApplyDescriptors[0], 0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                               Smoke->VelocityImages[1].View, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_GENERAL);
        VkDescriptorImageWrite(&RenderState->DescriptorManager, Smoke->PressureApplyDescriptors[0], 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                               Smoke->ColorImages[0].View, DemoState->ClampLinearSampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        VkDescriptorImageWrite(&RenderState->DescriptorManager, Smoke->PressureApplyDescriptors[0], 2, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                               Smoke->ColorImages[1].View, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_GENERAL);
        VkDescriptorImageWrite(&RenderState->DescriptorManager, Smoke->PressureApplyDescriptors[0], 3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                               Smoke->TemperatureImages[0].View, DemoState->ClampLinearSampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        VkDescriptorImageWrite(&RenderState->DescriptorManager, Smoke->PressureApplyDescriptors[0], 4, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                               Smoke->TemperatureImages[1].View, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_GENERAL);

        VkDescriptorImageWrite(&RenderState->DescriptorManager, Smoke->PressureApplyDescriptors[1], 0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                               Smoke->VelocityImages[0].View, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_GENERAL);
        VkDescriptorImageWrite(&RenderState->DescriptorManager, Smoke->PressureApplyDescriptors[1], 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                               Smoke->ColorImages[1].View, DemoState->ClampLinearSampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        VkDescriptorImageWrite(&RenderState->DescriptorManager, Smoke->PressureApplyDescriptors[1], 2, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                               Smoke->ColorImages[0].View, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_GENERAL);
        VkDescriptorImageWrite(&RenderState->DescriptorManager, Smoke->PressureApplyDescriptors[1], 3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                               Smoke->TemperatureImages[1].View, DemoState->ClampLinearSampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        VkDescriptorImageWrite(&RenderState->DescriptorManager, Smoke->PressureApplyDescriptors[1], 4, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                               Smoke->TemperatureImages[0].View, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_GENERAL);
    }
        
    // NOTE: Render Descriptors
    {
        VkDescriptorImageWrite(&RenderState->DescriptorManager, Smoke->RenderDescriptors[0], 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                               Smoke->ColorImages[0].View, DemoState->LinearSampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
            
        VkDescriptorImageWrite(&RenderState->DescriptorManager, Smoke->RenderDescriptors[1], 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                               Smoke->ColorImages[1].View, DemoState->LinearSampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    }
    
    VkDescriptorManagerFlush(RenderState->Device, &RenderState->DescriptorManager);

    FluidSimInitBarriers(Commands, Smoke->ColorImages, 1);
    FluidSimInitBarriers(Commands, Smoke->VelocityImages, 1);
    FluidSimInitBarriersUav(Commands, Smoke->PressureImages, 1);
    FluidSimInitBarriers(Commands, Smoke->TemperatureImages, 1);
    VkBarrierImageAdd(Commands, Smoke->DivergenceImage.Image, VK_IMAGE_ASPECT_COLOR_BIT, 
                      VK_ACCESS_MEMORY_READ_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
                      VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_IMAGE_LAYOUT_GENERAL);
    VkCommandsBarrierFlush(Commands);

    // NOTE: Clear all images
    {
        SmokeSetInputs(Smoke, Commands, 0, V2(0), V2(0));
        VkCommandsTransferFlush(Commands, RenderState->Device);

        VkClearColorValue ClearValue = VkClearColorCreate(0, 0, 0, 0).color;
        VkClearColorValue TemperatureValue = VkClearColorCreate(Smoke->Inputs.RoomTemperature, 0, 0, 0).color;
        VkImageSubresourceRange Range = {};
        Range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        Range.baseMipLevel = 0;
        Range.levelCount = 1;
        Range.baseArrayLayer = 0;
        Range.layerCount = 1;
        
        vkCmdClearColorImage(Commands->Buffer, Smoke->ColorImages[0].Image, VK_IMAGE_LAYOUT_GENERAL, &ClearValue, 1, &Range);
        vkCmdClearColorImage(Commands->Buffer, Smoke->VelocityImages[0].Image, VK_IMAGE_LAYOUT_GENERAL, &ClearValue, 1, &Range);
        vkCmdClearColorImage(Commands->Buffer, Smoke->PressureImages[0].Image, VK_IMAGE_LAYOUT_GENERAL, &ClearValue, 1, &Range);
        vkCmdClearColorImage(Commands->Buffer, Smoke->TemperatureImages[0].Image, VK_IMAGE_LAYOUT_GENERAL, &TemperatureValue, 1, &Range);

        FluidSimSwapBarriers(Commands, Smoke->ColorImages, 1);
        FluidSimSwapBarriers(Commands, Smoke->VelocityImages, 1);
        FluidSimSwapBarriersUav(Commands, Smoke->PressureImages, 1);
        FluidSimSwapBarriers(Commands, Smoke->TemperatureImages, 1);
        VkCommandsBarrierFlush(Commands);

        // NOTE: Init smoke
    }
}

inline smoke_sim SmokeInit(vk_commands* Commands, u32 Width, u32 Height)
{
    smoke_sim Result = {};

    Result.UniformBuffer = VkBufferCreate(RenderState->Device, &DemoState->GpuArena, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                          sizeof(smoke_inputs));

    // NOTE: Global Descriptor Layout
    {
        vk_descriptor_layout_builder Builder = VkDescriptorLayoutBegin(&Result.GlobalDescLayout);
        VkDescriptorLayoutAdd(&Builder, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT);
        VkDescriptorLayoutAdd(&Builder, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT);
        VkDescriptorLayoutEnd(RenderState->Device, &Builder);
    }
    
    // NOTE: Smoke Splat Pipeline
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
                                                       "smoke_splat.spv", "main", Layouts, ArrayCount(Layouts));
    }
        
    // NOTE: Smoke Advection Pipeline
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
                                                                "smoke_advection.spv", "main", Layouts, ArrayCount(Layouts));
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
                                                                "smoke_divergence.spv", "main", Layouts, ArrayCount(Layouts));
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
                                                                   "smoke_pressure_iteration.spv", "main", Layouts, ArrayCount(Layouts));
    }

    // NOTE: Smoke Pressure Apply Pipeline
    {
        {
            vk_descriptor_layout_builder Builder = VkDescriptorLayoutBegin(&Result.PressureApplyDescLayout);
            VkDescriptorLayoutAdd(&Builder, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT);
            VkDescriptorLayoutAdd(&Builder, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT);
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
                                                               "smoke_pressure_apply.spv", "main", Layouts, ArrayCount(Layouts));
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
        Result.Inputs.RoomTemperature = 1.0f;
        Result.Inputs.Gravity = 9.81f;
        Result.Inputs.MolarMass = 53.4915f; // NOTE: https://www.webqc.org/molecular-weight-of-NH4Cl%28smoke%29.html
        Result.Inputs.R = 8.3145f; // NOTE: https://en.wikipedia.org/wiki/Gas_constant
    }

    SmokeResize(&Result, Commands, Width, Height);

    return Result;
}

inline void SmokeSetInputs(smoke_sim* Smoke, vk_commands* Commands, f32 FrameTime, v2 MousePos, v2 PrevMousePos)
{
    Smoke->Inputs.MousePos = MousePos;
    Smoke->Inputs.DeltaMousePos = MousePos - PrevMousePos;
    Smoke->Inputs.FrameTime = FrameTime;
    
    smoke_inputs* GpuPtr = VkCommandsPushWriteStruct(Commands, Smoke->UniformBuffer, smoke_inputs,
                                                     BarrierMask(VkAccessFlagBits(0), VK_PIPELINE_STAGE_ALL_COMMANDS_BIT),
                                                     BarrierMask(VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT));
    Copy(&Smoke->Inputs, GpuPtr, sizeof(smoke_inputs));
}

inline void SmokeSwap(vk_commands* Commands, smoke_sim* Smoke)
{
    // NOTE: Convert input images to output
    VkPipelineStageFlags StageFlags = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
    FluidSimSwapBarriers(Commands, Smoke->ColorImages, Smoke->InputId);
    FluidSimSwapBarriers(Commands, Smoke->TemperatureImages, Smoke->InputId);
    FluidSimSwapBarriers(Commands, Smoke->VelocityImages, Smoke->InputId);
    Smoke->InputId = !Smoke->InputId;

    VkCommandsBarrierFlush(Commands);
}

inline void SmokeSimulate(vk_commands* Commands, smoke_sim* Smoke)
{
    u32 DispatchX = CeilU32(f32(Smoke->Width) / 8.0f);
    u32 DispatchY = CeilU32(f32(Smoke->Height) / 8.0f);
    
    // NOTE: Splats
    {
        VkDescriptorSet DescriptorSets[] =
            {
                Smoke->GlobalDescriptor,
                Smoke->SplatDescriptors[Smoke->InputId],
            };

        VkBarrierImageAdd(Commands, Smoke->TemperatureImages[Smoke->InputId].Image, VK_IMAGE_ASPECT_COLOR_BIT, 
                          VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                          VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_IMAGE_LAYOUT_GENERAL);
        VkBarrierImageAdd(Commands, Smoke->ColorImages[Smoke->InputId].Image, VK_IMAGE_ASPECT_COLOR_BIT, 
                          VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                          VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_IMAGE_LAYOUT_GENERAL);
        VkBarrierImageAdd(Commands, Smoke->VelocityImages[Smoke->InputId].Image, VK_IMAGE_ASPECT_COLOR_BIT, 
                          VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                          VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_IMAGE_LAYOUT_GENERAL);
        VkCommandsBarrierFlush(Commands);

        local_global u32 FrameId = 0;

        //if ((FrameId % 20) == 0)
        {
            VkComputeDispatch(Commands, Smoke->SplatPipeline, DescriptorSets, ArrayCount(DescriptorSets), DispatchX, DispatchY, 1);
        }
        FrameId += 1;
            
        VkBarrierImageAdd(Commands, Smoke->TemperatureImages[Smoke->InputId].Image, VK_IMAGE_ASPECT_COLOR_BIT,
                          VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_IMAGE_LAYOUT_GENERAL,
                          VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        VkBarrierImageAdd(Commands, Smoke->ColorImages[Smoke->InputId].Image, VK_IMAGE_ASPECT_COLOR_BIT,
                          VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_IMAGE_LAYOUT_GENERAL,
                          VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        VkBarrierImageAdd(Commands, Smoke->VelocityImages[Smoke->InputId].Image, VK_IMAGE_ASPECT_COLOR_BIT, 
                          VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_IMAGE_LAYOUT_GENERAL,
                          VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        VkCommandsBarrierFlush(Commands);
    }
        
    {
        VkDescriptorSet DescriptorSets[] =
            {
                Smoke->GlobalDescriptor,
                Smoke->AdvectionDescriptors[Smoke->InputId],
            };

        VkComputeDispatch(Commands, Smoke->AdvectionPipeline, DescriptorSets, ArrayCount(DescriptorSets), DispatchX, DispatchY, 1);
    }
        
    VkBarrierImageAdd(Commands, Smoke->VelocityImages[!Smoke->InputId].Image, VK_IMAGE_ASPECT_COLOR_BIT, 
                      VK_ACCESS_SHADER_WRITE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_IMAGE_LAYOUT_GENERAL,
                      VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    VkCommandsBarrierFlush(Commands);
    
    // NOTE: Divergence
    {
        VkDescriptorSet DescriptorSets[] =
        {
            Smoke->GlobalDescriptor,
            Smoke->DivergenceDescriptors[Smoke->InputId],
        };

        VkComputeDispatch(Commands, Smoke->DivergencePipeline, DescriptorSets, ArrayCount(DescriptorSets), DispatchX, DispatchY, 1);
    }
    
    VkBarrierImageAdd(Commands, Smoke->DivergenceImage.Image, VK_IMAGE_ASPECT_COLOR_BIT, 
                      VK_ACCESS_SHADER_WRITE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_IMAGE_LAYOUT_GENERAL,
                      VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_IMAGE_LAYOUT_GENERAL);
    VkCommandsBarrierFlush(Commands);

    // NOTE: Pressure Iteration
    for (u32 IterationId = 0; IterationId < 40; ++IterationId)
    {
        VkDescriptorSet DescriptorSets[] =
        {
            Smoke->GlobalDescriptor,
            Smoke->PressureDescriptors[Smoke->PressureInputId],
        };
        VkComputeDispatch(Commands, Smoke->PressureIterationPipeline, DescriptorSets, ArrayCount(DescriptorSets), DispatchX, DispatchY, 1);
        
        FluidSimSwapBarriersUav(Commands, Smoke->PressureImages, Smoke->PressureInputId);
        Smoke->PressureInputId = !Smoke->PressureInputId;
        VkCommandsBarrierFlush(Commands);
    }
    
    VkBarrierImageAdd(Commands, Smoke->VelocityImages[!Smoke->InputId].Image, VK_IMAGE_ASPECT_COLOR_BIT,
                      VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                      VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_IMAGE_LAYOUT_GENERAL);
    VkCommandsBarrierFlush(Commands);

    {
        VkDescriptorSet DescriptorSets[] =
            {
                Smoke->GlobalDescriptor,
                Smoke->PressureApplyDescriptors[Smoke->InputId],
                Smoke->PressureDescriptors[Smoke->PressureInputId],
            };
        VkComputeDispatch(Commands, Smoke->PressureApplyPipeline, DescriptorSets, ArrayCount(DescriptorSets), DispatchX, DispatchY, 1);
    }
    
    // NOTE: Convert to default states
    VkBarrierImageAdd(Commands, Smoke->DivergenceImage.Image, VK_IMAGE_ASPECT_COLOR_BIT,
                      VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_IMAGE_LAYOUT_GENERAL,
                      VK_ACCESS_SHADER_WRITE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_IMAGE_LAYOUT_GENERAL);
    VkCommandsBarrierFlush(Commands);

    SmokeSwap(Commands, Smoke);
}
