
/*
  NOTE: http://jamie-wong.com/2016/08/05/webgl-fluid-simulation/
        https://codepen.io/davvidbaker/pen/ENbqdQ


      Ideas:

        - Can we convert some of these shaders to use gather ops instead? Currently we scoop +1, -1 in each axis. Gather would be 0 and 1
          so potentially less precise but might be much faster?
*/

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

    // NOTE: Smoke globals
    float Gravity;
    float RoomTemperature;
    float MolarMass;
    float R; // NOTE: Universal gas constant

    // NOTE: Splat data
    vec2 SplatCenter;
    float SplatRadius;
    
} FluidSimInputs;

layout(set = 0, binding = 1, r32f) uniform image2D DivergenceImage;

//
// NOTE: Pressure descriptor
//

layout(set = 2, binding = 0) uniform sampler2D InPressureImage;
layout(set = 2, binding = 1, r32f) uniform image2D OutPressureImage;

// TODO: Make global everywhere
#define Pi32 (3.141592653589f)

vec2 GetUv(vec2 InvocationId)
{
    vec2 Result = (InvocationId + vec2(0.5)) / vec2(FluidSimInputs.Dim, FluidSimInputs.Dim);
    return Result;
}

vec2 GetUv()
{
    return GetUv(gl_GlobalInvocationID.xy);
}

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
