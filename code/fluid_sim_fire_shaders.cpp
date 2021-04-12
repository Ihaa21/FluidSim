#version 450

#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_nonuniform_qualifier: enable

#include "fluid_sim_shader_globals.cpp"

//
// NOTE: Fire Splat
//

#if FIRE_SPLAT

layout(set = 1, binding = 0, r32f) uniform image2D TemperatureImage;
layout(set = 1, binding = 1, r32f) uniform image2D VelocityImage;
layout(set = 1, binding = 2, r32f) uniform image2D TimerImage;

float RandomFloat(vec4 Value)
{
    vec4 SmallValue = sin(Value);
    float Result = dot(SmallValue, vec4(12.9898, 78.233, 37.719, 09.151));
    Result = fract(sin(Result) * 143758.5453);
    return Result;
}

layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;
void main()
{
    vec2 TextureSize = vec2(FluidSimInputs.Dim, FluidSimInputs.Dim);
    if (gl_GlobalInvocationID.x < TextureSize.x && gl_GlobalInvocationID.y < TextureSize.y)
    {
        vec2 Uv = GetUv();
        float SplatValue = 0.0f;

#if 0
        // NOTE: Box Radius
        bool InRadius = (Uv.x >= (FluidSimInputs.MousePos.x - FluidSimInputs.SplatRadius) &&
                         Uv.x <= (FluidSimInputs.MousePos.x + FluidSimInputs.SplatRadius) && 
                         Uv.y >= (FluidSimInputs.MousePos.y - FluidSimInputs.SplatRadius) &&
                         Uv.y <= (FluidSimInputs.MousePos.y + FluidSimInputs.SplatRadius));
#else
        // NOTE: Circle Radius
        bool InRadius = length(Uv - FluidSimInputs.MousePos) <=  FluidSimInputs.SplatRadius;
#endif
        
        if (InRadius)
        {
#if 1
            // NOTE: Exponential splat
            float Dx = FluidSimInputs.MousePos.x - Uv.x;
            float Dy = FluidSimInputs.MousePos.y - Uv.y;
            SplatValue = 0.71f*exp(-(Dx * Dx + Dy * Dy) / FluidSimInputs.SplatRadius);
#else
            SplatValue = 0.01f;
#endif

            float Entropy = RandomFloat(vec4(gl_GlobalInvocationID.x * gl_GlobalInvocationID.y, 0, 0, 0)).x;
            
            // NOTE: Update temperature
            float CurrTemperature = imageLoad(TemperatureImage, ivec2(gl_GlobalInvocationID.xy)).x;
            float NewTemperature = CurrTemperature;// + 0.01f*(Entropy);
            imageStore(TemperatureImage, ivec2(gl_GlobalInvocationID.xy), vec4(NewTemperature, 0, 0, 0));

            // NOTE: Update velocity
            vec2 NewVelocity = 0.5f*vec2(0, 1.0f) + -25.0f * FluidSimInputs.DeltaMousePos; // + abs(Entropy));
            imageStore(VelocityImage, ivec2(gl_GlobalInvocationID.xy), vec4(NewVelocity, 0, 0));

            // NOTE: Update timer coordinate
            float NewTimer = 0.95f * SplatValue;
            imageStore(TimerImage, ivec2(gl_GlobalInvocationID.xy), vec4(NewTimer, 0, 0, 0));
        }
    }
}

#endif

//
// NOTE: Fire Combustion
//

#if FIRE_BURN_FUEL

layout(set = 1, binding = 0) uniform sampler2D InTimerImage;

layout(set = 1, binding = 1, r32f) uniform image2D TemperatureImage;

layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;
void main()
{
#if 0
    vec2 TextureSize = vec2(FluidSimInputs.Dim, FluidSimInputs.Dim);
    if (gl_GlobalInvocationID.x < TextureSize.x && gl_GlobalInvocationID.y < TextureSize.y)
    {
        vec2 Uv = GetUv();
        // TODO: Can we remove all of this and just use splats to modify the temperature? 
        float Temperature = imageLoad(TemperatureImage, ivec2(gl_GlobalInvocationID.xy)).x;
        float Fuel = texture(InTimerImage, Uv).x;

        // NOTE: Cool temperature
        Temperature = max(0.0f, Temperature - FluidSimInputs.FrameTime * FluidSimInputs.FireCooling * pow(Temperature / FluidSimInputs.FireBurnTemp, 4.0f));

        // NOTE: Add more heat based on fuel
        Temperature = max(Temperature, Fuel*FluidSimInputs.FireBurnTemp);
        
        //imageStore(TemperatureImage, ivec2(gl_GlobalInvocationID.xy), vec4(Temperature, 0, 0, 0));
    }
#endif
}

#endif

//
// NOTE: Fire Advection
//

#if FIRE_ADVECTION

layout(set = 1, binding = 0) uniform sampler2D InVelocityImage;
layout(set = 1, binding = 1, rg32f) uniform image2D OutVelocityImage;

layout(set = 1, binding = 2) uniform sampler2D InTemperatureImage;

layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;
void main()
{
    vec2 TextureSize = vec2(FluidSimInputs.Dim, FluidSimInputs.Dim);
    if (gl_GlobalInvocationID.x < TextureSize.x && gl_GlobalInvocationID.y < TextureSize.y)
    {        
        vec2 SamplePos = AdvectionFindPos(InVelocityImage, gl_GlobalInvocationID.xy, TextureSize);

        // NOTE: Get buoyancy force
        float Temperature = texture(InTemperatureImage, GetUv()).x;
#if 1
        Temperature = max(FluidSimInputs.RoomTemperature, Temperature);
        float Multiplier = FluidSimInputs.MolarMass * FluidSimInputs.Gravity / FluidSimInputs.R;
        float PressureConstant = 1.0f;
        float Buoyancy = Multiplier * ((1.0f / FluidSimInputs.RoomTemperature) - (1.0f / Temperature));
#else
        float Buoyancy = 0.2f * FluidSimInputs.FrameTime * Temperature;
#endif
        
        // NOTE: Advect the velocity field
        vec2 NewVelocity = texture(InVelocityImage, SamplePos).xy + Buoyancy * vec2(0, 1);
        imageStore(OutVelocityImage, ivec2(gl_GlobalInvocationID.xy), vec4(NewVelocity, 0, 0));
    }
}

#endif

//
// NOTE: Fire Pressure Application
//

#if FIRE_PRESSURE_APPLY

layout(set = 1, binding = 0, rg32f) uniform image2D VelocityImage;

layout(set = 1, binding = 1) uniform sampler2D InTemperatureImage;
layout(set = 1, binding = 2, r32f) uniform image2D OutTemperatureImage;

layout(set = 1, binding = 3) uniform sampler2D InTimerImage;
layout(set = 1, binding = 4, r32f) uniform image2D OutTimerImage;

layout(set = 1, binding = 5, rgba32f) uniform image2D OutColorImage;

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
            float PressureLeft = LoadPressureClamp(ivec2(gl_GlobalInvocationID.xy), -ivec2(1, 0), TextureSize).x;
            float PressureRight = LoadPressureClamp(ivec2(gl_GlobalInvocationID.xy), ivec2(1, 0), TextureSize).x;
            float PressureUp = LoadPressureClamp(ivec2(gl_GlobalInvocationID.xy), ivec2(0, 1), TextureSize).x;
            float PressureDown = LoadPressureClamp(ivec2(gl_GlobalInvocationID.xy), -ivec2(0, 1), TextureSize).x;

            float Multiplier = (FluidSimInputs.FrameTime / (2*FluidSimInputs.Density*FluidSimInputs.Epsilon));
            vec2 AdvectedVel = imageLoad(VelocityImage, ivec2(gl_GlobalInvocationID.xy)).xy;
            CorrectedVel.x = AdvectedVel.x - Multiplier * (PressureRight - PressureLeft);
            CorrectedVel.y = AdvectedVel.y - Multiplier * (PressureUp - PressureDown);
            
            imageStore(VelocityImage, ivec2(gl_GlobalInvocationID.xy), vec4(CorrectedVel, 0, 0));
        }

        vec2 SamplePos = AdvectionFindPos(CorrectedVel, gl_GlobalInvocationID.xy, TextureSize);

        // NOTE: Advect the timer coordinate
        float NewTimer;
        {
            NewTimer = texture(InTimerImage, SamplePos).x;
            NewTimer.x = max(0.0f, NewTimer.x - FluidSimInputs.FrameTime);
            //NewTimer.x = 0.92f * NewTimer.x;
            imageStore(OutTimerImage, ivec2(gl_GlobalInvocationID.xy), vec4(NewTimer, 0, 0, 0));
        }

        // NOTE: Advect the temperature
        {
            vec4 NewTemperature = texture(InTemperatureImage, SamplePos);
            // TODO: REMOVE THE HACK
            NewTemperature.x = max(NewTemperature.x, FluidSimInputs.RoomTemperature);
            imageStore(OutTemperatureImage, ivec2(gl_GlobalInvocationID.xy), NewTemperature);
        }

        // NOTE: Set the color
        {
            vec4 OutColor = vec4(mix(vec3(1.0, 0.58, 0.0), vec3(1.0, 0.7, 0.4), (((NewTimer - 0.55) * 10.0 + 0.5) * 2.0)), NewTimer);
            OutColor.rgb = vec3(1.0, 0.2, 0.0);
            if (OutColor.a > 0.37) OutColor.rgb = vec3(1.0, 0.8, 0.0);
            if (OutColor.a > 0.65) OutColor.rgb = vec3(1.0, 1.0, 1.0);
            OutColor.a = float(OutColor.a > 0.1);

            if (OutColor.a == 0)
            {
                OutColor.rgb = vec3(0);
            }

            // TODO: We are artificially flipping here
            ivec2 OutPos = ivec2(gl_GlobalInvocationID.x, TextureSize.y - gl_GlobalInvocationID.y);
            imageStore(OutColorImage, OutPos, OutColor);
        }
    }
}

#endif
