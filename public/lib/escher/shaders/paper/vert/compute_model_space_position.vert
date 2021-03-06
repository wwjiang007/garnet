#ifdef WOBBLE_VERTEX_POSITION

#error Not implemented.

#else

layout(set = 1, binding = 0) uniform PerObject {
  mat4 model_transform;
  vec4 color;
  // Format of each plane:
  // - xyz: normalized plane normal, pointing toward the non-clipped half-space.
  // - w: distance from origin to plane along plane normal vector, negated.
  vec4 clip_planes[NUM_CLIP_PLANES];
};

vec4 ComputeModelSpacePosition() {
  return vec4(inPosition, 1);
}

#endif
