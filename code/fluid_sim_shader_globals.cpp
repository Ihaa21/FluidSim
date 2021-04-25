
/*
  NOTE: http://jamie-wong.com/2016/08/05/webgl-fluid-simulation/
        https://codepen.io/davvidbaker/pen/ENbqdQ


      Ideas:

        - Can we convert some of these shaders to use gather ops instead? Currently we scoop +1, -1 in each axis. Gather would be 0 and 1
          so potentially less precise but might be much faster?
*/

// TODO: Make global everywhere
#define Pi32 (3.141592653589f)

vec2 AdvectionFindPos(vec2 Velocity, vec2 GlobalThreadId, vec2 TextureSize, float FrameTime)
{
    // NOTE: Operate in UV space (centers are 0.5, 0.5)
    vec2 StartPosUv = (GlobalThreadId + vec2(0.5f)) / TextureSize;
    vec2 Result = StartPosUv - Velocity * FrameTime;

    return Result;
}

ivec2 MirrorPixelCoord(ivec2 SamplePos, ivec2 TextureSize)
{
    ivec2 Result;
    
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
    Result = ivec2(Uv * TextureSize);

    return Result;
}

ivec2 ClampPixelCoord(ivec2 SamplePos, ivec2 SampleOffset, vec2 TextureSize)
{
    ivec2 Result;

    // NOTE: If we are out of bounds, use the pressure in the center for our result
    if ((SamplePos.x + SampleOffset.x) < 0 || (SamplePos.x + SampleOffset.x) >= TextureSize.x ||
        (SamplePos.y + SampleOffset.y) < 0 || (SamplePos.y + SampleOffset.y) >= TextureSize.y)
    {
        Result = SamplePos;
    }
    else
    {
        Result = SamplePos + SampleOffset;
    }
    
    return Result;
}
