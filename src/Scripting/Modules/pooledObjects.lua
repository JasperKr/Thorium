local ffi = require("ffi")

---@diagnostic disable: lowercase-global
local tempObjectHandlers = {}

--- Creates a new temporary object handler
---@param constructor function
---@param set function
---@param amount? integer
---@param ... unknown
---@return function
function SnapEngine.internal.newTempObjectHandler(constructor, set, amount, ...)
  amount = amount or 50

  local self = {
    tempObjects = {},
    index = 0,
    loopAmount = amount - 1,
    setNew = set,
  }

  self.get = function(...)
    self.index = self.index % self.loopAmount + 1
    self.setNew(self.tempObjects[self.index], ...)
    return self.tempObjects[self.index]
  end

  for i = 1, amount do
    table.insert(self.tempObjects, constructor(...))
  end
  table.insert(tempObjectHandlers, self)
  return self.get
end

---@diagnostic disable: return-type-mismatch
---@class ImVec2
---@field x number
---@field y number

---@class ImVec4
---@field x number
---@field y number
---@field z number
---@field w number

---@class float1
---@field [0] number

---@class float2
---@field [0] number
---@field [1] number

---@class float3
---@field [0] number
---@field [1] number
---@field [2] number

---@class float4
---@field [0] number
---@field [1] number
---@field [2] number
---@field [3] number

---@class int1
---@field [0] number

---@class int2
---@field [0] number
---@field [1] number

---@class int3
---@field [0] number
---@field [1] number
---@field [2] number

---@class int4
---@field [0] number
---@field [1] number
---@field [2] number
---@field [3] number

---@class bool1
---@field [0] boolean

---@return ImVec2
function SnapEngine.math.ImVec2(...)
  return ffi.new("ImVec2", ...)
end

---@return ImVec4
function SnapEngine.math.ImVec4(...)
  return ffi.new("ImVec4", ...)
end

---@param x number
---@return float1
function SnapEngine.math.float1(x)
  return ffi.new("float[1]", x)
end

---@param x number
---@param y number
---@return float2
function SnapEngine.math.float2(x, y)
  return ffi.new("float[2]", x, y)
end

---@param x number
---@param y number
---@param z number
---@return float3
function SnapEngine.math.float3(x, y, z)
  return ffi.new("float[3]", x, y, z)
end

---@param x number
---@param y number
---@param z number
---@param w number
---@return float4
function SnapEngine.math.float4(x, y, z, w)
  return ffi.new("float[4]", x, y, z, w)
end

---@param x number
---@return int1
function SnapEngine.math.int1(x)
  return ffi.new("int[1]", x)
end

---@param x number
---@param y number
---@return int2
function SnapEngine.math.int2(x, y)
  return ffi.new("int[2]", x, y)
end

---@param x number
---@param y number
---@param z number
---@return int3
function SnapEngine.math.int3(x, y, z)
  return ffi.new("int[3]", x, y, z)
end

---@param x number
---@param y number
---@param z number
---@param w number
---@return int4
function SnapEngine.math.int4(x, y, z, w)
  return ffi.new("int[4]", x, y, z, w)
end

---@param x boolean
---@return bool1
function SnapEngine.math.bool1(x)
  return (ffi.new("bool[1]", x))
end

if SnapEngine.math.ImVec2 then
  SnapEngine.math.tempImVec2 = SnapEngine.internal.newTempObjectHandler(SnapEngine.math.ImVec2, function(value, x, y)
    if type(x) == "cdata" then
      y = x.y; x = x.x
    end
    value.x = x or 0; value.y = y or 0
  end, nil, 0, 0)

  SnapEngine.math.tempImVec4 = SnapEngine.internal.newTempObjectHandler(SnapEngine.math.ImVec4,
    function(value, x, y, z, w)
      if type(x) == "cdata" then
        y = x.y
        z = x.z
        w = x.w
        x = x.x
      end
      value.x = x or 0; value.y = y or 0; value.z = z or 0; value.w = w or 0
    end, nil, 0, 0, 0, 0)

  SnapEngine.math.tempImColor = SnapEngine.internal.newTempObjectHandler(SnapEngine.math.ImVec4,
    function(value, r, g, b, a)
      if type(r) == "cdata" then
        g = r.y;
        b = r.z;
        a = r.w;
        r = r.x
      end

      r, g, b = snap.math.linearToGamma(r or 0, g or 0, b or 0)

      value.x = r; value.y = g; value.z = b; value.w = a or 1
    end, nil, 0, 0, 0, 1)

  SnapEngine.math.ImColor = function(r, g, b, a)
    if type(r) == "cdata" then
      g = r.y; b = r.z; a = r.w; r = r.x
    end

    r, g, b = snap.math.linearToGamma(r or 0, g or 0, b or 0)

    return SnapEngine.math.ImVec4(r, g, b, a or 1)
  end
end

