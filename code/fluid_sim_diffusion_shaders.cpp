#version 450

#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_nonuniform_qualifier: enable

#include "fluid_sim_shader_globals.cpp"

layout(set = 0, binding = 0) uniform fluid_sim_inputs
{
    vec2 MousePos;
    vec2 DeltaMousePos;
    
    float FrameTime;
    float Density;
    float Epsilon;
    uint Dim;
    
    vec2 SplatCenter;
    float SplatRadius;
} FluidSimInputs;

layout(set = 0, binding = 1, r32f) uniform image2D DivergenceImage;

vec2 GetUv(vec2 InvocationId)
{
    vec2 Result = (InvocationId + vec2(0.5)) / vec2(FluidSimInputs.Dim, FluidSimInputs.Dim);
    return Result;
}

vec2 GetUv()
{
    return GetUv(gl_GlobalInvocationID.xy);
}

//
// NOTE: Diffusion Fluid Init
//

#if INIT

layout(set = 1, binding = 0, rgba32f) uniform image2D OutColorImage;
layout(set = 1, binding = 1, rg32f) uniform image2D OutVelocityImage;

layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;
void main()
{
    vec2 TextureSize = vec2(FluidSimInputs.Dim, FluidSimInputs.Dim);
    if (gl_GlobalInvocationID.x < TextureSize.x && gl_GlobalInvocationID.y < TextureSize.y)
    {
        vec2 OutVelocity;

        // NOTE: Write circular vector field
        {
            vec2 Pos = GetUv();
            Pos = 2*Pos - vec2(1);
            OutVelocity = 0.5f*vec2(sin(2*Pi32*Pos.y), sin(2*Pi32*Pos.x));
            imageStore(OutVelocityImage, ivec2(gl_GlobalInvocationID.xy), vec4(OutVelocity, 0, 0));
        }
        
        // NOTE: Write color image
        {
            vec2 Uv = GetUv();
            Uv = 2*Uv - vec2(1);
#if 1
            vec4 OutColor = vec4(step(1.0, mod(floor((Uv.x + 1.0) / 0.2) + floor((Uv.y + 1.0) / 0.2), 2.0)),
                                 step(1.0, mod(floor((Uv.x + 1.0) / 0.3) + floor((Uv.y + 1.0) / 0.3), 2.0)),
                                 step(1.0, mod(floor((Uv.x + 1.0) / 0.4) + floor((Uv.y + 1.0) / 0.4), 2.0)),
                                 1);
#else
            vec4 OutColor = vec4(step(1.0, mod(floor((Uv.x + 1.0) / 0.2) + floor((Uv.y + 1.0) / 0.2), 2.0)),
                                 step(1.0, mod(floor((Uv.x + 1.0) / 0.2) + floor((Uv.y + 1.0) / 0.2), 2.0)),
                                 step(1.0, mod(floor((Uv.x + 1.0) / 0.2) + floor((Uv.y + 1.0) / 0.2), 2.0)),
                                 1);
#endif
            
            imageStore(OutColorImage, ivec2(gl_GlobalInvocationID.xy), OutColor);
        }
    }    
}

#endif

//
// NOTE: Advection
//

#if ADVECTION

layout(set = 1, binding = 0) uniform sampler2D InVelocityImage;
layout(set = 1, binding = 1, rg32f) uniform image2D OutVelocityImage;

layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;
void main()
{
    vec2 TextureSize = vec2(FluidSimInputs.Dim, FluidSimInputs.Dim);
    if (gl_GlobalInvocationID.x < TextureSize.x && gl_GlobalInvocationID.y < TextureSize.y)
    {        
        // NOTE: Advect the velocity field
        ivec2 PixelCoord = ivec2(gl_GlobalInvocationID.xy);
        vec2 CurrVel = texelFetch(InVelocityImage, PixelCoord, 0).xy;
        vec2 SamplePos = AdvectionFindPos(CurrVel, gl_GlobalInvocationID.xy, TextureSize, FluidSimInputs.FrameTime);
        vec2 NewVelocity = texture(InVelocityImage, SamplePos).xy;
        imageStore(OutVelocityImage, PixelCoord, vec4(NewVelocity, 0, 0));
    }
}

#endif

//
// NOTE: Divergence
//

#if DIVERGENCE

layout(set = 1, binding = 0) uniform sampler2D InVelocityImage;

layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;
void main()
{
    vec2 TextureSize = vec2(FluidSimInputs.Dim, FluidSimInputs.Dim);
    if (gl_GlobalInvocationID.x < TextureSize.x && gl_GlobalInvocationID.y < TextureSize.y)
    {
        vec2 Uv = GetUv();

        float VelRight = textureOffset(InVelocityImage, Uv, ivec2(1, 0)).x;
        float VelLeft = textureOffset(InVelocityImage, Uv, -ivec2(1, 0)).x;
        float VelUp = textureOffset(InVelocityImage, Uv, ivec2(0, 1)).y;
        float VelDown = textureOffset(InVelocityImage, Uv, -ivec2(0, 1)).y;

        // IMPORTANT: Seems that removing the sub brackets changes the precision or something because results look different
        float Multiplier = ((-2.0 * FluidSimInputs.Epsilon * FluidSimInputs.Density) / FluidSimInputs.FrameTime);
        float Divergence = Multiplier * ((VelRight - VelLeft) + (VelUp - VelDown));
        imageStore(DivergenceImage, ivec2(gl_GlobalInvocationID.xy), vec4(Divergence, 0, 0, 0));
    }    
}

