#version 450

#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_nonuniform_qualifier: enable

#include "fluid_sim_shader_globals.cpp"

//
// NOTE: Smoke Splat
//

#if SMOKE_SPLAT

layout(set = 1, binding = 0, r32f) uniform image2D TemperatureImage;
layout(set = 1, binding = 1, r32f) uniform image2D ColorImage;
layout(set = 1, binding = 2, r32f) uniform image2D VelocityImage;

layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;
void main()
{
    vec2 TextureSize = vec2(FluidSimInputs.Dim, FluidSimInputs.Dim);
    if (gl_GlobalInvocationID.x < TextureSize.x && gl_GlobalInvocationID.y < TextureSize.y)
    {
        vec2 Uv = GetUv();
        float SplatValue = 0.0f;
        if (Uv.x >= (FluidSimInputs.SplatCenter.x - FluidSimInputs.SplatRadius) &&
            Uv.x <= (FluidSimInputs.SplatCenter.x + FluidSimInputs.SplatRadius) && 
            Uv.y >= (FluidSimInputs.SplatCenter.y - FluidSimInputs.SplatRadius) &&
            Uv.y <= (FluidSimInputs.SplatCenter.y + FluidSimInputs.SplatRadius))
        {
#if 0
            // NOTE: Exponential splat
            float Dx = FluidSimInputs.SplatCenter.x - Uv.x;
            float Dy = FluidSimInputs.SplatCenter.y - Uv.y;
            SplatValue = exp(-(Dx * Dx + Dy * Dy) / FluidSimInputs.SplatRadius);
#else
            SplatValue = 1.0f;
#endif

            // NOTE: Update temperature
            float CurrTemperature = imageLoad(TemperatureImage, ivec2(gl_GlobalInvocationID.xy)).x;
            float NewTemperature = CurrTemperature + 0.01f*SplatValue;
            imageStore(TemperatureImage, ivec2(gl_GlobalInvocationID.xy), vec4(NewTemperature, 0, 0, 0));

            // NOTE: Update color
            float CurrColor = imageLoad(ColorImage, ivec2(gl_GlobalInvocationID.xy)).x;
            float NewColor = CurrColor + SplatValue;
            imageStore(ColorImage, ivec2(gl_GlobalInvocationID.xy), vec4(NewColor, 0, 0, 0));

            // NOTE: Update velocity
            vec2 NewVelocity = 0.01f*vec2(0, SplatValue);
            imageStore(VelocityImage, ivec2(gl_GlobalInvocationID.xy), vec4(NewVelocity, 0, 0));
        }
    }
}

#endif

//
// NOTE: Smoke Advection
//

#if SMOKE_ADVECTION

layout(set = 1, binding = 0) uniform sampler2D InVelocityImage;
layout(set = 1, binding = 1, rg32f) uniform image2D OutVelocityImage;

layout(set = 1, binding = 2) uniform sampler2D InTemperatureImage;
layout(set = 1, binding = 3, rg32f) uniform image2D OutTemperatureImage;

layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;
void main()
{
    vec2 TextureSize = vec2(FluidSimInputs.Dim, FluidSimInputs.Dim);
    if (gl_GlobalInvocationID.x < TextureSize.x && gl_GlobalInvocationID.y < TextureSize.y)
    {        
        vec2 SamplePos = AdvectionFindPos(InVelocityImage, gl_GlobalInvocationID.xy, TextureSize);

        // NOTE: Advect Temperature
        vec4 NewTemperature = texture(InTemperatureImage, SamplePos);

        // TODO: Handle boundaries properly
        // NOTE: We currently make temperature always non zero, we probably want to handle image edges with a sampler vs in shader?
        if (NewTemperature.x <= 0.0f)
        {
            // NOTE: This should only happen on borders
            NewTemperature.x = FluidSimInputs.RoomTemperature;
        }
        
        imageStore(OutTemperatureImage, ivec2(gl_GlobalInvocationID.xy), NewTemperature);

        // NOTE: Get buoyancy force
        float Multiplier = FluidSimInputs.MolarMass * FluidSimInputs.Gravity / FluidSimInputs.R;
        // TODO: Make pressure a constant passed in
        float Buoyancy = 0.01f * Multiplier * ((1.0f / FluidSimInputs.RoomTemperature) - (1.0f / NewTemperature.x));
        
        // NOTE: Advect the velocity field
        vec2 NewVelocity = texture(InVelocityImage, SamplePos).xy + Buoyancy * vec2(0, 1);
        imageStore(OutVelocityImage, ivec2(gl_GlobalInvocationID.xy), vec4(NewVelocity, 0, 0));
    }
}

#endif

//
// NOTE: Smoke Pressure Application
//

#if SMOKE_PRESSURE_APPLY

layout(set = 1, binding = 0, rg32f) uniform image2D VelocityImage;

layout(set = 1, binding = 1) uniform sampler2D InColorImage;
layout(set = 1, binding = 2, r32f) uniform image2D OutColorImage;

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

        vec2 SamplePos = AdvectionFindPos(CorrectedVel, gl_GlobalInvocationID.xy, TextureSize);

        // NOTE: Advect the color
        {
            vec4 NewColor = texture(InColorImage, SamplePos);
            imageStore(OutColorImage, ivec2(gl_GlobalInvocationID.xy), NewColor);
        }
    }
}

#endif
