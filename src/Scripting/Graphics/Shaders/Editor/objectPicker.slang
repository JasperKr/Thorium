import raytracing;
import camera;

struct PushConstantBuffer {
  float2 uv;
};

[[vk::push_constant]]
PushConstantBuffer PushConstants;

struct TraceResult {
  uint instanceIndex;
  uint primitiveIndex;
  uint hit;
  uint padding;
};

RWStructuredBuffer<TraceResult> TraceResults;

[shader("compute")]
[numthreads(1, 1, 1)]
void computeMain(uint3 id: SV_DispatchThreadID) {
  float3 rayOrigin = camera.Position;
  float3 rayDirection = camera.GetRayDirection(PushConstants.uv);

  RayDesc ray;
  ray.Origin = rayOrigin;
  ray.Direction = rayDirection;
  ray.TMin = camera.Near;
  ray.TMax = camera.Far;

  RayQuery<RAY_FLAG_SKIP_PROCEDURAL_PRIMITIVES | RAY_FLAG_FORCE_OPAQUE> q;
  q.TraceRayInline(SceneBVH,
                   RAY_FLAG_SKIP_PROCEDURAL_PRIMITIVES | RAY_FLAG_FORCE_OPAQUE,
                   0xff, ray);

  // search for the closest hit
  while (q.Proceed()) {
  }

  if (q.CommittedStatus() == COMMITTED_TRIANGLE_HIT) {
    TraceResults[0].hit = 1;
    TraceResults[0].instanceIndex = q.CommittedInstanceIndex();
    TraceResults[0].primitiveIndex = q.CommittedPrimitiveIndex();
  } else {
    TraceResults[0].hit = 0;
  }
}