SnapEngine.math.tempFloat1 = SnapEngine.internal.newTempObjectHandler(SnapEngine.math.float1, function(value, x)
  if type(x) == "cdata" then
    x = x[0]
  end
  value[0] = x or 0
end, nil, 0)

SnapEngine.math.tempFloat2 = SnapEngine.internal.newTempObjectHandler(SnapEngine.math.float2, function(value, x, y)
  if type(x) == "cdata" then
    y = x[1]
    x = x[0]
  end
  value[0] = x or 0; value[1] = y or 0
end, nil, 0, 0)

SnapEngine.math.tempFloat3 = SnapEngine.internal.newTempObjectHandler(SnapEngine.math.float3, function(value, x, y, z)
  if type(x) == "cdata" then
    y = x[1]
    z = x[2]
    x = x[0]
  end
  value[0] = x or 0; value[1] = y or 0; value[2] = z or 0
end, nil, 0, 0, 0)

SnapEngine.math.tempFloat4 = SnapEngine.internal.newTempObjectHandler(SnapEngine.math.float4, function(value, x, y, z, w)
  if type(x) == "cdata" then
    y = x[1]
    z = x[2]
    w = x[3]
    x = x[0]
  end
  value[0] = x or 0; value[1] = y or 0; value[2] = z or 0; value[3] = w or 0
end, nil, 0, 0, 0, 0)

SnapEngine.math.tempInt1 = SnapEngine.internal.newTempObjectHandler(SnapEngine.math.int1, function(value, x)
  if type(x) == "cdata" then
    x = x[0]
  end
  value[0] = x or 0
end, nil, 0)

SnapEngine.math.tempInt2 = SnapEngine.internal.newTempObjectHandler(SnapEngine.math.int2, function(value, x, y)
  if type(x) == "cdata" then
    y = x[1]
    x = x[0]
  end
  value[0] = x or 0; value[1] = y or 0
end, nil, 0, 0)

SnapEngine.math.tempInt3 = SnapEngine.internal.newTempObjectHandler(SnapEngine.math.int3, function(value, x, y, z)
  if type(x) == "cdata" then
    y = x[1]
    z = x[2]
    x = x[0]
  end
  value[0] = x or 0; value[1] = y or 0; value[2] = z or 0
end, nil, 0, 0, 0)

SnapEngine.math.tempInt4 = SnapEngine.internal.newTempObjectHandler(SnapEngine.math.int4, function(value, x, y, z, w)
  if type(x) == "cdata" then
    y = x[1]
    z = x[2]
    w = x[3]
    x = x[0]
  end
  value[0] = x or 0; value[1] = y or 0; value[2] = z or 0; value[3] = w or 0
end, nil, 0, 0, 0, 0)

SnapEngine.math.tempBool1 = SnapEngine.internal.newTempObjectHandler(SnapEngine.math.bool1, function(value, x)
  if type(x) == "cdata" then
    x = x[0]
  end
  value[0] = x or 0
end, nil, false)

SnapEngine.math.tempVec2 = SnapEngine.internal.newTempObjectHandler(vec2, function(value, x, y)
  if type(x) == "table" then
    value.x = x[1]; value.y = x[2]
  elseif type(x) == "cdata" then
    value.x = x.x; value.y = x.y
  else
    value.x = x or 0; value.y = y or 0
  end
end, nil, 0, 0)

SnapEngine.math.tempVec3 = SnapEngine.internal.newTempObjectHandler(vec3, function(value, x, y, z)
  if type(x) == "table" then
    value.x = x[1]; value.y = x[2]; value.z = x[3]
  elseif type(x) == "cdata" then
    value.x = x.x; value.y = x.y; value.z = x.z
  else
    value.x = x or 0; value.y = y or 0; value.z = z or 0
  end
end, nil, 0, 0, 0)

SnapEngine.math.tempVec4 = SnapEngine.internal.newTempObjectHandler(vec4, function(value, x, y, z, w)
  if type(x) == "table" then
    value.x = x[1]; value.y = x[2]; value.z = x[3]; value.w = x[4]
  elseif type(x) == "cdata" then
    value.x = x.x; value.y = x.y; value.z = x.z; value.w = x.w
  else
    value.x = x or 0; value.y = y or 0; value.z = z or 0; value.w = w or 0
  end
end, nil, 0, 0, 0, 0)

SnapEngine.math.tempQuaternion = SnapEngine.internal.newTempObjectHandler(quaternion, function(value, x, y, z, w)
  if type(x) == "table" then
    value.x = x[1]; value.y = x[2]; value.z = x[3]; value.w = x[4]
  elseif type(x) == "cdata" then
    value.x = x.x; value.y = x.y; value.z = x.z; value.w = x.w
  else
    value.x = x or 0; value.y = y or 0; value.z = z or 0; value.w = w or 0
  end
end, nil, 0, 0, 0, 0)
