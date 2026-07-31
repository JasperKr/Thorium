module raytracing;

public restrict RaytracingAccelerationStructure SceneBVH;

public bool TraceAnyHitRay(RaytracingAccelerationStructure sceneBVH,
                           RayDesc ray) {
  RayQuery<RAY_FLAG_SKIP_PROCEDURAL_PRIMITIVES |
           RAY_FLAG_CULL_BACK_FACING_TRIANGLES |
           RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH> q;

  q.TraceRayInline(sceneBVH,
                   RAY_FLAG_SKIP_PROCEDURAL_PRIMITIVES |
                       RAY_FLAG_CULL_BACK_FACING_TRIANGLES |
                       RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH,
                   0xff, ray);

  while (q.Proceed()) {
  }

  return q.CommittedStatus() == COMMITTED_TRIANGLE_HIT;
}
