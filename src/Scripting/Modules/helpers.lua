snap.helpers = snap.helpers or {}

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
  local tanHalfFov                           = math.tan(math.rad(fov / 2))
  out                                        = out or mat4()

  local height                               = 1 / tanHalfFov
  local width                                = height / aspectRatio
  local range                                = far / (far - near)

  -- row-major
  out[1][1], out[1][2], out[1][3], out[1][4] = width, 0, 0, 0
  out[2][1], out[2][2], out[2][3], out[2][4] = 0, height, 0, 0
  out[3][1], out[3][2], out[3][3], out[3][4] = 0, 0, range, 1
  out[4][1], out[4][2], out[4][3], out[4][4] = 0, 0, -range * near, 0

  return out
end

local function printAnyInternal(any, tabs)
  if type(any) == "table" then
    local result = "{\n"
    for key, value in pairs(any) do
      result = result ..
          string.rep("  ", tabs + 1) .. tostring(key) .. ": " .. printAnyInternal(value, tabs + 1) .. ",\n"
    end
    return result .. string.rep("  ", tabs) .. "}"
  else
    return tostring(any)
  end
end

local function printAnyCompactInternal(any, tabs)
  if type(any) == "table" then
    local result = "{ "
    local count = 0
    for key, value in pairs(any) do
      count = count + 1
    end

    if count <= 5 then
      local result = "{ "
      local i = 0
      for key, value in pairs(any) do
        i = i + 1
        result = result .. tostring(key) .. ": " .. printAnyCompactInternal(value, tabs + 1)
        if i < count then
          result = result .. ", "
        end
      end

      return result .. " }"
    else
      local result = "{\n"
      for key, value in pairs(any) do
        result = result ..
            string.rep("  ", tabs + 1) .. tostring(key) .. ": " .. printAnyCompactInternal(value, tabs + 1) .. ",\n"
      end
      return result .. string.rep("  ", tabs) .. "}"
    end
  else
    return tostring(any)
  end
end

function snap.helpers.print(...)
  for i = 1, select("#", ...) do
    local any = select(i, ...)
    print(printAnyInternal(any, 0))
  end
end

function snap.helpers.printCompact(...)
  for i = 1, select("#", ...) do
    local any = select(i, ...)
    print(printAnyCompactInternal(any, 0))
  end
end

local function reverse_iterator(tbl, i)
  i = i - 1
  local v = tbl[i]
  if nil ~= v then
    return i, v
  end
end

---@diagnostic disable-next-line: lowercase-global
function reverse_ipairs(t)
  return reverse_iterator, t, #t + 1
end
