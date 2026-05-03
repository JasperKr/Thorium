---@diagnostic disable: lowercase-global
local ffi = require("ffi")

jit.on(true, true)

local mat4Mt = {}
---@class matrix4x4
local mat4F = {
  [0] = { [0] = 0, 0, 0, 0 },
  [1] = { [0] = 0, 0, 0, 0 },
  [2] = { [0] = 0, 0, 0, 0 },
  [3] = { [0] = 0, 0, 0, 0 }
}
local mat3Mt = {}
---@class matrix3x3
local mat3F = {
  [0] = { [0] = 0, 0, 0 },
  [1] = { [0] = 0, 0, 0 },
  [2] = { [0] = 0, 0, 0 }
}

ffi.cdef [[
typedef struct {
  float m[16];
} Rmatrix4x4;

typedef struct {
  float m[9];
} Rmatrix3x3;
]]

ffi.metatype("Rmatrix4x4", mat4Mt)
ffi.metatype("Rmatrix3x3", mat3Mt)

---@return matrix4x4 matrix
function matrix4x4(...)
  local matrix = ffi.new("Rmatrix4x4")
  if select("#", ...) == 16 then
    for row = 0, 3 do
      for column = 0, 3 do
        local idx = row * 4 + column
        matrix.m[idx] = select(idx + 1, ...)
      end
    end
  elseif select("#", ...) == 4 then
    -- 4x {x=.,y=.,z=.,w=.}
    for rowIdx = 0, 3 do
      local row = select(rowIdx + 1, ...)
      matrix.m[rowIdx * 4 + 0] = row.x
      matrix.m[rowIdx * 4 + 1] = row.y
      matrix.m[rowIdx * 4 + 2] = row.z
      matrix.m[rowIdx * 4 + 3] = row.w
    end
  else
    matrix.m[0] = 1
    matrix.m[5] = 1
    matrix.m[10] = 1
    matrix.m[15] = 1
  end

  return matrix
end

---@return matrix3x3 matrix

function matrix3x3(...)
  local matrix = ffi.new("Rmatrix3x3")
  if select("#", ...) == 9 then
    for row = 0, 2 do
      for column = 0, 2 do
        local idx = row * 3 + column
        matrix.m[idx] = select(idx + 1, ...)
      end
    end
  elseif select("#", ...) == 3 then
    -- 3x {x=.,y=.,z=.}
    for rowIdx = 0, 2 do
      local row = select(rowIdx + 1, ...)
      matrix.m[rowIdx * 3 + 0] = row.x
      matrix.m[rowIdx * 3 + 1] = row.y
      matrix.m[rowIdx * 3 + 2] = row.z
    end
  else
    matrix.m[0] = 1
    matrix.m[4] = 1
    matrix.m[8] = 1
  end
  return matrix
end

mat3F.type = "matrix_3x3"
mat4F.type = "matrix_4x4"
function mat4Mt.__tostring(self)
  local m = self.m

  local str = string.format("[\n  %s, %s, %s, %s,\n  %s, %s, %s, %s,\n  %s, %s, %s, %s,\n  %s, %s, %s, %s\n]",
    tostring(m[0]), tostring(m[1]), tostring(m[2]), tostring(m[3]),
    tostring(m[4]), tostring(m[5]), tostring(m[6]), tostring(m[7]),
    tostring(m[8]), tostring(m[9]), tostring(m[10]), tostring(m[11]),
    tostring(m[12]), tostring(m[13]), tostring(m[14]), tostring(m[15])
  )

  return str
end

function mat3Mt.__tostring(self)
  local m = self.m
  local str = string.format("[\n  %s, %s, %s,\n  %s, %s, %s,\n  %s, %s, %s\n]",
    tostring(m[0]), tostring(m[1]), tostring(m[2]),
    tostring(m[3]), tostring(m[4]), tostring(m[5]),
    tostring(m[6]), tostring(m[7]), tostring(m[8])
  )

  return str
end

