
/*

  NOTE: Sources:

    - http://jamie-wong.com/2016/08/05/webgl-fluid-simulation/ (IMPLEMENTED)
    - http://developer.download.nvidia.com/books/HTML/gpugems/gpugems_ch38.html
    - https://developer.nvidia.com/gpugems/gpugems3/part-v-physics-simulation/chapter-30-real-time-simulation-and-rendering-3d-fluids
    - https://www.shahinrabbani.ca/torch2pd.html
    - 
  
 */
                                                                        \
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
                                          VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_IMAGE_ASPECT_COLOR_BIT);
    Result.PressureImages[0] = VkImageCreate(RenderState->Device, &Result.Arena, Width, Height, VK_FORMAT_R32_SFLOAT,
                                             VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_IMAGE_ASPECT_COLOR_BIT);
    Result.PressureImages[1] = VkImageCreate(RenderState->Device, &Result.Arena, Width, Height, VK_FORMAT_R32_SFLOAT,
                                             VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_IMAGE_ASPECT_COLOR_BIT);

    {
        vk_descriptor_layout_builder Builder = VkDescriptorLayoutBegin(&Result.GlobalDescLayout);
        VkDescriptorLayoutAdd(&Builder, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT);
        VkDescriptorLayoutAdd(&Builder, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT);
        VkDescriptorLayoutEnd(RenderState->Device, &Builder);
    }
    
    // NOTE: Init Pipeline
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
                                                      "fluid_diffusion_init.spv", "main", Layouts, ArrayCount(Layouts));
    }

    // NOTE: Advection Velocity Pipeline
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
            
        Result.AdvectionVelPipeline = VkPipelineComputeCreate(RenderState->Device, &RenderState->PipelineManager, &DemoState->TempArena,
                                                              "fluid_advection.spv", "main", Layouts, ArrayCount(Layouts));
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
            VkDescriptorLayoutAdd(&Builder, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT);
            VkDescriptorLayoutAdd(&Builder, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT);
            VkDescriptorLayoutEnd(RenderState->Device, &Builder);
        }

        VkDescriptorSetLayout Layouts[] = 
            {
                Result.GlobalDescLayout,
                Result.PressureDescLayout, // NOTE: This is a null descriptor layout really
                Result.PressureDescLayout,
            };
            
        Result.PressureIterationPipeline = VkPipelineComputeCreate(RenderState->Device, &RenderState->PipelineManager, &DemoState->TempArena,
                                                                   "fluid_pressure_iteration.spv", "main", Layouts, ArrayCount(Layouts));
    }

    // NOTE: Pressure Apply Pipeline
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
                                                               "fluid_pressure_apply.spv", "main", Layouts, ArrayCount(Layouts));
    }

    // NOTE: Render Pipeline
    {
        Result.CopyToRtPipeline = FullScreenCopyImageCreate(RenderPass, SubPassId);
    }

    // NOTE: Populate descriptors
    {
        // NOTE: Global Descriptor
        {
            Result.GlobalDescriptor = VkDescriptorSetAllocate(RenderState->Device, RenderState->DescriptorPool, Result.GlobalDescLayout);
            VkDescriptorBufferWrite(&RenderState->DescriptorManager, Result.GlobalDescriptor, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, Result.UniformBuffer);
            VkDescriptorImageWrite(&RenderState->DescriptorManager, Result.GlobalDescriptor, 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                                   Result.DivergenceImage.View, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_GENERAL);
        }

        // NOTE: Init Descriptor
        {
            Result.InitDescriptor = VkDescriptorSetAllocate(RenderState->Device, RenderState->DescriptorPool, Result.InitDescLayout);
            VkDescriptorImageWrite(&RenderState->DescriptorManager, Result.InitDescriptor, 0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                                   Result.ColorImages[1].View, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_GENERAL);
            VkDescriptorImageWrite(&RenderState->DescriptorManager, Result.InitDescriptor, 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                                   Result.VelocityImages[1].View, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_GENERAL);
        }

        // NOTE: Advection Descriptor (ping pong)
        {
            Result.AdvectionDescriptors[0] = VkDescriptorSetAllocate(RenderState->Device, RenderState->DescriptorPool, Result.AdvectionDescLayout);
            VkDescriptorImageWrite(&RenderState->DescriptorManager, Result.AdvectionDescriptors[0], 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                   Result.VelocityImages[0].View, Result.LinearSampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
            VkDescriptorImageWrite(&RenderState->DescriptorManager, Result.AdvectionDescriptors[0], 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                                   Result.VelocityImages[1].View, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_GENERAL);

            Result.AdvectionDescriptors[1] = VkDescriptorSetAllocate(RenderState->Device, RenderState->DescriptorPool, Result.AdvectionDescLayout);
            VkDescriptorImageWrite(&RenderState->DescriptorManager, Result.AdvectionDescriptors[1], 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                   Result.VelocityImages[1].View, Result.LinearSampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
            VkDescriptorImageWrite(&RenderState->DescriptorManager, Result.AdvectionDescriptors[1], 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                                   Result.VelocityImages[0].View, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_GENERAL);
        }

        // NOTE: Divergence Descriptor (ping pong)
        {
            Result.DivergenceDescriptors[0] = VkDescriptorSetAllocate(RenderState->Device, RenderState->DescriptorPool, Result.DivergenceDescLayout);
            VkDescriptorImageWrite(&RenderState->DescriptorManager, Result.DivergenceDescriptors[0], 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                   Result.VelocityImages[1].View, Result.PointSampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

            Result.DivergenceDescriptors[1] = VkDescriptorSetAllocate(RenderState->Device, RenderState->DescriptorPool, Result.DivergenceDescLayout);
            VkDescriptorImageWrite(&RenderState->DescriptorManager, Result.DivergenceDescriptors[1], 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                   Result.VelocityImages[0].View, Result.PointSampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        }

        // NOTE: Pressure Descriptor
        {
            Result.PressureDescriptors[0] = VkDescriptorSetAllocate(RenderState->Device, RenderState->DescriptorPool, Result.PressureDescLayout);
            VkDescriptorImageWrite(&RenderState->DescriptorManager, Result.PressureDescriptors[0], 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                   Result.PressureImages[0].View, Result.PointSampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
            VkDescriptorImageWrite(&RenderState->DescriptorManager, Result.PressureDescriptors[0], 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                                   Result.PressureImages[1].View, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_GENERAL);

            Result.PressureDescriptors[1] = VkDescriptorSetAllocate(RenderState->Device, RenderState->DescriptorPool, Result.PressureDescLayout);
            VkDescriptorImageWrite(&RenderState->DescriptorManager, Result.PressureDescriptors[1], 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                   Result.PressureImages[1].View, Result.PointSampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
            VkDescriptorImageWrite(&RenderState->DescriptorManager, Result.PressureDescriptors[1], 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                                   Result.PressureImages[0].View, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_GENERAL);
        }

        // NOTE: Pressure Apply Descriptor
        {
            Result.PressureApplyDescriptors[0] = VkDescriptorSetAllocate(RenderState->Device, RenderState->DescriptorPool, Result.PressureApplyDescLayout);
            VkDescriptorImageWrite(&RenderState->DescriptorManager, Result.PressureApplyDescriptors[0], 0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                                   Result.VelocityImages[1].View, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_GENERAL);
            VkDescriptorImageWrite(&RenderState->DescriptorManager, Result.PressureApplyDescriptors[0], 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                   Result.ColorImages[0].View, Result.LinearSampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
            VkDescriptorImageWrite(&RenderState->DescriptorManager, Result.PressureApplyDescriptors[0], 2, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                                   Result.ColorImages[1].View, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_GENERAL);

            Result.PressureApplyDescriptors[1] = VkDescriptorSetAllocate(RenderState->Device, RenderState->DescriptorPool, Result.PressureApplyDescLayout);
            VkDescriptorImageWrite(&RenderState->DescriptorManager, Result.PressureApplyDescriptors[1], 0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                                   Result.VelocityImages[0].View, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_GENERAL);
            VkDescriptorImageWrite(&RenderState->DescriptorManager, Result.PressureApplyDescriptors[1], 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                   Result.ColorImages[1].View, Result.LinearSampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
            VkDescriptorImageWrite(&RenderState->DescriptorManager, Result.PressureApplyDescriptors[1], 2, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                                   Result.ColorImages[0].View, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_GENERAL);
        }
        
        // NOTE: Render Descriptors
        {
            Result.RenderDescriptors[0] = VkDescriptorSetAllocate(RenderState->Device, RenderState->DescriptorPool, RenderState->CopyImageDescLayout);
            VkDescriptorImageWrite(&RenderState->DescriptorManager, Result.RenderDescriptors[0], 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                   Result.ColorImages[1].View, DemoState->LinearSampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
            
            Result.RenderDescriptors[1] = VkDescriptorSetAllocate(RenderState->Device, RenderState->DescriptorPool, RenderState->CopyImageDescLayout);
            VkDescriptorImageWrite(&RenderState->DescriptorManager, Result.RenderDescriptors[1], 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                   Result.ColorImages[0].View, DemoState->LinearSampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        }
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
    
    // NOTE: Convert output images to input (color is already in read only)
    VkBarrierImageAdd(&RenderState->BarrierManager, VK_ACCESS_SHADER_WRITE_BIT, StageFlags,
                      VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                      VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT, FluidSim->VelocityImages[FluidSim->InputId].Image);
    
    VkBarrierManagerFlush(&RenderState->BarrierManager, Commands.Buffer);
    
}

inline void FluidSimInit(vk_commands Commands, fluid_sim* FluidSim)
{
    VkBarrierImageAdd(&RenderState->BarrierManager, VK_ACCESS_MEMORY_READ_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                      VK_IMAGE_LAYOUT_UNDEFINED, VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                      VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT, FluidSim->VelocityImages[0].Image);
    VkBarrierImageAdd(&RenderState->BarrierManager, VK_ACCESS_MEMORY_READ_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                      VK_IMAGE_LAYOUT_UNDEFINED, VK_ACCESS_SHADER_WRITE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                      VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_ASPECT_COLOR_BIT, FluidSim->VelocityImages[1].Image);
    
    VkBarrierImageAdd(&RenderState->BarrierManager, VK_ACCESS_MEMORY_READ_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                      VK_IMAGE_LAYOUT_UNDEFINED, VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                      VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT, FluidSim->ColorImages[0].Image);
    VkBarrierImageAdd(&RenderState->BarrierManager, VK_ACCESS_MEMORY_READ_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                      VK_IMAGE_LAYOUT_UNDEFINED, VK_ACCESS_SHADER_WRITE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                      VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_ASPECT_COLOR_BIT, FluidSim->ColorImages[1].Image);

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
                FluidSim->GlobalDescriptor,
                FluidSim->InitDescriptor,
            };
        u32 DispatchX = CeilU32(f32(FluidSim->Width) / 8.0f);
        u32 DispatchY = CeilU32(f32(FluidSim->Height) / 8.0f);
        VkComputeDispatch(Commands, FluidSim->InitPipeline, DescriptorSets, ArrayCount(DescriptorSets), DispatchX, DispatchY, 1);
    }

    VkBarrierImageAdd(&RenderState->BarrierManager, VK_ACCESS_SHADER_WRITE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                      VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                      VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT, FluidSim->ColorImages[1].Image);
    VkBarrierManagerFlush(&RenderState->BarrierManager, Commands.Buffer);
}

inline void FluidSimFrameBegin(fluid_sim* FluidSim, f32 FrameTime)
{
    // TODO: Make framework calculate correct frametime
    FluidSim->UniformsCpu.FrameTime = 1.0f / 120.0f; //0.25f*FrameTime;
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
    FluidSimSwap(Commands, FluidSim);

    u32 DispatchX = CeilU32(f32(FluidSim->Width) / 8.0f);
    u32 DispatchY = CeilU32(f32(FluidSim->Height) / 8.0f);
    u32 PressureInput = 0;

    // NOTE: Advection (Velocity) + Clear Pressure
    {
        VkDescriptorSet DescriptorSets[] =
        {
            FluidSim->GlobalDescriptor,
            FluidSim->AdvectionDescriptors[FluidSim->InputId],
        };

        VkComputeDispatch(Commands, FluidSim->AdvectionVelPipeline, DescriptorSets, ArrayCount(DescriptorSets), DispatchX, DispatchY, 1);
    }
    
    VkBarrierImageAdd(&RenderState->BarrierManager, VK_ACCESS_SHADER_WRITE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                      VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                      VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT, FluidSim->VelocityImages[!FluidSim->InputId].Image);
    VkBarrierImageAdd(&RenderState->BarrierManager, VK_ACCESS_SHADER_WRITE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                      VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                      VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT, FluidSim->PressureImages[PressureInput].Image);
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
    for (u32 IterationId = 0; IterationId < 5; ++IterationId)
    {
        VkDescriptorSet DescriptorSets[] =
        {
            FluidSim->GlobalDescriptor,
            FluidSim->PressureDescriptors[PressureInput], // NOTE: This is really a null entry
            FluidSim->PressureDescriptors[PressureInput],
        };
        
        VkComputeDispatch(Commands, FluidSim->PressureIterationPipeline, DescriptorSets, ArrayCount(DescriptorSets), DispatchX, DispatchY, 1);

        VkBarrierImageAdd(&RenderState->BarrierManager, VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                          VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_SHADER_WRITE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                          VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_ASPECT_COLOR_BIT, FluidSim->PressureImages[PressureInput].Image);
        VkBarrierImageAdd(&RenderState->BarrierManager, VK_ACCESS_SHADER_WRITE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                          VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                          VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT, FluidSim->PressureImages[!PressureInput].Image);
        VkBarrierManagerFlush(&RenderState->BarrierManager, Commands.Buffer);

        PressureInput = !PressureInput;
    }
    
    VkBarrierImageAdd(&RenderState->BarrierManager, VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                      VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                      VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_ASPECT_COLOR_BIT, FluidSim->VelocityImages[!FluidSim->InputId].Image);
    VkBarrierManagerFlush(&RenderState->BarrierManager, Commands.Buffer);
    
    // NOTE: Pressure Apply and Advect Color
    {
        VkDescriptorSet DescriptorSets[] =
        {
            FluidSim->GlobalDescriptor,
            FluidSim->PressureApplyDescriptors[FluidSim->InputId],
            FluidSim->PressureDescriptors[PressureInput],
        };
        VkComputeDispatch(Commands, FluidSim->PressureApplyPipeline, DescriptorSets, ArrayCount(DescriptorSets), DispatchX, DispatchY, 1);
    }

    // NOTE: Convert for rendering
    VkBarrierImageAdd(&RenderState->BarrierManager, VK_ACCESS_SHADER_WRITE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                      VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                      VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT, FluidSim->ColorImages[!FluidSim->InputId].Image);
    
    // NOTE: Convert to default states
    VkBarrierImageAdd(&RenderState->BarrierManager, VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                      VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_SHADER_WRITE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                      VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_ASPECT_COLOR_BIT, FluidSim->DivergenceImage.Image);
    VkBarrierImageAdd(&RenderState->BarrierManager, VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                      VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_SHADER_WRITE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                      VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_ASPECT_COLOR_BIT, FluidSim->PressureImages[PressureInput].Image);
    VkBarrierManagerFlush(&RenderState->BarrierManager, Commands.Buffer);
}
