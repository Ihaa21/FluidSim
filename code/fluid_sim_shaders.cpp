#version 450

/*
  NOTE: http://jamie-wong.com/2016/08/05/webgl-fluid-simulation/
        https://codepen.io/davvidbaker/pen/ENbqdQ


      Ideas:

        - Can we convert some of these shaders to use gather ops instead? Currently we scoop +1, -1 in each axis. Gather would be 0 and 1
          so potentially less precise but might be much faster?
*/

#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_nonuniform_qualifier: enable

#include "descriptor_layouts.cpp"
#include "blinn_phong_lighting.cpp"

//
// NOTE: Global Descriptor
//

layout(set = 0, binding = 0) uniform fluid_sim_inputs
{
    float FrameTime;
    float Density;
    float Epsilon;
    uint Dim;
} FluidSimInputs;

layout(set = 0, binding = 1, r32f) uniform image2D DivergenceImage;

//
// NOTE: Pressure descriptor
//

layout(set = 2, binding = 0) uniform sampler2D InPressureImage;
layout(set = 2, binding = 1, r32f) uniform image2D OutPressureImage;

// TODO: Make global everywhere
#define Pi32 (3.141592653589f)

vec2 GetUv()
{
    vec2 Result = (gl_GlobalInvocationID.xy + vec2(0.5)) / vec2(FluidSimInputs.Dim, FluidSimInputs.Dim);
    return Result;
}

//
// NOTE: Fluid Init
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
            vec4 OutColor = vec4(step(1.0, mod(floor((Uv.x + 1.0) / 0.2) + floor((Uv.y + 1.0) / 0.2), 2.0)),
                                 step(1.0, mod(floor((Uv.x + 1.0) / 0.3) + floor((Uv.y + 1.0) / 0.3), 2.0)),
                                 step(1.0, mod(floor((Uv.x + 1.0) / 0.4) + floor((Uv.y + 1.0) / 0.4), 2.0)),
                                 1);
            
            imageStore(OutColorImage, ivec2(gl_GlobalInvocationID.xy), OutColor);
        }
    }    
}

#endif

//
// NOTE: Advection (Velocity)
//

vec2 AdvectionFindPos(vec2 Velocity, vec2 GlobalThreadId, vec2 TextureSize)
{
    // NOTE: Operate in UV space (centers are 0.5, 0.5)
    vec2 StartPosUv = (GlobalThreadId + vec2(0.5f)) / TextureSize;
    vec2 Result = StartPosUv - Velocity * FluidSimInputs.FrameTime;

    return Result;
}

vec2 AdvectionFindPos(sampler2D VelocityImage, vec2 GlobalThreadId, vec2 TextureSize)
{
    vec2 Velocity = texelFetch(VelocityImage, ivec2(GlobalThreadId.xy), 0).xy;
    vec2 Result = AdvectionFindPos(Velocity, GlobalThreadId, TextureSize);

    return Result;
}

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
        vec2 SamplePos = AdvectionFindPos(InVelocityImage, gl_GlobalInvocationID.xy, TextureSize);
        vec2 NewVelocity = texture(InVelocityImage, SamplePos).xy;
        imageStore(OutVelocityImage, ivec2(gl_GlobalInvocationID.xy), vec4(NewVelocity, 0, 0));
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

        float VelRight = texture(InVelocityImage, Uv + vec2(FluidSimInputs.Epsilon, 0)).x;
        float VelLeft = texture(InVelocityImage, Uv - vec2(FluidSimInputs.Epsilon, 0)).x;
        float VelUp = texture(InVelocityImage, Uv + vec2(0, FluidSimInputs.Epsilon)).y;
        float VelDown = texture(InVelocityImage, Uv - vec2(0, FluidSimInputs.Epsilon)).y;

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

layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;
void main()
{
    vec2 TextureSize = vec2(FluidSimInputs.Dim, FluidSimInputs.Dim);
    if (gl_GlobalInvocationID.x < TextureSize.x && gl_GlobalInvocationID.y < TextureSize.y)
    {
        vec2 Uv = GetUv();
        float PressureCenter = texture(InPressureImage, Uv).x;
        float PressureLeft = textureOffset(InPressureImage, Uv, -ivec2(2, 0)).x;
        float PressureRight = textureOffset(InPressureImage, Uv, ivec2(2, 0)).x;
        float PressureUp = textureOffset(InPressureImage, Uv, ivec2(0, 2)).x;
        float PressureDown = textureOffset(InPressureImage, Uv, -ivec2(0, 2)).x;
        
        float Divergence = imageLoad(DivergenceImage, ivec2(gl_GlobalInvocationID.xy)).x;
        float NewPressureCenter = (Divergence + PressureRight + PressureLeft + PressureUp + PressureDown) * 0.25f;

        imageStore(OutPressureImage, ivec2(gl_GlobalInvocationID.xy), vec4(NewPressureCenter, 0, 0, 0));
    }
}

#endif

//
// NOTE: Pressure Application
//

#if PRESSURE_APPLY

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
            float PressureLeft = textureOffset(InPressureImage, Uv, -ivec2(1, 0)).x;
            float PressureRight = textureOffset(InPressureImage, Uv, ivec2(1, 0)).x;
            float PressureUp = textureOffset(InPressureImage, Uv, ivec2(0, 1)).x;
            float PressureDown = textureOffset(InPressureImage, Uv, -ivec2(0, 1)).x;

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