--- Multiply a matrix by a vector.
---@param vector vec4
---@param out vec4?
---@return vec4
function mat4F:vMul(vector, out)
  out = out or vec4()

  local m = self.m
  out.x = m[0] * vector.x + m[4] * vector.y + m[8] * vector.z + m[12] * vector.w
  out.y = m[1] * vector.x + m[5] * vector.y + m[9] * vector.z + m[13] * vector.w
  out.z = m[2] * vector.x + m[6] * vector.y + m[10] * vector.z + m[14] * vector.w
  out.w = m[3] * vector.x + m[7] * vector.y + m[11] * vector.z + m[15] * vector.w

  return out
end

function mat4F:vMulSep(x, y, z, w)
  local m = self.m
  local x1 = m[0] * x + m[4] * y + m[8] * z + m[12] * w
  local y1 = m[1] * x + m[5] * y + m[9] * z + m[13] * w
  local z1 = m[2] * x + m[6] * y + m[10] * z + m[14] * w
  local w1 = m[3] * x + m[7] * y + m[11] * z + m[15] * w

  return x1, y1, z1, w1
end

function mat4F:vMulSepW1(x, y, z)
  local m = self.m
  local x1 = m[0] * x + m[4] * y + m[8] * z + m[12]
  local y1 = m[1] * x + m[5] * y + m[9] * z + m[13]
  local z1 = m[2] * x + m[6] * y + m[10] * z + m[14]
  local w1 = m[3] * x + m[7] * y + m[11] * z + m[15]

  return x1, y1, z1, w1
end

function mat4F:vMulSepW0(x, y, z)
  local m = self.m
  local x1 = m[0] * x + m[4] * y + m[8] * z
  local y1 = m[1] * x + m[5] * y + m[9] * z
  local z1 = m[2] * x + m[6] * y + m[10] * z
  local w1 = m[3] * x + m[7] * y + m[11] * z

  return x1, y1, z1, w1
end

function mat3F:vMul(vector)
  local m = self.m
  local x = m[0] * vector.x + m[3] * vector.y + m[6] * vector.z
  local y = m[1] * vector.x + m[4] * vector.y + m[7] * vector.z
  local z = m[2] * vector.x + m[5] * vector.y + m[8] * vector.z

  return vec3(x, y, z)
end

function mat3F:vMulSep(x, y, z)
  local m = self.m
  local x1 = m[0] * x + m[3] * y + m[6] * z
  local y1 = m[1] * x + m[4] * y + m[7] * z
  local z1 = m[2] * x + m[5] * y + m[8] * z

  return x1, y1, z1
end

mat4Mt.__index = mat4F
mat3Mt.__index = mat3F

function mat4Mt.__mul(x, y)
  local m = ffi.new("Rmatrix4x4")
  for i = 0, 3 do
    for j = 0, 3 do
      local idx = i * 4 + j
      m.m[idx] = x.m[i * 4 + 0] * y.m[0 * 4 + j] + x.m[i * 4 + 1] * y.m[1 * 4 + j] +
          x.m[i * 4 + 2] * y.m[2 * 4 + j] + x.m[i * 4 + 3] * y.m[3 * 4 + j]
    end
  end

  return m
end

function mat3Mt.__mul(x, y)
  local m = ffi.new("Rmatrix3x3")
  for i = 0, 2 do
    for j = 0, 2 do
      local idx = i * 3 + j
      m.m[idx] = x.m[i * 3 + 0] * y.m[0 * 3 + j] + x.m[i * 3 + 1] * y.m[1 * 3 + j] +
          x.m[i * 3 + 2] * y.m[2 * 3 + j]
    end
  end

  return m
end

local temp4x4 = matrix4x4()

function mat4F:mul(y, out)
  if self == out then
    self:mul(y, temp4x4)
    return self:set(temp4x4)
  end

  for row = 0, 3 do
    for column = 0, 3 do
      local idx = row * 4 + column
      out.m[idx] = self.m[row * 4 + 0] * y.m[0 * 4 + column] + self.m[row * 4 + 1] * y.m[1 * 4 + column] +
          self.m[row * 4 + 2] * y.m[2 * 4 + column] + self.m[row * 4 + 3] * y.m[3 * 4 + column]
    end
  end

  return out
