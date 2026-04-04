---@class snap.camera
---@field position vec3
---@field rotation vec3
---@field nextPosition vec3 Next frame's position
---@field nextRotation vec3 Next frame's rotation
---@field resolution vec2
---@field fov number
---@field near number
---@field far number
---@
---@field projectionMatrix matrix4x4
---@field inverseProjectionMatrix matrix4x4
---@field translationMatrix matrix4x4
---@field rotationMatrix matrix4x4
---@field viewMatrix matrix4x4
---@field inverseViewMatrix matrix4x4
---@field viewProjectionMatrix matrix4x4
---@field inverseViewProjectionMatrix matrix4x4
---@field inverseRotationMatrix matrix4x4
---@field rotationProjectionMatrix matrix4x4
---@field inverseRotationProjectionMatrix matrix4x4
---@
---@field frustum Frustum
---@field postProcessing boolean
---@field name string
---@field id string
---@field bloom table
---@field linear boolean
---@field specular boolean
---@field buffer snap.Buffer
---@field bufferData snap.Bytedata
---@field textures {static:table, current:table, previous:table, internal:table}
---@field interactable boolean
---@field lighting {tileBuffer: snap.Texture, indexBuffer: snap.Buffer, indexCounterBuffer: snap.Buffer, indicesPerVoxel: number, tileWidth: number, tileHeight: number}
---@field onResizeFunctions function[]
---@field invalidatedHistory boolean
local Camera = {}
Camera.__index = Camera

--- Creates a new camera
---@param name string
---@param position vec3
---@param rotation vec3
---@param resolution vec2
---@param fov number?
---@param near number?
---@param far number?
---@return snap.camera
function snap.graphics.newCamera(name, position, rotation, resolution, fov, near, far)
  local self = setmetatable({}, Camera)

  -- Enforce that the resolution is a multiple of 32, for tiled lighting
  resolution.x = math.ceil(resolution.x / 32) * 32
  resolution.y = math.ceil(resolution.y / 32) * 32

  self.position = position
  self.rotation = rotation
  self.resolution = resolution or error("No resolution provided for camera")
  self.fov = fov or math.rad(90)
  self.near = near or 0.1
  self.far = far or 1000

  self.nextPosition = position
  self.nextRotation = rotation

  local aspectRatio = resolution.x / resolution.y

  local inverseProjectionMatrix = mat4()
  local viewMatrix = mat4()
  local inverseViewMatrix = mat4()
  local viewProjectionMatrix = mat4()
  local inverseViewProjectionMatrix = mat4()
  local rotationProjectionMatrix = mat4()
  local inverseRotationProjectionMatrix = mat4()

  local projectionMatrix = snap.graphics.newPerspectiveProjectionMatrixSimple(aspectRatio, self.fov, self.near,
    self.far)

  projectionMatrix:invertTranspose(inverseProjectionMatrix)
  local translationMatrix = snap.math.newTranslationMatrix(-position)
  local rotationMatrix = snap.math.eulerToMatrix(rotation:get())
  rotationMatrix:mul(projectionMatrix, rotationProjectionMatrix)
  rotationMatrix:invertTranspose(inverseRotationProjectionMatrix)

  translationMatrix:mul(rotationMatrix, viewMatrix)
  viewMatrix:invertTranspose(inverseViewMatrix)
  viewMatrix:mul(projectionMatrix, viewProjectionMatrix)
  viewProjectionMatrix:invertTranspose(inverseViewProjectionMatrix)

  self.projectionMatrix = projectionMatrix
  self.inverseProjectionMatrix = inverseProjectionMatrix
  self.translationMatrix = translationMatrix
  self.rotationMatrix = rotationMatrix
  self.viewMatrix = viewMatrix
  self.inverseViewMatrix = inverseViewMatrix
  self.viewProjectionMatrix = viewProjectionMatrix
  self.inverseViewProjectionMatrix = inverseViewProjectionMatrix
  self.rotationProjectionMatrix = rotationProjectionMatrix
  self.inverseRotationProjectionMatrix = inverseRotationProjectionMatrix

  self.textures = {
    static = { -- textures that don't change every frame
      geometry = {},
      gtao = {},
    },
    current = {},  -- textures that change every frame, the current frame
    previous = {}, -- textures that change every frame, the previous frame
    internal = {   -- internal data for managing texture swaps
      allScenes = {},
      allSceneViews = {},
      allDepths = {},
      allDepthsLoZ = {},
      allDepthViews = {},
      allLoZDepthViews = {},
      allReflections = {},
    }
  }

  self.bloom = {}
  self.name = name

  local cameraElementFormat = {
    { name = "ViewMatrix",                      format = "floatmat4" },
    { name = "InverseViewMatrix",               format = "floatmat4" },
    { name = "ProjectionMatrix",                format = "floatmat4" },
    { name = "InverseProjectionMatrix",         format = "floatmat4" },
    { name = "ViewProjectionMatrix",            format = "floatmat4" },
    { name = "InverseViewProjectionMatrix",     format = "floatmat4" },
    { name = "RotationProjectionMatrix",        format = "floatmat4" },
    { name = "InverseRotationProjectionMatrix", format = "floatmat4" },
    { name = "Position",                        format = "floatvec3" },
    { name = "Near",                            format = "float" },
    { name = "Far",                             format = "float" },
    { name = "NearMulFar",                      format = "float" },
    { name = "FarMinusNear",                    format = "float" },
    { name = "HistoryInvalidated",              format = "uint32" },
    { name = "Jitter",                          format = "floatvec2" },
    { name = "ProjectionType",                  format = "uint32" },
    { name = "ShadowCascadeCount",              format = "uint32" },
  }

  self.buffer = snap.graphics.newBuffer({
    {
      name = "camera",
      format = cameraElementFormat
    },
    {
      name = "previousCamera",
      format = cameraElementFormat
    }
  }, 1, { uniform = true, debugname = "Camera [" .. name .. "] buffer", usage = "dynamic" })
  self.bufferData = snap.data.newBytedata(self.buffer:getSize())

  return self
