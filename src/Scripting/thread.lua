Imgui = require("Editor.cimgui.init")
require("Modules.vec")
require("Modules.quaternions")
require("Modules.matrices")
require("Modules.math")
local ffi = require("ffi")

local lastDrawTime = 0
local lastImDrawTime = 0
local lastShownTime = 0
local lastShownImDrawTime = 0
local count = 0

local commandBufferChannel, canStartChannel, scene = ...

local t = snap.timer.getTime()
local testImgdata = snap.data.newImagedata(16, 16, "rgba8")
for y = 0, 15 do
  for x = 0, 15 do
    local value = snap.math.random()
    testImgdata:setPixel(x, y, 1, 1, 1, value)
  end
end

local vertexformat = {
  { name = "position", format = "floatvec2",  location = 0 },
  { name = "uv",       format = "floatvec2",  location = 1 },
  { name = "color",    format = "unorm8vec4", location = 2 },
}

local indices = { 1, 2, 3, 1, 3, 4 }
local vertices = {
  { 0,   0,   0, 0, 1, 1, 1, 1 },
  { 100, 0,   1, 0, 1, 1, 1, 1 },
  { 100, 100, 1, 1, 1, 1, 1, 1 },
  { 0,   100, 0, 1, 1, 1, 1, 1 },
}
local mesh

local shader = snap.graphics.newShader("Scripting/Graphics/Shaders/forward.slang")
local computeshader = snap.graphics.newShader("Scripting/Graphics/Shaders/test2.slang")
local value = 0.15
local cameraBuffer

local texture
local testnumber = ffi.new("float[1]")
print("Generated noise texture in " .. tostring(snap.timer.getTime() - t) .. " seconds")

local function draw()
  local startTime = snap.timer.getTime()
  Imgui.Begin("Test window")

  Imgui.Separator()
  Imgui.Text("FPS: " .. tostring(snap.timer.getFPS()))
  Imgui.Text("DT: " .. tostring(snap.timer.getDelta()))
  Imgui.Text("Last Draw Time (ms): " .. tostring(lastShownTime * 1000))
  Imgui.Text("Last ImGui Draw Time (ms): " .. tostring(lastShownImDrawTime * 1000))

  Imgui.Separator()
  Imgui.InputFloat("Value", testnumber, 0.01)

  Imgui.Separator()
  scene:drawUIElement()

  Imgui.End()
  Imgui.ShowDemoWindow()

  snap.gui.endFrame()
  local imStartTime = snap.timer.getTime()
  snap.gui.draw()
  snap.graphics.setScissor();
  lastImDrawTime = lastImDrawTime + snap.timer.getTime() - imStartTime

  snap.graphics.setShader(shader)
  shader:send("CameraData", cameraBuffer)
  scene:drawModels()
  snap.graphics.setShader()

  lastDrawTime = lastDrawTime + snap.timer.getTime() - startTime
  count = count + 1
  if (count >= 50) then
    lastShownTime = lastDrawTime / count
    lastShownImDrawTime = lastImDrawTime / count
    count = 0
    lastDrawTime = 0
    lastImDrawTime = 0
  end
end

local t = snap.timer.getTime()

while true do
  if not (canStartChannel:demand(1)) then
    break
  end

  snap.graphics.aquireGraphics()
  if not texture then
    print("New texture")
    texture = snap.graphics.newTexture(testImgdata, { storage = true, sampler = true })

    local format = {
      {
        name = "test",
        format = {
          { name = "position", format = "floatvec2" },
          { name = "uv",       format = "float" },
          { name = "color",    format = "float",    arraysize = 4 },
        }
      },
      { name = "test2", format = "uint32" }
    }

    mesh = snap.graphics.newMesh(vertexformat, vertices, "triangles")

    local indicesData = snap.data.newBytedata(#indices * 4)
    for j = 1, #indices do
      indicesData:setUInt32((j - 1) * 4, indices[j] - 1)
    end
    mesh:setIndices(indicesData)

    --[[
    public cbuffer CameraData {
  public float4 CameraPosition;
  public float4x4 ViewMatrix;
  public float4x4 InverseViewMatrix;
  public float4x4 RotationMatrix;
  public float4x4 InverseRotationMatrix;
  public float4x4 ProjectionMatrix;
  public float4x4 InverseProjectionMatrix;
  public float4x4 ViewProjectionMatrix;
  public float4x4 InverseViewProjectionMatrix;
  public float4x4 RotationProjectionMatrix;
  public float4x4 InverseRotationProjectionMatrix;
    };
    ]]

    local format = {
      { name = "CameraPosition",                  format = "floatvec4" },
      { name = "ViewMatrix",                      format = "floatmat4" },
      { name = "InverseViewMatrix",               format = "floatmat4" },
      { name = "RotationMatrix",                  format = "floatmat4" },
      { name = "InverseRotationMatrix",           format = "floatmat4" },
      { name = "ProjectionMatrix",                format = "floatmat4" },
      { name = "InverseProjectionMatrix",         format = "floatmat4" },
      { name = "ViewProjectionMatrix",            format = "floatmat4" },
      { name = "InverseViewProjectionMatrix",     format = "floatmat4" },
      { name = "RotationProjectionMatrix",        format = "floatmat4" },
      { name = "InverseRotationProjectionMatrix", format = "floatmat4" },
    }

    local viewMatrix = mat4()
    local inverseViewMatrix = mat4()
    local rotationMatrix = mat4()
    local inverseRotationMatrix = mat4()
    local projectionMatrix = mat4()
    local inverseProjectionMatrix = mat4()
    local viewProjectionMatrix = mat4()
    local inverseViewProjectionMatrix = mat4()
    local rotationProjectionMatrix = mat4()
    local inverseRotationProjectionMatrix = mat4()

    local matrices = {
      viewMatrix, inverseViewMatrix, rotationMatrix, inverseRotationMatrix,
      projectionMatrix, inverseProjectionMatrix, viewProjectionMatrix, inverseViewProjectionMatrix,
      rotationProjectionMatrix, inverseRotationProjectionMatrix
    }


    local dataArray = { 0, 0, 0, 0 }

    for i, matrix in ipairs(matrices) do
      local data = matrix:table()
      for j = 1, #data do
        table.insert(dataArray, #dataArray)
      end
    end

    cameraBuffer = snap.graphics.newBuffer(format, 1, { uniform = true })
    cameraBuffer:setData(dataArray)
  end

  ---@type snap.DetailedBlendMode
  local blendmode = {
    srccolor = "one",
    dstcolor = "oneminussrcalpha",
    colorop = "add",
    srcalpha = "one",
    dstalpha = "zero",
    alphaop = "add",
  }

  snap.graphics.setRenderTarget({ loadas = "clear", blendmode = { blendmode = "alpha", alphamode = "premultiplied" } })

  local dt = snap.timer.getTime() - t
  t = snap.timer.getTime()

  snap.gui.newFrame(dt)

  draw()

  commandBufferChannel:push(snap.graphics.submitGraphics())
end

print("THREAD #1 EXITING")
