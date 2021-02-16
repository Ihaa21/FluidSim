#version 450

/*
  NOTE: http://jamie-wong.com/2016/08/05/webgl-fluid-simulation/

  NOTE: du(t)/dt = -u(t) * div * u(t) - 1/P * (div * p(t)) + v * (div^2 * u(t)) + external_force

        (div * u(t)) = 0

      Our fluid has no viscosity so set v = 0. Then,

        du(t)/dt = -u(t) * div * u(t) - 1/P * (div * p(t)) + external_force

      Its a bit weird with notation but div is a variable that when multiplied by a function, gives us a new function, so its
      a different type than scalar/vector. div = (d/dx, d/dy, d/dz)

      We now rewrite this into matrix/vector form:

        [du_x(t)/dt; du_y(t)/dt] = -[u_x(t); u_y(t)] * [d/dx, d/dy] * [u_x(t); u_y(t)] - 1/P * [dp_x(t)/dx, dp_y(t)/dy]
                                 = -[du_x(t)/dx, du_x(t)/dy; du_y(t)/dx, du_y(t)/dy] * [u_x(t); u_y(t)] - 1/P * [dp_x(t)/dx, dp_y(t)/dy]

      We work to solve for pressure since all other variables can be found. Pressure is what will make our fluid divergence free
      and incompressible.

        du_x(x, y, t)/dt ~ (u_x(x, y, t + dt) - u_x(x, y, t)) / dt

      Above lets us approximate all the derivative operations of u (our velocity field). This is the formula we get for x component
      using the above approximation:

        du_x(t)/dt = -(du_x(t)/dx * u_x(t) + du_x(t)/dy * u_y(t)) - 1/P * dp_x(t)/dx
        (u_x(x, y, t + dt) - u_x(x, y, t)) / dt = -((u_x(x + epsilon, y, t) - u_x(x, y, t)) / epsilon) * u_x(x, y, t)
                                                  -((u_x(x, y + epsilon, t) - u_x(x, y, t)) / epsilon) * u_y(x, y, t)
                                                  -1/P * ((p_x(x + epsilon, y, t) - p_x(x, y, t)) / epsilon)
                                                          
        u_x(x, y, t + dt)) = u_x(x, y, t)
                            -dt*((u_x(x + epsilon, y, t) - u_x(x, y, t)) / epsilon) * u_x(x, y, t)
                            -dt*((u_x(x, y + epsilon, t) - u_x(x, y, t)) / epsilon) * u_y(x, y, t)
                            -dt/P * ((p_x(x + epsilon, y, t) - p_x(x, y, t)) / epsilon)

      Advection is the following:

        u(x, y, t + dt) = u(x - u_x(x, y, t)*dt, y - u_y(x, y, t)*dt, t)

      The first 3 terms above can be rewritten as the following:

        u_x(x, y, t) - u_x(x, y, t)*du_x(x, y, t)/dx * dt - u_y(x, y, t) * du_x(x, y, t)/dy * dt
        = u_x(x, y, t) - (u_x(x, y, t)*du_x(x, y, t)/dx + u_y(x, y, t) * du_x(x, y, t)/dy) * dt

      The post makes the claim that the above is the same or similar to advection of velocity. They are similar, I guess you have
      to say that the derivative terms are approximated by bilinear interpolation since thats the nudge in x/y. The only part that
      is confusing to me is that for the y case, we have u_y(t) * du_x(t)/dy. The u_y gets added for teh x term which is weird,
      I think it wouldn't in bilinear since all coordinate axis are kept separate. The post then replaces this whole term with
      u_a_x which is velocity after being advected.

        u_x(x, y, t + dt) = u_a_x(x, y, t) - 1/P * dt * (p(x + epsilon, y, t) - p(x - epsilon, y, t)) / 2*epsilon

      IMPORTANT: I changed the notation to match the post, instead of keeping one point fixed and varying the other by epsilon,
      we are varying both by epsilon in opposite directions.

      We now solve for pressure using the fact that div * u = 0:

        div * u = 0
        du_x/dx + du_y/dy = 0
        (u_x(x + epsilon, y, t + dt) - u_x(x - epsilon, y, t + dt)) / 2epsilon +
        (u_y(x, y + epsilon, t + dt) - u_y(x, y - epsilon, t + dt)) / 2epsilon = 0

      Sub our equation for u_x/u_y in temrs of pressure:

        0 = (u_x(x + epsilon, y, t + dt) - u_x(x - epsilon, y, t + dt)) / 2epsilon + (u_y(x, y + epsilon, t + dt) - u_y(x, y - epsilon, t + dt)) / 2epsilon
                
        0 = (1/2epsilon) * [+ (u_a_x(x+epsilon, y, t) - 1/P * dt * (p(x + 2epsilon, y, t) - p(x, y, t)))
                            - (u_a_x(x-epsilon, y, t) - 1/P * dt * (p(x, y, t) - p(x - 2epsilon, y, t)))
                            + (u_a_y(x, x+epsilon, t) - 1/P * dt * (p(x, y + 2epsilon, t) - p(x, y, t)))
                            - (u_a_x(x, y-epsilon, t) - 1/P * dt * (p(x, y, t) - p(x, x - 2epsilon, t)))]

        (dt/P) *
        ((p(x + 2epsilon, y, t) - p(x, y, t)) - (p(x, y, t) - p(x - 2epsilon, y, t)) +
         (p(x, y + 2epsilon, t) - p(x, y, t)) - (p(x, y, t) - p(x, x - 2epsilon, t))
        = [u_a_x(x+epsilon, y, t)) - u_a_x(x-epsilon, y, t)) + u_a_y(x, x+epsilon, t)) - u_a_x(x, y-epsilon, t))]

        (dt/P) * (-4*p(x, y, t) + p(x + 2epsilon, y, t) + p(x - 2epsilon, y, t) + p(x, y + 2epsilon, t) + p(x, x - 2epsilon, t)
        = [u_a_x(x+epsilon, y, t)) - u_a_x(x-epsilon, y, t)) + u_a_y(x, y+epsilon, t)) - u_a_x(x, y-epsilon, t))]

      All the values on RHS are known but LHS we have 5 unknowns for any chosen grid point. We replace each side with the following:

        d_i_j = (-P/dt)*[u_a_x((i+1)*epsilon, y, t)) -
                         u_a_x((i-1)*epsilon, y, t)) +
                         u_a_y(x, (j+1)*epsilon, t)) -
                         u_a_x(x, (j-1)*epsilon, t))]

      The post uses slightly different notation but I think they made a mistake with their constant term at teh front. Then,

        p_i_j = p(i*epsilon, j*epsilon, t)

      So we get the following:

        4*p_i_j - p_i+2_j - p_i-2_j - p_i_j+2 - p_i_j-2 = d_i_j

      The post solves this equation of 4 unknowns using Jacobi method which means equate for each variable in terms of hte others.
      Then, make a guess, calculate the new values, and repeat this process until you get something close to the solution.
                
 */

