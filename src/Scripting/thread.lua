Imgui = require("Editor.cimgui.init")
require("Modules.vec")
require("Modules.quaternions")
require("Modules.matrices")
require("Modules.math")
require("Modules.helpers")
require("Graphics.camera")
local ffi = require("ffi")

local lastDrawTime = 0
local lastImDrawTime = 0
local lastShownTime = 0
local lastShownImDrawTime = 0
local count = 0

local commandBufferChannel, canStartChannel, scene, events = ...

local camera = snap.graphics.newCamera("main camera", vec3(), quaternion(),
  vec2(snap.graphics.getDimensions()), 60, 0.1, 1000)

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

-- Cube vertices
local vertices = {
  { -1, -1, -1, 0, 1, 1, 1, 1 },
  { 1,  -1, -1, 1, 1, 1, 1, 1 },
  { 1,  1,  -1, 1, 0, 1, 1, 1 },
  { -1, 1,  -1, 0, 0, 1, 1, 1 },
  { -1, -1, 1,  0, 1, 1, 1, 1 },
  { 1,  -1, 1,  1, 1, 1, 1, 1 },
  { 1,  1,  1,  1, 0, 1, 1, 1 },
  { -1, 1,  1,  0, 0, 1, 1, 1 },
}

local indices = {
  1, 2, 3, 1, 3, 4, -- back face
  5, 6, 7, 5, 7, 8, -- front face
  1, 2, 6, 1, 6, 5, -- bottom face
  2, 3, 7, 2, 7, 6, -- right face
  3, 4, 8, 3, 8, 7, -- top face
  4, 1, 5, 4, 5, 8, -- left face
}

local mesh

local shader = snap.graphics.newShader("Scripting/Graphics/Shaders/forward.slang")

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
  camera:Update()
  camera:UpdateState()
  shader:send("transform", "Position", 0, 0, 5)
  shader:send("transform", "Rotation", 0, 0, 0, 1)
  shader:send("transform", "Scale", 1, 1, 1)
  shader:send("CameraData", camera.buffer)
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

local isDown = {}
function snap.mousepressed(x, y, button)
  isDown[button] = true
end

function snap.mousereleased(x, y, button)
  isDown[button] = false
end

function snap.keypressed(key)
  isDown[key] = true
end

function snap.keyreleased(key)
  isDown[key] = false
end

function snap.mousemoved(x, y, dx, dy)
  if not isDown[3] then
    return
  end

  local quat = snap.math.eulerToQuaternion(-dy * 0.001, dx * 0.001, 0)
  local currentQuat = quaternion(camera:getRotation())
  local newQuat = quat * currentQuat
  camera:SetRotation(newQuat.x, newQuat.y, newQuat.z, newQuat.w)
end

local t = snap.timer.getTime()

while true do
  if not (canStartChannel:demand(1)) then
    break
  end

  local event = events:pop()
  while event do
    if snap[event[1]] then
      snap[event[1]](unpack(event, 2))
    end

    event = events:pop()
  end

  if (isDown["a"]) then
    local left = -camera:GetRight()
    local x, y, z = camera:getPosition()
    camera:SetPosition(x + left.x * 0.1, y + left.y * 0.1, z + left.z * 0.1)
  end

  if (isDown["d"]) then
    local right = camera:GetRight()
    local x, y, z = camera:getPosition()
    camera:SetPosition(x + right.x * 0.1, y + right.y * 0.1, z + right.z * 0.1)
  end

  if (isDown["w"]) then
    local forward = camera:GetForward()
    local x, y, z = camera:getPosition()
    camera:SetPosition(x + forward.x * 0.1, y + forward.y * 0.1, z + forward.z * 0.1)
  end

  if (isDown["s"]) then
    local back = -camera:GetForward()
    local x, y, z = camera:getPosition()
    camera:SetPosition(x + back.x * 0.1, y + back.y * 0.1, z + back.z * 0.1)
  end

  snap.graphics.aquireGraphics()
  if not texture then
    texture = snap.graphics.newTexture(testImgdata, { storage = true, sampler = true })
    mesh = snap.graphics.newMesh(vertexformat, vertices, "triangles")

    local indicesData = snap.data.newBytedata(#indices * 4)
    for j = 1, #indices do
      indicesData:setUInt32((j - 1) * 4, indices[j] - 1)
    end
    mesh:setIndices(indicesData)
  end

  snap.graphics.setRenderTarget({ loadas = "clear", blendmode = { blendmode = "alpha", alphamode = "premultiplied" } })

  local dt = snap.timer.getTime() - t
  t = snap.timer.getTime()

  snap.gui.newFrame(dt)

  draw()

  commandBufferChannel:push(snap.graphics.submitGraphics())
end

print("THREAD #1 EXITING")