end

local temp3x3 = matrix3x3()

function mat3F:mul(y, out)
  if self == out then
    self:mul(y, temp3x3)
    return self:set(temp3x3)
  end

  for i = 0, 2 do
    for j = 0, 2 do
      local idx = i * 3 + j
      out.m[idx] = self.m[i * 3 + 0] * y.m[0 * 3 + j] + self.m[i * 3 + 1] * y.m[1 * 3 + j] +
          self.m[i * 3 + 2] * y.m[2 * 3 + j]
    end
  end

  return out
end

local tempMat4_2 = matrix4x4()
function mat4F:transpose(out)
  local v = out or matrix4x4()

  if out == self then
    v = tempMat4_2
  end

  local m = self.m
  -- v.m[0] = m[0]
  v.m[1] = m[4]
  v.m[2] = m[8]
  v.m[3] = m[12]

  v.m[4] = m[1]
  -- v.m[5] = m[5]
  v.m[6] = m[9]
  v.m[7] = m[13]

  v.m[8] = m[2]
  v.m[9] = m[6]
  -- v.m[10] = m[10]
  v.m[11] = m[14]

  v.m[12] = m[3]
  v.m[13] = m[7]
  v.m[14] = m[11]
  -- v.m[15] = m[15]

  if out == self then
    self:set(v)
  end

  return v
end

function mat3F:transpose(out)
  local v = out or matrix3x3()
  local m = self.m
  -- v.m[0] = m[0]
  v.m[1] = m[3]
  v.m[2] = m[6]

  v.m[3] = m[1]
  -- v.m[4] = m[4]
  v.m[5] = m[7]

  v.m[6] = m[2]
  v.m[7] = m[5]
  -- v.m[8] = m[8]

  return v
end

function mat4F:transposeSelf()
  local m = self.m
  -- v.m[0] = m[0]
  m[1] = m[4]
  m[2] = m[8]
  m[3] = m[12]

  m[4] = m[1]
  -- m.m[5] = m[5]
  m[6] = m[9]
  m[7] = m[13]

  m[8] = m[2]
  m[9] = m[6]
  -- m.m[10] = m[10]
  m[11] = m[14]

  m[12] = m[3]
  m[13] = m[7]
  m[14] = m[11]
  -- m.m[15] = m[15]

  return self
end

function mat3F:transposeSelf()
  local m = self.m

  -- v.m[0] = m[0]
  m[1] = m[3]
  m[2] = m[6]

  m[3] = m[1]
  -- m.m[4] = m[4]
  m[5] = m[7]

  m[6] = m[2]
  m[7] = m[5]
  -- m.m[8] = m[8]

  return self
end

function mat4F:copy()
  return ffi.new("Rmatrix4x4", self:get())
end

function mat3F:copy()
  return ffi.new("Rmatrix3x3", self:get())
end

function mat4F:table()
  local m = self.m

  return {
    m[0], m[1], m[2], m[3],
    m[4], m[5], m[6], m[7],
    m[8], m[9], m[10], m[11],
    m[12], m[13], m[14], m[15]
  }
end

function mat3F:table()
  local m = self.m

  return {
    m[0], m[1], m[2],
    m[3], m[4], m[5],
    m[6], m[7], m[8]
  }
end

function mat4F:writeToTable(t, index)
  for i = 0, 15 do
    t[index + i] = self.m[i]
  end
end

function mat3F:writeToTable(t, index)
  for i = 0, 8 do
    t[index + i] = self.m[i]
  end
end

function mat4F:getTransposed()
  local m = self.m
  return m[0], m[4], m[8], m[12],
      m[1], m[5], m[9], m[13],
      m[2], m[6], m[10], m[14],
      m[3], m[7], m[11], m[15]
end

function mat3F:getTransposed()
  local m = self.m
  return m[0], m[3], m[6],
      m[1], m[4], m[7],
      m[2], m[5], m[8]
end

