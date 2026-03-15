---@param left number
---@param right number
---@param top number
---@param bottom number
---@param near number
---@param far number
---@param out matrix4x4?
---@return matrix4x4
function snap.graphics.newOrthographicProjectionMatrix(left, right, top, bottom, near, far, out)
  out = out or mat4()

  out:setFromNumbers(
    2 / (right - left), 0, 0, -(right + left) / (right - left),
    0, -2 / (top - bottom), 0, -(top + bottom) / (top - bottom),
    0, 0, -2 / (far - near), -(far + near) / (far - near),
    0, 0, 0, 1
  )

  return out
end

function snap.graphics.newPerspectiveProjectionMatrix(left, right, top, bottom, near, far)
  return mat4(
    (near * 2) / (right - left), 0, (right + left) / (right - left), 0,
    0, -(near * 2) / (top - bottom), (top + bottom) / (top - bottom), 0,
    0, 0, -((far + near) / (far - near)), -(2 * far * near) / (far - near),
    0, 0, -1, 0
  )
end

function snap.graphics.newPerspectiveProjectionMatrixSimple(aspectRatio, fov, near, far, out)
  local tanHalfFov = math.tan(math.rad(fov / 2))

  out = out or mat4()

  out[1][1], out[2][1], out[3][1], out[4][1] = 1 / (aspectRatio * tanHalfFov), 0, 0, 0
  out[1][2], out[2][2], out[3][2], out[4][2] = 0, -1 / tanHalfFov, 0, 0
  out[1][3], out[2][3], out[3][3], out[4][3] = 0, 0, -(far + near) / (far - near), -(2 * far * near) / (far - near)
  out[1][4], out[2][4], out[3][4], out[4][4] = 0, 0, -1, 0

  return out
end
