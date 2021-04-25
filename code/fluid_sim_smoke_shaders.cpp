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

    float Gravity;
    float RoomTemperature;
    float MolarMass;
    float R;
    
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
// NOTE: Splat
//

#if SPLAT

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
#if 1
            // NOTE: Exponential splat
            float Dx = FluidSimInputs.SplatCenter.x - Uv.x;
            float Dy = FluidSimInputs.SplatCenter.y - Uv.y;
            SplatValue = 0.1f*exp(-(Dx * Dx + Dy * Dy) / FluidSimInputs.SplatRadius);
#else
            SplatValue = 1.0f;
#endif

            // NOTE: Update temperature
            float CurrTemperature = imageLoad(TemperatureImage, ivec2(gl_GlobalInvocationID.xy)).x;
            float NewTemperature = CurrTemperature + 0.01f*SplatValue;
            imageStore(TemperatureImage, ivec2(gl_GlobalInvocationID.xy), vec4(NewTemperature, 0, 0, 0));

            // NOTE: Update color
            float CurrColor = imageLoad(ColorImage, ivec2(gl_GlobalInvocationID.xy)).x;
            float NewColor = CurrColor + 0.1f*SplatValue;
            imageStore(ColorImage, ivec2(gl_GlobalInvocationID.xy), vec4(NewColor, NewColor, NewColor, 1));

            // NOTE: Update velocity
            vec2 NewVelocity = 0.01f*vec2(0, SplatValue);
            imageStore(VelocityImage, ivec2(gl_GlobalInvocationID.xy), vec4(NewVelocity, 0, 0));
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

layout(set = 1, binding = 2) uniform sampler2D InTemperatureImage;

layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;
void main()
{
    vec2 TextureSize = vec2(FluidSimInputs.Dim, FluidSimInputs.Dim);
    if (gl_GlobalInvocationID.x < TextureSize.x && gl_GlobalInvocationID.y < TextureSize.y)
    {
        ivec2 PixelCoord = ivec2(gl_GlobalInvocationID.xy);
        vec2 CurrVel = texelFetch(InVelocityImage, PixelCoord, 0).xy;
        vec2 SamplePos = AdvectionFindPos(CurrVel, gl_GlobalInvocationID.xy, TextureSize, FluidSimInputs.FrameTime);

        // NOTE: Get buoyancy force
        float Multiplier = FluidSimInputs.MolarMass * FluidSimInputs.Gravity / FluidSimInputs.R;
        float Temperature = texture(InTemperatureImage, GetUv()).x;
        // TODO: Make pressure a constant passed in
        float Buoyancy = 0.01f * Multiplier * ((1.0f / FluidSimInputs.RoomTemperature) - (1.0f / Temperature));
        
        // NOTE: Advect the velocity field
        vec2 NewVelocity = texture(InVelocityImage, SamplePos).xy + Buoyancy * vec2(0, 1);
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
        ivec2 LeftCoord = ClampPixelCoord(CenterCoord, -ivec2(2, 0), ivec2(TextureSize));
        ivec2 RightCoord = ClampPixelCoord(CenterCoord, ivec2(2, 0), ivec2(TextureSize));
        ivec2 UpCoord = ClampPixelCoord(CenterCoord, ivec2(0, 2), ivec2(TextureSize));
        ivec2 DownCoord = ClampPixelCoord(CenterCoord, -ivec2(0, 2), ivec2(TextureSize));
        
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
// NOTE: Pressure Application
//

#if PRESSURE_APPLY

layout(set = 1, binding = 0, rg32f) uniform image2D VelocityImage;

layout(set = 1, binding = 1) uniform sampler2D InColorImage;
layout(set = 1, binding = 2, r32f) uniform image2D OutColorImage;

layout(set = 1, binding = 3) uniform sampler2D InTemperatureImage;
layout(set = 1, binding = 4, r32f) uniform image2D OutTemperatureImage;

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
            ivec2 LeftCoord = ClampPixelCoord(CenterCoord, -ivec2(1, 0), ivec2(TextureSize));
            ivec2 RightCoord = ClampPixelCoord(CenterCoord, ivec2(1, 0), ivec2(TextureSize));
            ivec2 UpCoord = ClampPixelCoord(CenterCoord, ivec2(0, 1), ivec2(TextureSize));
            ivec2 DownCoord = ClampPixelCoord(CenterCoord, -ivec2(0, 1), ivec2(TextureSize));

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

        vec2 SamplePos = AdvectionFindPos(CorrectedVel, gl_GlobalInvocationID.xy, TextureSize, FluidSimInputs.FrameTime);

        // NOTE: Advect the temperature
        {
            vec4 NewTemperature = texture(InTemperatureImage, SamplePos);
            // TODO: FIX THIS HACK
            NewTemperature.x = max(NewTemperature.x, FluidSimInputs.RoomTemperature);
            imageStore(OutTemperatureImage, CenterCoord, NewTemperature);
        }
        
        // NOTE: Advect the color
        {
            vec4 NewColor = texture(InColorImage, SamplePos);
            imageStore(OutColorImage, CenterCoord, NewColor);
        }
    }
}

#endif