function mat4F:get()
  local m = self.m
  return m[0], m[1], m[2], m[3],
      m[4], m[5], m[6], m[7],
      m[8], m[9], m[10], m[11],
      m[12], m[13], m[14], m[15]
end

function mat3F:get()
  local m = self.m
  return m[0], m[1], m[2],
      m[3], m[4], m[5],
      m[6], m[7], m[8]
end

local invMatrix = matrix4x4()

-- translated from love12's github: https://github.com/love2d/love/blob/12.0-development/src/common/Matrix.cpp matrix4:invert
--- Inverts the matrix.
---@param out matrix4x4?
---@return matrix4x4
function mat4F:invertTranspose(out)
  local inv = invMatrix.m
  local m = self.m

  inv[0] = m[5] * m[10] * m[15] -
      m[5] * m[11] * m[14] -
      m[9] * m[6] * m[15] +
      m[9] * m[7] * m[14] +
      m[13] * m[6] * m[11] -
      m[13] * m[7] * m[10];

  inv[4] = -m[4] * m[10] * m[15] +
      m[4] * m[11] * m[14] +
      m[8] * m[6] * m[15] -
      m[8] * m[7] * m[14] -
      m[12] * m[6] * m[11] +
      m[12] * m[7] * m[10];

  inv[8] = m[4] * m[9] * m[15] -
      m[4] * m[11] * m[13] -
      m[8] * m[5] * m[15] +
      m[8] * m[7] * m[13] +
      m[12] * m[5] * m[11] -
      m[12] * m[7] * m[9];

  inv[12] = -m[4] * m[9] * m[14] +
      m[4] * m[10] * m[13] +
      m[8] * m[5] * m[14] -
      m[8] * m[6] * m[13] -
      m[12] * m[5] * m[10] +
      m[12] * m[6] * m[9];

  inv[1] = -m[1] * m[10] * m[15] +
      m[1] * m[11] * m[14] +
      m[9] * m[2] * m[15] -
      m[9] * m[3] * m[14] -
      m[13] * m[2] * m[11] +
      m[13] * m[3] * m[10];

  inv[5] = m[0] * m[10] * m[15] -
      m[0] * m[11] * m[14] -
      m[8] * m[2] * m[15] +
      m[8] * m[3] * m[14] +
      m[12] * m[2] * m[11] -
      m[12] * m[3] * m[10];

  inv[9] = -m[0] * m[9] * m[15] +
      m[0] * m[11] * m[13] +
      m[8] * m[1] * m[15] -
      m[8] * m[3] * m[13] -
      m[12] * m[1] * m[11] +
      m[12] * m[3] * m[9];

  inv[13] = m[0] * m[9] * m[14] -
      m[0] * m[10] * m[13] -
      m[8] * m[1] * m[14] +
      m[8] * m[2] * m[13] +
      m[12] * m[1] * m[10] -
      m[12] * m[2] * m[9];

  inv[2] = m[1] * m[6] * m[15] -
      m[1] * m[7] * m[14] -
      m[5] * m[2] * m[15] +
      m[5] * m[3] * m[14] +
      m[13] * m[2] * m[7] -
      m[13] * m[3] * m[6];

  inv[6] = -m[0] * m[6] * m[15] +
      m[0] * m[7] * m[14] +
      m[4] * m[2] * m[15] -
      m[4] * m[3] * m[14] -
      m[12] * m[2] * m[7] +
      m[12] * m[3] * m[6];

  inv[10] = m[0] * m[5] * m[15] -
      m[0] * m[7] * m[13] -
      m[4] * m[1] * m[15] +
      m[4] * m[3] * m[13] +
      m[12] * m[1] * m[7] -
      m[12] * m[3] * m[5];

  inv[14] = -m[0] * m[5] * m[14] +
      m[0] * m[6] * m[13] +
      m[4] * m[1] * m[14] -
      m[4] * m[2] * m[13] -
      m[12] * m[1] * m[6] +
      m[12] * m[2] * m[5];

  inv[3] = -m[1] * m[6] * m[11] +
      m[1] * m[7] * m[10] +
      m[5] * m[2] * m[11] -
      m[5] * m[3] * m[10] -
      m[9] * m[2] * m[7] +
      m[9] * m[3] * m[6];

  inv[7] = m[0] * m[6] * m[11] -
      m[0] * m[7] * m[10] -
      m[4] * m[2] * m[11] +
      m[4] * m[3] * m[10] +
      m[8] * m[2] * m[7] -
      m[8] * m[3] * m[6];

  inv[11] = -m[0] * m[5] * m[11] +
      m[0] * m[7] * m[9] +
      m[4] * m[1] * m[11] -
      m[4] * m[3] * m[9] -
      m[8] * m[1] * m[7] +
      m[8] * m[3] * m[5];

  inv[15] = m[0] * m[5] * m[10] -
      m[0] * m[6] * m[9] -
      m[4] * m[1] * m[10] +
      m[4] * m[2] * m[9] +
      m[8] * m[1] * m[6] -
      m[8] * m[2] * m[5];

  local det = m[0] * inv[0] + m[1] * inv[4] + m[2] * inv[8] + m[3] * inv[12];
  local invdet = 1.0 / det

  for i = 0, 15 do
    inv[i] = inv[i] * invdet
  end

  return (out or matrix4x4()):set(invMatrix)