#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_nonuniform_qualifier: enable

#include "descriptor_layouts.cpp"
#include "blinn_phong_lighting.cpp"

layout(set = 0, binding = 0) uniform fluid_sim_inputs
{
    float FrameTime;
    float Density;
    float Epsilon;
    uint Dim;
} FluidSimInputs;

layout(set = 0, binding = 1) uniform sampler2D InputImage;
layout(set = 0, binding = 2) uniform sampler2D InputVelocity;

layout(set = 0, binding = 3, rg32f) uniform image2D OutputImage;
layout(set = 0, binding = 4, rg32f) uniform image2D OutputVelocity;

layout(set = 0, binding = 5, r32f) uniform image2D DivergenceImage;

//layout(set = 1, binding = 0, r32f) uniform image2D InputPressureImage;
layout(set = 1, binding = 0) uniform sampler2D InputPressureImage;
layout(set = 1, binding = 1, r32f) uniform image2D OutputPressureImage;

// TODO: Make global everywhere
#define Pi32 (3.141596f)

ivec2 TexelWrap(ivec2 Texel, ivec2 TextureSize)
{
    ivec2 Result = Texel;

    Result += TextureSize;
    Result = Result % TextureSize;
    
    //Result.x = Texel.x % TextureSize.x;
    //Result.y = Texel.y % TextureSize.y;
    
#if 0
    if (Result.x < 0)
    {
        Result.x += TextureSize.x;
    }
    else if (Result.x >= TextureSize.x)
    {
        Result.x -= TextureSize.x;
    }
    
    if (Result.y < 0)
    {
        Result.y += TextureSize.y;
    }
    else if (Result.y >= TextureSize.y)
    {
        Result.y -= TextureSize.y;
    }
#endif
    return Result;
}

//
// NOTE: Fluid Init
//

#if INIT

layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;
void main()
{
    vec2 TextureSize = vec2(FluidSimInputs.Dim, FluidSimInputs.Dim);
    if (gl_GlobalInvocationID.x < TextureSize.x && gl_GlobalInvocationID.y < TextureSize.y)
    {
        vec2 OutVelocity;

        // NOTE: Write vector field
        {
            // NOTE: Right vector field
            {
                
            }

            // TODO: Test Case 1:
            {
                /*
                if (gl_GlobalInvocationID.x < 100)
                {
                    OutVelocity = vec2(1, 0);
                }
                else if (gl_GlobalInvocationID.x < 200)
                {
                    OutVelocity = vec2(-1, 0);
                }
                else
                {
                    OutVelocity = vec2(1, 0);
                }
                */
            }

            // TODO: Test Case 2:
            {
                /*
                if (gl_GlobalInvocationID.x < 100)
                {
                    OutVelocity = vec2(1, 0);
                }
                else
                {
                    OutVelocity = vec2(-1, 0);
                }
                */
            }

            // TODO: Test Case 3:
            {
                /*
                if (gl_GlobalInvocationID.y < 100)
                {
                    OutVelocity = vec2(0, 1);
                }
                else
                {
                    OutVelocity = vec2(0, -1);
                }
                //*/
            }
            
            // NOTE: Circular vector field
            {
                ///*
                vec2 Pos = 2.0f * (gl_GlobalInvocationID.xy / TextureSize);
                OutVelocity = vec2(sin(2*Pi32*Pos.y), cos(2*Pi32*Pos.x));
                //*/
            }

            imageStore(OutputVelocity, ivec2(gl_GlobalInvocationID.xy), vec4(OutVelocity, 0, 0));
        }
        
        // NOTE: Write color image
        {
            vec4 OutColor;
            
            // NOTE: Write checker board
            ivec2 ModTexelPos = ivec2(gl_GlobalInvocationID.xy) % ivec2(64);
            if ((ModTexelPos.x < 32 && ModTexelPos.y < 32) || (ModTexelPos.x >= 32 && ModTexelPos.y >= 32))
            {
                OutColor = vec4(0, 0, 0, 1);
            }
            else
            {
                OutColor = vec4(1);
            }
            
            imageStore(OutputImage, ivec2(gl_GlobalInvocationID.xy), OutColor);
        }
    }    
}

#endif

//
// NOTE: Advection (Velocity)
//

#if ADVECTION

layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;
void main()
{
    vec2 TextureSize = vec2(FluidSimInputs.Dim, FluidSimInputs.Dim);
    if (gl_GlobalInvocationID.x < TextureSize.x && gl_GlobalInvocationID.y < TextureSize.y)
    {
        vec2 Velocity = texelFetch(InputVelocity, ivec2(gl_GlobalInvocationID.xy), 0).xy;

        // NOTE: Operate in UV space (centers are 0.5, 0.5)
        vec2 StartPosUv = (gl_GlobalInvocationID.xy + vec2(0.5f)) / TextureSize;
        vec2 StartPos = StartPosUv - Velocity * FluidSimInputs.FrameTime;
        
        // NOTE: Advect the velocity field
        vec2 NewVelocity = texture(InputVelocity, StartPos).xy;
        imageStore(OutputVelocity, ivec2(gl_GlobalInvocationID.xy), vec4(NewVelocity, 0, 0));

        // NOTE: Clear pressure
        //imageStore(InputPressureImage, ivec2(gl_GlobalInvocationID.xy), vec4(0, 0, 0, 0));
        imageStore(OutputPressureImage, ivec2(gl_GlobalInvocationID.xy), vec4(0, 0, 0, 0));
    }
}

#endif

//
// NOTE: Divergence
//

#if DIVERGENCE

layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;
void main()
{
    vec2 TextureSize = vec2(FluidSimInputs.Dim, FluidSimInputs.Dim);
    if (gl_GlobalInvocationID.x < TextureSize.x && gl_GlobalInvocationID.y < TextureSize.y)
    {
        // TODO: Can this be done via one gather op? It might be less precise since we don't go in the negative dir but might work
        float VelLeft = imageLoad(OutputVelocity, TexelWrap(ivec2(gl_GlobalInvocationID.xy) - ivec2(1, 0), ivec2(TextureSize))).x;
        float VelRight = imageLoad(OutputVelocity, TexelWrap(ivec2(gl_GlobalInvocationID.xy) + ivec2(1, 0), ivec2(TextureSize))).x;
        float VelUp = imageLoad(OutputVelocity, TexelWrap(ivec2(gl_GlobalInvocationID.xy) + ivec2(0, 1), ivec2(TextureSize))).y;
        float VelDown = imageLoad(OutputVelocity, TexelWrap(ivec2(gl_GlobalInvocationID.xy) - ivec2(0, 1), ivec2(TextureSize))).y;

        // TODO: We are assuming epsilon is the same in x and y
        float Epsilon = 1.0f / TextureSize.x;
        float Divergence = (-2 * FluidSimInputs.Epsilon * FluidSimInputs.Density / FluidSimInputs.FrameTime) * (VelRight - VelLeft + VelUp - VelDown);

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
        //float PressureLeft = imageLoad(InputPressureImage, TexelWrap(ivec2(gl_GlobalInvocationID.xy) - ivec2(2, 0), ivec2(TextureSize))).x;
        //float PressureRight = imageLoad(InputPressureImage, TexelWrap(ivec2(gl_GlobalInvocationID.xy) + ivec2(2, 0), ivec2(TextureSize))).x;
        //float PressureUp = imageLoad(InputPressureImage, TexelWrap(ivec2(gl_GlobalInvocationID.xy) + ivec2(0, 2), ivec2(TextureSize))).x;
        //float PressureDown = imageLoad(InputPressureImage, TexelWrap(ivec2(gl_GlobalInvocationID.xy) - ivec2(0, 2), ivec2(TextureSize))).x;

        vec2 Uv = (gl_GlobalInvocationID.xy + vec2(0.5)) / TextureSize;
        float PressureLeft = textureOffset(InputPressureImage, Uv, -ivec2(2, 0)).x;
        float PressureRight = textureOffset(InputPressureImage, Uv, ivec2(2, 0)).x;
        float PressureUp = textureOffset(InputPressureImage, Uv, ivec2(0, 2)).x;
        float PressureDown = textureOffset(InputPressureImage, Uv, -ivec2(0, 2)).x;
        
        float Divergence = imageLoad(DivergenceImage, ivec2(gl_GlobalInvocationID.xy)).x;
        float NewPressureCenter = (Divergence + PressureRight + PressureLeft + PressureUp + PressureDown) / 4.0f;

        imageStore(OutputPressureImage, ivec2(gl_GlobalInvocationID.xy), vec4(NewPressureCenter, 0, 0, 0));
    }
}

#endif

//
// NOTE: Pressure Application
//

#if PRESSURE_APPLY

layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;
void main()
{
    vec2 TextureSize = vec2(FluidSimInputs.Dim, FluidSimInputs.Dim);
    if (gl_GlobalInvocationID.x < TextureSize.x && gl_GlobalInvocationID.y < TextureSize.y)
    {
        // NOTE: Apply pressure
        vec2 CorrectedVel;
        {
            vec2 Uv = (gl_GlobalInvocationID.xy + vec2(0.5)) / TextureSize;
            float PressureLeft = textureOffset(InputPressureImage, Uv, -ivec2(1, 0)).x;
            float PressureRight = textureOffset(InputPressureImage, Uv, ivec2(1, 0)).x;
            float PressureUp = textureOffset(InputPressureImage, Uv, ivec2(0, 1)).x;
            float PressureDown = textureOffset(InputPressureImage, Uv, -ivec2(0, 1)).x;

            //float PressureLeft = imageLoad(InputPressureImage, TexelWrap(ivec2(gl_GlobalInvocationID.xy) - ivec2(1, 0), ivec2(TextureSize))).x;
            //float PressureRight = imageLoad(InputPressureImage, TexelWrap(ivec2(gl_GlobalInvocationID.xy) + ivec2(1, 0), ivec2(TextureSize))).x;
            //float PressureUp = imageLoad(InputPressureImage, TexelWrap(ivec2(gl_GlobalInvocationID.xy) + ivec2(0, 1), ivec2(TextureSize))).x;
            //float PressureDown = imageLoad(InputPressureImage, TexelWrap(ivec2(gl_GlobalInvocationID.xy) - ivec2(0, 1), ivec2(TextureSize))).x;

            float Multiplier = (FluidSimInputs.FrameTime / (2*FluidSimInputs.Density*FluidSimInputs.Epsilon));
            vec2 AdvectedVel = imageLoad(OutputVelocity, ivec2(gl_GlobalInvocationID.xy)).xy;
            CorrectedVel.x = AdvectedVel.x - Multiplier * (PressureRight - PressureLeft);
            CorrectedVel.y = AdvectedVel.y - Multiplier * (PressureUp - PressureDown);
            //CorrectedVel.x -= (FluidSimInputs.FrameTime / (FluidSimInputs.Density)) * (PressureRight - PressureLeft);
            //CorrectedVel.y -= (FluidSimInputs.FrameTime / (FluidSimInputs.Density)) * (PressureUp - PressureDown);
            
            imageStore(OutputVelocity, ivec2(gl_GlobalInvocationID.xy), vec4(CorrectedVel, 0, 0));
        }
        
        // NOTE: Advect the color
        {
            vec2 StartPosUv = (gl_GlobalInvocationID.xy + vec2(0.5f)) / TextureSize;
            vec2 StartPos = StartPosUv - CorrectedVel * FluidSimInputs.FrameTime;

            vec4 NewColor = texture(InputImage, StartPos);
            imageStore(OutputImage, ivec2(gl_GlobalInvocationID.xy), NewColor);
        }
    }
}

#endif
