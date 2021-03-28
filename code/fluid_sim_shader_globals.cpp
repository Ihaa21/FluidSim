
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

layout(set = 2, binding = 0, r32f) uniform image2D InPressureImage;
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

float LoadPressureMirror(ivec2 SamplePos, ivec2 TextureSize)
{
    float Result = 0.0f;

#if 0
    ivec2 MirrorSamplePos = SamplePos;
    if (MirrorSamplePos.x < 0)
    {
        MirrorSamplePos.x += int(TextureSize.x);
    }
    else if (MirrorSamplePos.x > TextureSize.x)
    {
        MirrorSamplePos.x -= int(TextureSize.x);
    }
    
    if (MirrorSamplePos.y < 0)
    {
        MirrorSamplePos.y += int(TextureSize.y);
    }
    else if (MirrorSamplePos.y > TextureSize.y)
    {
        MirrorSamplePos.y -= int(TextureSize.y);
    }
#endif
    
    vec2 Uv = fract((vec2(SamplePos) + vec2(0.5f)) / vec2(TextureSize));
    ivec2 MirrorSamplePos = ivec2(Uv * TextureSize);
    
    Result = imageLoad(InPressureImage, MirrorSamplePos).x;

    return Result;
}

float LoadPressureClosedBorder(ivec2 SamplePos, ivec2 SampleOffset, vec2 TextureSize)
{
    float Result = 0.0f;

    // NOTE: If we are out of bounds, use the pressure in the center for our result
    if ((SamplePos.x + SampleOffset.x) < 0 || (SamplePos.x + SampleOffset.x) >= TextureSize.x ||
        (SamplePos.y + SampleOffset.y) < 0 || (SamplePos.y + SampleOffset.y) >= TextureSize.y)
    {
        Result = imageLoad(InPressureImage, SamplePos).x;
    }
    else
    {
        Result = imageLoad(InPressureImage, SamplePos + SampleOffset).x;
    }
    
    return Result;
}

float LoadPressureClamp(ivec2 SamplePos, ivec2 SampleOffset, vec2 TextureSize)
{
    float Result = imageLoad(InPressureImage, SamplePos + SampleOffset).x;
    return Result;
}