end

local tempVec3 = vec3()

function Camera:UpdateMatrices()
  local aspectRatio = self.resolution.x / self.resolution.y

  snap.graphics.newPerspectiveProjectionMatrixSimple(aspectRatio, self.fov, self.near,
    self.far, self.projectionMatrix)

  self.projectionMatrix:invertTranspose(self.inverseProjectionMatrix)
  snap.math.newTranslationMatrix(mathv.unm3(self.position, tempVec3),
    self.translationMatrix)
  snap.math.eulerToMatrix(self.rotation.x, self.rotation.y, self.rotation.z, self.rotationMatrix)
  self.rotationMatrix:mul(self.projectionMatrix, self.rotationProjectionMatrix)
  self.rotationMatrix:invertTranspose(self.inverseRotationProjectionMatrix)

  self.translationMatrix:mul(self.rotationMatrix, self.viewMatrix)
  self.viewMatrix:invertTranspose(self.inverseViewMatrix)
  self.viewMatrix:mul(self.projectionMatrix, self.viewProjectionMatrix)
  self.viewProjectionMatrix:invertTranspose(self.inverseViewProjectionMatrix)
end

function Camera:UpdateState()
  self:UpdateMatrices()
end

function Camera:Update()
  if self.nextPosition == self.position and self.nextRotation == self.rotation then
    return
  end

  self.position = self.nextPosition
  self.rotation = self.nextRotation
end

---@return vec3 forward
function Camera:GetForward()
  tempVec3:set(self.inverseRotationMatrix[3][1], self.inverseRotationMatrix[3][2], self.inverseRotationMatrix[3][3])
  return tempVec3
end

---@return vec3 right
function Camera:GetRight()
  tempVec3:set(self.inverseRotationMatrix[1][1], self.inverseRotationMatrix[1][2], self.inverseRotationMatrix[1][3])
  return tempVec3
end

---@return vec3 up
function Camera:GetUp()
  tempVec3:set(self.inverseRotationMatrix[2][1], self.inverseRotationMatrix[2][2], self.inverseRotationMatrix[2][3])
  return tempVec3
end

local jitterSequence = {
  { 0,     0 },
  { 0.5,   0.5 },
  { 0.5,   -0.5 },
  { -0.5,  0.5 },
  { -0.5,  -0.5 },
  { 0.25,  0.25 },
  { 0.25,  -0.25 },
  { -0.25, 0.25 },
  { -0.25, -0.25 }
}

local jitter = { 0, 0 }

function Camera:UpdateGpuBuffers()
  self.bufferData:setFloat(0, self.viewMatrix:get())
  self.bufferData:setFloat(64, self.inverseViewMatrix:get())
  self.bufferData:setFloat(128, self.projectionMatrix:get())
  self.bufferData:setFloat(192, self.inverseProjectionMatrix:get())
  self.bufferData:setFloat(256, self.viewProjectionMatrix:get())
  self.bufferData:setFloat(320, self.inverseViewProjectionMatrix:get())
  self.bufferData:setFloat(384, self.rotationProjectionMatrix:get())
  self.bufferData:setFloat(448, self.inverseRotationProjectionMatrix:get())
  self.bufferData:setFloat(512, self.position:get())
  self.bufferData:setFloat(524, self.near)
  self.bufferData:setFloat(528, self.far)
  self.bufferData:setFloat(532, self.near * self.far)
  self.bufferData:setFloat(536, self.far - self.near)
  self.bufferData:setUint(540, self.invalidatedHistory and 1 or 0)
  self.bufferData:setFloat(544, jitter[1])
  self.bufferData:setFloat(548, jitter[2])
end
