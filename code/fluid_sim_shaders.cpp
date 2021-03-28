#version 450

#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_nonuniform_qualifier: enable

#include "fluid_sim_shader_globals.cpp"

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
// NOTE: Pressure Mirror Iteration (Jacobis Method)
//

#if PRESSURE_MIRROR_ITERATION

layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;
void main()
{
    vec2 TextureSize = vec2(FluidSimInputs.Dim, FluidSimInputs.Dim);
    if (gl_GlobalInvocationID.x < TextureSize.x && gl_GlobalInvocationID.y < TextureSize.y)
    {
        float PressureCenter = imageLoad(InPressureImage, ivec2(gl_GlobalInvocationID.xy)).x;
        float PressureLeft = LoadPressureMirror(ivec2(gl_GlobalInvocationID.xy) - ivec2(2, 0), ivec2(TextureSize));
        float PressureRight = LoadPressureMirror(ivec2(gl_GlobalInvocationID.xy) + ivec2(2, 0), ivec2(TextureSize));
        float PressureUp = LoadPressureMirror(ivec2(gl_GlobalInvocationID.xy) + ivec2(0, 2), ivec2(TextureSize));
        float PressureDown = LoadPressureMirror(ivec2(gl_GlobalInvocationID.xy) - ivec2(0, 2), ivec2(TextureSize));
        
        float Divergence = imageLoad(DivergenceImage, ivec2(gl_GlobalInvocationID.xy)).x;
        float NewPressureCenter = (Divergence + PressureRight + PressureLeft + PressureUp + PressureDown) * 0.25f;

        imageStore(OutPressureImage, ivec2(gl_GlobalInvocationID.xy), vec4(NewPressureCenter, 0, 0, 0));
    }
}

#endif

//
// NOTE: Pressure Clamp Iteration (Jacobis Method)
//

#if PRESSURE_CLAMP_ITERATION

layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;
void main()
{
    vec2 TextureSize = vec2(FluidSimInputs.Dim, FluidSimInputs.Dim);
    if (gl_GlobalInvocationID.x < TextureSize.x && gl_GlobalInvocationID.y < TextureSize.y)
    {
        vec2 Uv = GetUv();
        float PressureCenter = imageLoad(InPressureImage, ivec2(gl_GlobalInvocationID.xy)).x;
        float PressureLeft = LoadPressureClamp(ivec2(gl_GlobalInvocationID.xy), -ivec2(2, 0), TextureSize).x;
        float PressureRight = LoadPressureClamp(ivec2(gl_GlobalInvocationID.xy), ivec2(2, 0), TextureSize).x;
        float PressureUp = LoadPressureClamp(ivec2(gl_GlobalInvocationID.xy), ivec2(0, 2), TextureSize).x;
        float PressureDown = LoadPressureClamp(ivec2(gl_GlobalInvocationID.xy), -ivec2(0, 2), TextureSize).x;
        
        float Divergence = imageLoad(DivergenceImage, ivec2(gl_GlobalInvocationID.xy)).x;
        float NewPressureCenter = (Divergence + PressureRight + PressureLeft + PressureUp + PressureDown) * 0.25f;

        imageStore(OutPressureImage, ivec2(gl_GlobalInvocationID.xy), vec4(NewPressureCenter, 0, 0, 0));
    }
}

#endif