#endif

//
// NOTE: Pressure Iteration (Jacobis Method)
//

#if PRESSURE_ITERATION

layout(set = 1, binding = 0, r32f) uniform image2D InPressureImage;
layout(set = 1, binding = 1, r32f) uniform image2D OutPressureImage;

layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;
void main()
{
    vec2 TextureSize = vec2(FluidSimInputs.Dim, FluidSimInputs.Dim);
    if (gl_GlobalInvocationID.x < TextureSize.x && gl_GlobalInvocationID.y < TextureSize.y)
    {
        ivec2 CenterCoord = ivec2(gl_GlobalInvocationID.xy);
        ivec2 LeftCoord = MirrorPixelCoord(CenterCoord - ivec2(2, 0), ivec2(TextureSize));
        ivec2 RightCoord = MirrorPixelCoord(CenterCoord + ivec2(2, 0), ivec2(TextureSize));
        ivec2 UpCoord = MirrorPixelCoord(CenterCoord + ivec2(0, 2), ivec2(TextureSize));
        ivec2 DownCoord = MirrorPixelCoord(CenterCoord - ivec2(0, 2), ivec2(TextureSize));
        
        float PressureCenter = imageLoad(InPressureImage, CenterCoord).x;
        float PressureLeft = imageLoad(InPressureImage, LeftCoord).x;
        float PressureRight = imageLoad(InPressureImage, RightCoord).x;
        float PressureUp = imageLoad(InPressureImage, UpCoord).x;
        float PressureDown = imageLoad(InPressureImage, DownCoord).x;
        
        float Divergence = imageLoad(DivergenceImage, CenterCoord).x;
        float NewPressureCenter = (Divergence + PressureRight + PressureLeft + PressureUp + PressureDown) * 0.25f;

        imageStore(OutPressureImage, CenterCoord, vec4(NewPressureCenter, 0, 0, 0));
    }
}

#endif

//
// NOTE: Diffusion Pressure Application
//

#if PRESSURE_APPLY

layout(set = 1, binding = 0, rg32f) uniform image2D VelocityImage;

layout(set = 1, binding = 1) uniform sampler2D InColorImage;
layout(set = 1, binding = 2, rgba32f) uniform image2D OutColorImage;

layout(set = 2, binding = 0, r32f) uniform image2D InPressureImage;
layout(set = 2, binding = 1, r32f) uniform image2D OutPressureImage;

layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;
void main()
{
    vec2 TextureSize = vec2(FluidSimInputs.Dim, FluidSimInputs.Dim);
    if (gl_GlobalInvocationID.x < TextureSize.x && gl_GlobalInvocationID.y < TextureSize.y)
    {
        ivec2 CenterCoord = ivec2(gl_GlobalInvocationID.xy);
        
        // NOTE: Apply pressure
        vec2 CorrectedVel;
        {
            ivec2 LeftCoord = MirrorPixelCoord(CenterCoord - ivec2(1, 0), ivec2(TextureSize));
            ivec2 RightCoord = MirrorPixelCoord(CenterCoord + ivec2(1, 0), ivec2(TextureSize));
            ivec2 UpCoord = MirrorPixelCoord(CenterCoord + ivec2(0, 1), ivec2(TextureSize));
            ivec2 DownCoord = MirrorPixelCoord(CenterCoord - ivec2(0, 1), ivec2(TextureSize));

            float PressureLeft = imageLoad(InPressureImage, LeftCoord).x;
            float PressureRight = imageLoad(InPressureImage, RightCoord).x;
            float PressureUp = imageLoad(InPressureImage, UpCoord).x;
            float PressureDown = imageLoad(InPressureImage, DownCoord).x;

            float Multiplier = (FluidSimInputs.FrameTime / (2*FluidSimInputs.Density*FluidSimInputs.Epsilon));
            vec2 AdvectedVel = imageLoad(VelocityImage, CenterCoord).xy;
            CorrectedVel.x = AdvectedVel.x - Multiplier * (PressureRight - PressureLeft);
            CorrectedVel.y = AdvectedVel.y - Multiplier * (PressureUp - PressureDown);
            
            imageStore(VelocityImage, CenterCoord, vec4(CorrectedVel, 0, 0));
        }

        // NOTE: Advect the color
        {
            vec2 SamplePos = AdvectionFindPos(CorrectedVel, CenterCoord, TextureSize, FluidSimInputs.FrameTime);
            vec4 NewColor = texture(InColorImage, SamplePos);
            imageStore(OutColorImage, CenterCoord, NewColor);
        }
    }
}

#endif
