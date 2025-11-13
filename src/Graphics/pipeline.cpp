#include "graphics.hpp"

namespace Graphics {

auto CompareBlendModes(BlendMode *blendmodeA, BlendMode *blendmodeB) -> bool {
  if (blendmodeA->enabled != blendmodeB->enabled) {
    return false;
  }
  if (blendmodeA->srcColorBlendFactor != blendmodeB->srcColorBlendFactor) {
    return false;
  }
  if (blendmodeA->dstColorBlendFactor != blendmodeB->dstColorBlendFactor) {
    return false;
  }
  if (blendmodeA->srcAlphaBlendFactor != blendmodeB->srcAlphaBlendFactor) {
    return false;
  }
  if (blendmodeA->dstAlphaBlendFactor != blendmodeB->dstAlphaBlendFactor) {
    return false;
  }

  return true;
}

auto ComparePipelineStates(GraphicsState *stateA, GraphicsState *stateB)
    -> int {
  // Returns 0 if equal, 1 if requires no rebuild, 2 if requires rebuild
  if (!CompareBlendModes(&stateA->blendMode, &stateB->blendMode)) {
    return 1;
  }
  if (stateA->depthWriteEnable != stateB->depthWriteEnable) {
    return 1;
  }
  if (stateA->cullMode != stateB->cullMode) {
    return 1;
  }
  if (stateA->frontFace != stateB->frontFace) {
    return 1;
  }
  if (stateA->depthTestEnable != stateB->depthTestEnable) {
    return 1;
  }
  if (stateA->stencilCompareOp != stateB->stencilCompareOp) {
    return 1;
  }
  if (stateA->stencilReference != stateB->stencilReference) {
    return 1;
  }
  if (stateA->backfaceCulling != stateB->backfaceCulling) {
    return 1;
  }
  if (stateA->frontfaceClockwise != stateB->frontfaceClockwise) {
    return 1;
  }
  if (stateA->depthCompareOp != stateB->depthCompareOp) {
    return 1;
  }

  return 0;
}

} // namespace Graphics