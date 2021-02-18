
inline fluid_sim FluidSimCreate(u32 Width, u32 Height, VkRenderPass RenderPass, u32 SubPassId)
{
    fluid_sim Result = {};

    Result.InputId = 0;
    Result.Width = Width;
    Result.Height = Height;
    Result.Arena = VkLinearArenaCreate(RenderState->Device, RenderState->LocalMemoryId, MegaBytes(64));
    Result.UniformBuffer = VkBufferCreate(RenderState->Device, &Result.Arena, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                          sizeof(fluid_sim_inputs));

    Result.PointSampler = VkSamplerCreate(RenderState->Device, VK_FILTER_NEAREST, VK_SAMPLER_ADDRESS_MODE_REPEAT, 0.0f);
    Result.LinearSampler = VkSamplerCreate(RenderState->Device, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_REPEAT, 0.0f);

    Result.ColorImages[0] = VkImageCreate(RenderState->Device, &Result.Arena, Width, Height, VK_FORMAT_R32G32B32A32_SFLOAT,
                                          VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_IMAGE_ASPECT_COLOR_BIT);
    Result.ColorImages[1] = VkImageCreate(RenderState->Device, &Result.Arena, Width, Height, VK_FORMAT_R32G32B32A32_SFLOAT,
                                          VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_IMAGE_ASPECT_COLOR_BIT);

    Result.VelocityImages[0] = VkImageCreate(RenderState->Device, &Result.Arena, Width, Height, VK_FORMAT_R32G32_SFLOAT,
                                             VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_IMAGE_ASPECT_COLOR_BIT);
    Result.VelocityImages[1] = VkImageCreate(RenderState->Device, &Result.Arena, Width, Height, VK_FORMAT_R32G32_SFLOAT,
                                             VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_IMAGE_ASPECT_COLOR_BIT);

    Result.DivergenceImage = VkImageCreate(RenderState->Device, &Result.Arena, Width, Height, VK_FORMAT_R32_SFLOAT,
                                          VK_IMAGE_USAGE_STORAGE_BIT, VK_IMAGE_ASPECT_COLOR_BIT);
    Result.PressureImages[0] = VkImageCreate(RenderState->Device, &Result.Arena, Width, Height, VK_FORMAT_R32_SFLOAT,
                                             VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_IMAGE_ASPECT_COLOR_BIT);
    Result.PressureImages[1] = VkImageCreate(RenderState->Device, &Result.Arena, Width, Height, VK_FORMAT_R32_SFLOAT,
                                             VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_IMAGE_ASPECT_COLOR_BIT);
    
    {
        vk_descriptor_layout_builder Builder = VkDescriptorLayoutBegin(&Result.DescLayout);
        VkDescriptorLayoutAdd(&Builder, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT);
        VkDescriptorLayoutAdd(&Builder, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT);
        VkDescriptorLayoutAdd(&Builder, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT);
        VkDescriptorLayoutAdd(&Builder, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT);
        VkDescriptorLayoutAdd(&Builder, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT);
        VkDescriptorLayoutAdd(&Builder, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT);
        VkDescriptorLayoutEnd(RenderState->Device, &Builder);
    }
        
    {
        vk_descriptor_layout_builder Builder = VkDescriptorLayoutBegin(&Result.PressureDescLayout);
        VkDescriptorLayoutAdd(&Builder, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT);
        //VkDescriptorLayoutAdd(&Builder, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT);
        VkDescriptorLayoutAdd(&Builder, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT);
        VkDescriptorLayoutEnd(RenderState->Device, &Builder);
    }

    // NOTE: We store 2 descriptors for double buffering
    {
        {
            Result.Descriptors[0] = VkDescriptorSetAllocate(RenderState->Device, RenderState->DescriptorPool, Result.DescLayout);
            VkDescriptorBufferWrite(&RenderState->DescriptorManager, Result.Descriptors[0], 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, Result.UniformBuffer);

            // NOTE: The below varies because we ping pong images
            VkDescriptorImageWrite(&RenderState->DescriptorManager, Result.Descriptors[0], 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                   Result.ColorImages[0].View, Result.LinearSampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
            VkDescriptorImageWrite(&RenderState->DescriptorManager, Result.Descriptors[0], 2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                   Result.VelocityImages[0].View, Result.LinearSampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
            VkDescriptorImageWrite(&RenderState->DescriptorManager, Result.Descriptors[0], 3, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                                   Result.ColorImages[1].View, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_GENERAL);
            VkDescriptorImageWrite(&RenderState->DescriptorManager, Result.Descriptors[0], 4, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                                   Result.VelocityImages[1].View, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_GENERAL);

            VkDescriptorImageWrite(&RenderState->DescriptorManager, Result.Descriptors[0], 5, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                                   Result.DivergenceImage.View, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_GENERAL);
        }

        {
            Result.Descriptors[1] = VkDescriptorSetAllocate(RenderState->Device, RenderState->DescriptorPool, Result.DescLayout);
            VkDescriptorBufferWrite(&RenderState->DescriptorManager, Result.Descriptors[1], 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, Result.UniformBuffer);

            // NOTE: The below varies because we ping pong images
            VkDescriptorImageWrite(&RenderState->DescriptorManager, Result.Descriptors[1], 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                   Result.ColorImages[1].View, Result.LinearSampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
            VkDescriptorImageWrite(&RenderState->DescriptorManager, Result.Descriptors[1], 2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                   Result.VelocityImages[1].View, Result.LinearSampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
            VkDescriptorImageWrite(&RenderState->DescriptorManager, Result.Descriptors[1], 3, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                                   Result.ColorImages[0].View, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_GENERAL);
            VkDescriptorImageWrite(&RenderState->DescriptorManager, Result.Descriptors[1], 4, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                                   Result.VelocityImages[0].View, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_GENERAL);

            VkDescriptorImageWrite(&RenderState->DescriptorManager, Result.Descriptors[1], 5, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                                   Result.DivergenceImage.View, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_GENERAL);
        }

        {
            Result.RenderDescriptors[0] = VkDescriptorSetAllocate(RenderState->Device, RenderState->DescriptorPool, RenderState->CopyImageDescLayout);
            VkDescriptorImageWrite(&RenderState->DescriptorManager, Result.RenderDescriptors[0], 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                   Result.ColorImages[0].View, DemoState->LinearSampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        }

        {
            Result.RenderDescriptors[1] = VkDescriptorSetAllocate(RenderState->Device, RenderState->DescriptorPool, RenderState->CopyImageDescLayout);
            VkDescriptorImageWrite(&RenderState->DescriptorManager, Result.RenderDescriptors[1], 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                   Result.ColorImages[1].View, DemoState->LinearSampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        }

        {
            Result.PressureDescriptors[0] = VkDescriptorSetAllocate(RenderState->Device, RenderState->DescriptorPool, Result.PressureDescLayout);
            VkDescriptorImageWrite(&RenderState->DescriptorManager, Result.PressureDescriptors[0], 0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                                   Result.PressureImages[0].View, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_GENERAL);
            //VkDescriptorImageWrite(&RenderState->DescriptorManager, Result.PressureDescriptors[0], 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            //                       Result.PressureImages[0].View, Result.PointSampler, VK_IMAGE_LAYOUT_GENERAL);
            VkDescriptorImageWrite(&RenderState->DescriptorManager, Result.PressureDescriptors[0], 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                                   Result.PressureImages[1].View, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_GENERAL);
        }

        {
            Result.PressureDescriptors[1] = VkDescriptorSetAllocate(RenderState->Device, RenderState->DescriptorPool, Result.PressureDescLayout);
            VkDescriptorImageWrite(&RenderState->DescriptorManager, Result.PressureDescriptors[1], 0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                                   Result.PressureImages[1].View, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_GENERAL);
            //VkDescriptorImageWrite(&RenderState->DescriptorManager, Result.PressureDescriptors[1], 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            //                       Result.PressureImages[1].View, Result.PointSampler, VK_IMAGE_LAYOUT_GENERAL);
            VkDescriptorImageWrite(&RenderState->DescriptorManager, Result.PressureDescriptors[1], 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                                   Result.PressureImages[0].View, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_GENERAL);
        }
    }
    
    // NOTE: Init Pipeline
    {
        VkDescriptorSetLayout Layouts[] = 
            {
                Result.DescLayout,
                Result.PressureDescLayout,
            };
            
        Result.InitPipeline = VkPipelineComputeCreate(RenderState->Device, &RenderState->PipelineManager, &DemoState->TempArena,
                                                      "fluid_init.spv", "main", Layouts, ArrayCount(Layouts));
    }

    // NOTE: Advection Velocity Pipeline
    {
        VkDescriptorSetLayout Layouts[] = 
            {
                Result.DescLayout,
                Result.PressureDescLayout,
            };
            
        Result.AdvectionVelPipeline = VkPipelineComputeCreate(RenderState->Device, &RenderState->PipelineManager, &DemoState->TempArena,
                                                              "fluid_advection.spv", "main", Layouts, ArrayCount(Layouts));
    }

    // NOTE: Divergence Pipeline
    {
        VkDescriptorSetLayout Layouts[] = 
            {
                Result.DescLayout,
                Result.PressureDescLayout,
            };
            
        Result.DivergencePipeline = VkPipelineComputeCreate(RenderState->Device, &RenderState->PipelineManager, &DemoState->TempArena,
                                                            "fluid_divergence.spv", "main", Layouts, ArrayCount(Layouts));
    }

    // TODO: REMOVE
    {
        VkDescriptorSetLayout Layouts[] = 
            {
                Result.DescLayout,
                Result.PressureDescLayout,
            };
            
        Result.TestDivergencePipeline = VkPipelineComputeCreate(RenderState->Device, &RenderState->PipelineManager, &DemoState->TempArena,
                                                                "fluid_test_divergence.spv", "main", Layouts, ArrayCount(Layouts));
    }

    // NOTE: Pressure Iteration Pipeline
    {
        VkDescriptorSetLayout Layouts[] = 
            {
                Result.DescLayout,
                Result.PressureDescLayout,
            };
            
        Result.PressureIterationPipeline = VkPipelineComputeCreate(RenderState->Device, &RenderState->PipelineManager, &DemoState->TempArena,
                                                                   "fluid_pressure_iteration.spv", "main", Layouts, ArrayCount(Layouts));
    }

    // NOTE: Pressure Apply Pipeline
    {
        VkDescriptorSetLayout Layouts[] = 
            {
                Result.DescLayout,
                Result.PressureDescLayout,
            };
            
        Result.PressureApplyPipeline = VkPipelineComputeCreate(RenderState->Device, &RenderState->PipelineManager, &DemoState->TempArena,
                                                               "fluid_pressure_apply.spv", "main", Layouts, ArrayCount(Layouts));
    }

    // NOTE: Render Pipeline
    {
        Result.CopyToRtPipeline = FullScreenCopyImageCreate(RenderPass, SubPassId);
    }

    VkDescriptorManagerFlush(RenderState->Device, &RenderState->DescriptorManager);
    
    return Result;
}

inline void FluidSimSwap(vk_commands Commands, fluid_sim* FluidSim)
{
    // NOTE: Convert input images to output
    VkPipelineStageFlags StageFlags = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
    VkBarrierImageAdd(&RenderState->BarrierManager, VK_ACCESS_SHADER_READ_BIT, StageFlags,
                      VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_SHADER_WRITE_BIT, StageFlags,
                      VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_ASPECT_COLOR_BIT, FluidSim->ColorImages[FluidSim->InputId].Image);
    VkBarrierImageAdd(&RenderState->BarrierManager, VK_ACCESS_SHADER_READ_BIT, StageFlags,
                      VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_SHADER_WRITE_BIT, StageFlags,
                      VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_ASPECT_COLOR_BIT, FluidSim->VelocityImages[FluidSim->InputId].Image);

    FluidSim->InputId = !FluidSim->InputId;
    
    // NOTE: Convert output images to input
    VkBarrierImageAdd(&RenderState->BarrierManager, VK_ACCESS_SHADER_WRITE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                      VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_SHADER_READ_BIT, StageFlags,
                      VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT, FluidSim->ColorImages[FluidSim->InputId].Image);
    VkBarrierImageAdd(&RenderState->BarrierManager, VK_ACCESS_SHADER_WRITE_BIT, StageFlags,
                      VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                      VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT, FluidSim->VelocityImages[FluidSim->InputId].Image);
    
    VkBarrierManagerFlush(&RenderState->BarrierManager, Commands.Buffer);
    
}

inline void FluidSimInit(vk_commands Commands, fluid_sim* FluidSim)
{
    VkBarrierImageAdd(&RenderState->BarrierManager, VK_ACCESS_MEMORY_READ_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                      VK_IMAGE_LAYOUT_UNDEFINED, VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                      VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT, FluidSim->ColorImages[FluidSim->InputId].Image);
    VkBarrierImageAdd(&RenderState->BarrierManager, VK_ACCESS_MEMORY_READ_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                      VK_IMAGE_LAYOUT_UNDEFINED, VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                      VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT, FluidSim->VelocityImages[FluidSim->InputId].Image);

    VkBarrierImageAdd(&RenderState->BarrierManager, VK_ACCESS_MEMORY_READ_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                      VK_IMAGE_LAYOUT_UNDEFINED, VK_ACCESS_SHADER_WRITE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                      VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_ASPECT_COLOR_BIT, FluidSim->ColorImages[!FluidSim->InputId].Image);
    VkBarrierImageAdd(&RenderState->BarrierManager, VK_ACCESS_MEMORY_READ_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                      VK_IMAGE_LAYOUT_UNDEFINED, VK_ACCESS_SHADER_WRITE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                      VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_ASPECT_COLOR_BIT, FluidSim->VelocityImages[!FluidSim->InputId].Image);

    // NOTE: Init images from undefined
    VkBarrierImageAdd(&RenderState->BarrierManager, VK_ACCESS_MEMORY_READ_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                      VK_IMAGE_LAYOUT_UNDEFINED, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                      VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_ASPECT_COLOR_BIT, FluidSim->DivergenceImage.Image);
    VkBarrierImageAdd(&RenderState->BarrierManager, VK_ACCESS_MEMORY_READ_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                      VK_IMAGE_LAYOUT_UNDEFINED, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                      VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_ASPECT_COLOR_BIT, FluidSim->PressureImages[0].Image);
    VkBarrierImageAdd(&RenderState->BarrierManager, VK_ACCESS_MEMORY_READ_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                      VK_IMAGE_LAYOUT_UNDEFINED, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                      VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_ASPECT_COLOR_BIT, FluidSim->PressureImages[1].Image);
    
    VkBarrierManagerFlush(&RenderState->BarrierManager, Commands.Buffer);

    {
        VkDescriptorSet DescriptorSets[] =
            {
                FluidSim->Descriptors[FluidSim->InputId]
            };
        u32 DispatchX = CeilU32(f32(FluidSim->Width) / 8.0f);
        u32 DispatchY = CeilU32(f32(FluidSim->Height) / 8.0f);
        VkComputeDispatch(Commands, FluidSim->InitPipeline, DescriptorSets, ArrayCount(DescriptorSets), DispatchX, DispatchY, 1);
    }

    FluidSimSwap(Commands, FluidSim);
}

inline void FluidSimFrameBegin(fluid_sim* FluidSim, f32 FrameTime)
{
    // TODO: Make framework calculate correct frametime
    FluidSim->UniformsCpu.FrameTime = FrameTime;
    // TODO: Make this configurable
    FluidSim->UniformsCpu.Density = 1;
    Assert(FluidSim->Width == FluidSim->Height);
    FluidSim->UniformsCpu.Epsilon = 1.0f / f32(FluidSim->Width);
    FluidSim->UniformsCpu.Dim = FluidSim->Width;
    
    fluid_sim_inputs* GpuData = VkTransferPushWriteStruct(&RenderState->TransferManager, FluidSim->UniformBuffer, fluid_sim_inputs,
                                                     BarrierMask(VkAccessFlagBits(0), VK_PIPELINE_STAGE_ALL_COMMANDS_BIT),
                                                     BarrierMask(VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT));
    Copy(&FluidSim->UniformsCpu, GpuData, sizeof(fluid_sim_inputs));
}

inline void FluidSimSimulate(vk_commands Commands, fluid_sim* FluidSim)
{
    VkDescriptorSet DescriptorSets[] =
        {
            FluidSim->Descriptors[FluidSim->InputId],
            FluidSim->PressureDescriptors[0],
        };
    u32 DispatchX = CeilU32(f32(FluidSim->Width) / 8.0f);
    u32 DispatchY = CeilU32(f32(FluidSim->Height) / 8.0f);

    // NOTE: Advection (Velocity) + Clear Pressure
    VkComputeDispatch(Commands, FluidSim->AdvectionVelPipeline, DescriptorSets, ArrayCount(DescriptorSets), DispatchX, DispatchY, 1);
    
    VkBarrierImageAdd(&RenderState->BarrierManager, VK_ACCESS_SHADER_WRITE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                      VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                      VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_ASPECT_COLOR_BIT, FluidSim->VelocityImages[!FluidSim->InputId].Image);
    VkBarrierImageAdd(&RenderState->BarrierManager, VK_ACCESS_SHADER_WRITE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                      VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                      VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_ASPECT_COLOR_BIT, FluidSim->PressureImages[0].Image);
    VkBarrierManagerFlush(&RenderState->BarrierManager, Commands.Buffer);
    
    // NOTE: Divergence
    VkComputeDispatch(Commands, FluidSim->DivergencePipeline, DescriptorSets, ArrayCount(DescriptorSets), DispatchX, DispatchY, 1);

    VkBarrierImageAdd(&RenderState->BarrierManager, VK_ACCESS_SHADER_WRITE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                      VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                      VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_ASPECT_COLOR_BIT, FluidSim->DivergenceImage.Image);
    VkBarrierManagerFlush(&RenderState->BarrierManager, Commands.Buffer);

    // NOTE: Pressure Iteration
    for (u32 IterationId = 0; IterationId < 10; ++IterationId)
    {
        VkDescriptorSet NewDescriptorSets[] =
        {
            FluidSim->Descriptors[FluidSim->InputId],
            FluidSim->PressureDescriptors[(IterationId + 1) % 2],
        };
        
        VkComputeDispatch(Commands, FluidSim->PressureIterationPipeline, NewDescriptorSets, ArrayCount(NewDescriptorSets), DispatchX, DispatchY, 1);

        VkBarrierImageAdd(&RenderState->BarrierManager, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                          VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                          VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_ASPECT_COLOR_BIT, FluidSim->PressureImages[IterationId % 2].Image);
        VkBarrierImageAdd(&RenderState->BarrierManager, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                          VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                          VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_ASPECT_COLOR_BIT, FluidSim->PressureImages[(IterationId + 1) % 2].Image);
        VkBarrierManagerFlush(&RenderState->BarrierManager, Commands.Buffer);
    }

    // NOTE: Pressure Apply and Advect Color
    {
        VkDescriptorSet NewDescriptorSets[] =
        {
            FluidSim->Descriptors[FluidSim->InputId],
            FluidSim->PressureDescriptors[1],
        };
        VkComputeDispatch(Commands, FluidSim->PressureApplyPipeline, NewDescriptorSets, ArrayCount(NewDescriptorSets), DispatchX, DispatchY, 1);
    }

    VkBarrierImageAdd(&RenderState->BarrierManager, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                      VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                      VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_ASPECT_COLOR_BIT, FluidSim->VelocityImages[!FluidSim->InputId].Image);
    VkBarrierManagerFlush(&RenderState->BarrierManager, Commands.Buffer);

    VkComputeDispatch(Commands, FluidSim->DivergencePipeline, DescriptorSets, ArrayCount(DescriptorSets), DispatchX, DispatchY, 1);
    
    FluidSimSwap(Commands, FluidSim);
}
