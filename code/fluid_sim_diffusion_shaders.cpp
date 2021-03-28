#version 450

#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_nonuniform_qualifier: enable

#include "fluid_sim_shader_globals.cpp"

//
// NOTE: Diffusion Fluid Init
//

#if DIFFUSION_INIT

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
#if 0
            vec4 OutColor = vec4(step(1.0, mod(floor((Uv.x + 1.0) / 0.2) + floor((Uv.y + 1.0) / 0.2), 2.0)),
                                 step(1.0, mod(floor((Uv.x + 1.0) / 0.3) + floor((Uv.y + 1.0) / 0.3), 2.0)),
                                 step(1.0, mod(floor((Uv.x + 1.0) / 0.4) + floor((Uv.y + 1.0) / 0.4), 2.0)),
                                 1);
#endif
            vec4 OutColor = vec4(step(1.0, mod(floor((Uv.x + 1.0) / 0.2) + floor((Uv.y + 1.0) / 0.2), 2.0)),
                                 step(1.0, mod(floor((Uv.x + 1.0) / 0.2) + floor((Uv.y + 1.0) / 0.2), 2.0)),
                                 step(1.0, mod(floor((Uv.x + 1.0) / 0.2) + floor((Uv.y + 1.0) / 0.2), 2.0)),
                                 1);
            
            imageStore(OutColorImage, ivec2(gl_GlobalInvocationID.xy), OutColor);
        }
    }    
}

#endif

//
// NOTE: Diffusion Advection
//

#if DIFFUSION_ADVECTION

layout(set = 1, binding = 0) uniform sampler2D InVelocityImage;
layout(set = 1, binding = 1, rg32f) uniform image2D OutVelocityImage;

layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;
void main()
{
    vec2 TextureSize = vec2(FluidSimInputs.Dim, FluidSimInputs.Dim);
    if (gl_GlobalInvocationID.x < TextureSize.x && gl_GlobalInvocationID.y < TextureSize.y)
    {        
        // NOTE: Advect the velocity field
        vec2 SamplePos = AdvectionFindPos(InVelocityImage, gl_GlobalInvocationID.xy, TextureSize);
        vec2 NewVelocity = texture(InVelocityImage, SamplePos).xy;
        imageStore(OutVelocityImage, ivec2(gl_GlobalInvocationID.xy), vec4(NewVelocity, 0, 0));
    }
}

#endif

//
// NOTE: Diffusion Pressure Application
//

#if DIFFUSION_PRESSURE_APPLY

layout(set = 1, binding = 0, rg32f) uniform image2D VelocityImage;

layout(set = 1, binding = 1) uniform sampler2D InColorImage;
layout(set = 1, binding = 2, rgba32f) uniform image2D OutColorImage;

layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;
void main()
{
    vec2 TextureSize = vec2(FluidSimInputs.Dim, FluidSimInputs.Dim);
    if (gl_GlobalInvocationID.x < TextureSize.x && gl_GlobalInvocationID.y < TextureSize.y)
    {
        vec2 Uv = GetUv();
        
        // NOTE: Apply pressure
        vec2 CorrectedVel;
        {
            float PressureLeft = LoadPressureMirror(ivec2(gl_GlobalInvocationID.xy) - ivec2(1, 0), ivec2(TextureSize));
            float PressureRight = LoadPressureMirror(ivec2(gl_GlobalInvocationID.xy) + ivec2(1, 0), ivec2(TextureSize));
            float PressureUp = LoadPressureMirror(ivec2(gl_GlobalInvocationID.xy) + ivec2(0, 1), ivec2(TextureSize));
            float PressureDown = LoadPressureMirror(ivec2(gl_GlobalInvocationID.xy) - ivec2(0, 1), ivec2(TextureSize));

            float Multiplier = (FluidSimInputs.FrameTime / (2*FluidSimInputs.Density*FluidSimInputs.Epsilon));
            vec2 AdvectedVel = imageLoad(VelocityImage, ivec2(gl_GlobalInvocationID.xy)).xy;
            CorrectedVel.x = AdvectedVel.x - Multiplier * (PressureRight - PressureLeft);
            CorrectedVel.y = AdvectedVel.y - Multiplier * (PressureUp - PressureDown);
            
            imageStore(VelocityImage, ivec2(gl_GlobalInvocationID.xy), vec4(CorrectedVel, 0, 0));
        }

        // NOTE: Advect the color
        {
            vec2 SamplePos = AdvectionFindPos(CorrectedVel, gl_GlobalInvocationID.xy, TextureSize);
            vec4 NewColor = texture(InColorImage, SamplePos);
            imageStore(OutColorImage, ivec2(gl_GlobalInvocationID.xy), NewColor);
        }
    }
}

#endif