end

local invMatrix3 = matrix3x3()

-- translated from love12's github: https://github.com/love2d/love/blob/12.0-development/src/common/Matrix.cpp
function mat3F:invertTranspose(out)
  -- local e = self:table()
  -- e0 e3 e6
  -- e1 e4 e7
  -- e2 e5 e8
  local m = self.m

  local det = m[0] * (m[4] * m[8] - m[7] * m[5])
      - m[1] * (m[3] * m[8] - m[5] * m[6])
      + m[2] * (m[3] * m[7] - m[4] * m[6]);

  local invdet = 1.0 / det;
  local m2 = invMatrix3.m

  m2[0] = invdet * (m[4] * m[8] - m[7] * m[5]);
  m2[3] = -invdet * (m[1] * m[8] - m[2] * m[7]);
  m2[6] = invdet * (m[1] * m[5] - m[2] * m[4]);
  m2[1] = -invdet * (m[3] * m[8] - m[5] * m[6]);
  m2[4] = invdet * (m[0] * m[8] - m[2] * m[6]);
  m2[7] = -invdet * (m[0] * m[5] - m[3] * m[2]);
  m2[2] = invdet * (m[3] * m[7] - m[6] * m[4]);
  m2[5] = -invdet * (m[0] * m[7] - m[6] * m[1]);
  m2[8] = invdet * (m[0] * m[4] - m[3] * m[1]);

  return (out or matrix3x3()):set(invMatrix3)
end

local tempMat4 = matrix4x4()
function mat4F:invert(out)
  return self:invertTranspose(tempMat4):transpose(out)
end

local tempMat3 = matrix3x3()
function mat3F:invert(out)
  return self:invertTranspose(tempMat3):transpose(out)
end

function mat4F:set(mat)
  for i = 0, 15 do
    self.m[i] = mat.m[i]
  end

  return self
end

function mat3F:set(mat)
  for i = 0, 8 do
    self.m[i] = mat.m[i]
  end

  return self
end

function mat4F:setFromNumbers(...)
  for i = 0, 15 do
    self.m[i] = select(i + 1, ...)
  end

  return self
end

function mat3F:setFromNumbers(...)
  for i = 0, 8 do
    self.m[i] = select(i + 1, ...)
  end

  return self
end

function mat4F:clear()
  for i = 0, 15 do
    self.m[i] = 0
  end

  return self
end

function mat3F:clear()
  for i = 0, 8 do
    self.m[i] = 0
  end

  return self
end

function mat4F:identity()
  for i = 0, 15 do
    self.m[i] = 0
  end
  self.m[0] = 1
  self.m[5] = 1
  self.m[10] = 1
  self.m[15] = 1

  return self
end

function mat3F:identity()
  for i = 0, 8 do
    self.m[i] = 0
  end
  self.m[0] = 1
  self.m[4] = 1
  self.m[8] = 1

  return self
end
