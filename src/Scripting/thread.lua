engine = {
  helpers = {},
  graphics = {},
  math = {},
}

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

local cameraWidth = snap.graphics.getWidth() * 3 / 4
local cameraHeight = snap.graphics.getHeight() * 3 / 4

local camera = snap.graphics.newCamera("main camera", vec3(), quaternion(),
  vec2(cameraWidth, cameraHeight), 60, 0.1, 1000)

local t = snap.timer.getTime()
local testImgdata = snap.data.newImagedata(16, 16, "rgba8")
for y = 0, 15 do
  for x = 0, 15 do
    local value = snap.math.random()
    testImgdata:setPixel(x, y, 1, 1, 1, value)
  end
end

local shader = snap.graphics.newShader("Scripting/Graphics/Shaders/forward.slang")

local texture
print("Generated noise texture in " .. tostring(snap.timer.getTime() - t) .. " seconds")

local rendertarget
local depthbuffer
local snapshot

local function draw()
  snap.graphics.setRenderTarget(
    { { texture = rendertarget, loadas = "clear", blendmode = { blendmode = "alpha", alphamode = "premultiplied" } },
      { texture = depthbuffer,  loadas = 1 } })
  snap.graphics.setShader(shader)
  snap.graphics.setCullMode("none")
  camera:Update()
  camera:UpdateState()
  shader:send("CameraData", camera.buffer)
  snap.graphics.setCullMode("none")
  snap.graphics.setWindingOrder("ccw")
  snap.graphics.setDepthMode("less", true)
  scene:drawModels()
  snap.graphics.setShader()

  local startTime = snap.timer.getTime()
  Imgui.Begin("Test window")

  Imgui.Separator()
  Imgui.Text("FPS: " .. tostring(snap.timer.getFPS()))
  Imgui.Text("DT: " .. tostring(snap.timer.getDelta()))
  Imgui.Text("Last Draw Time (ms): " .. tostring(lastShownTime * 1000))
  Imgui.Text("Last ImGui Draw Time (ms): " .. tostring(lastShownImDrawTime * 1000))
  local toMb = 1 / 1024
  local memUsage = collectgarbage("count") * toMb
  Imgui.Text(string.format("Lua Memory Usage: %.1f MB", memUsage))

  Imgui.Separator()
  scene:drawUIElement()

  Imgui.End()

  Imgui.Begin("Test window 2")
  Imgui.Image(rendertarget, ffi.new("ImVec2", cameraWidth, cameraHeight))
  Imgui.End()

  Imgui.ShowDemoWindow()

  if snapshot then
    snapshot:draw()
  end

  snap.gui.endFrame()
  local imStartTime = snap.timer.getTime()
  snap.graphics.setRenderTarget({ loadas = "clear", blendmode = { blendmode = "alpha", alphamode = "premultiplied" } })
  snap.gui.draw()
  snap.graphics.setScissor();
  lastImDrawTime = lastImDrawTime + snap.timer.getTime() - imStartTime

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

local createSnapshot = false

local isDown = {}
function snap.mousepressed(x, y, button)
  isDown[button] = true
end

function snap.mousereleased(x, y, button)
  isDown[button] = false
end

function snap.keypressed(key)
  isDown[key] = true
  if key == "f5" then
    createSnapshot = true
  end
end

function snap.keyreleased(key)
  isDown[key] = false
end

function snap.mousemoved(x, y, dx, dy)
  if not isDown[3] then
    return
  end

  local quat = quaternion(snap.math.eulerToQuaternion(dx * 0.001, dy * 0.001, 0))
  local currentQuat = quaternion(camera:getRotation())
  local newQuat = quat * currentQuat
  camera:SetRotation(newQuat.x, newQuat.y, newQuat.z, newQuat.w)
end

local t = snap.timer.getTime()
local deltaTimestamp = snap.timer.getTime()

while true do
  if not (canStartChannel:demand(1)) then
    break
  end

  local delta = snap.timer.getTime() - deltaTimestamp
  deltaTimestamp = snap.timer.getTime()

  local event = events:pop()
  while event do
    if snap[event[1]] then
      snap[event[1]](unpack(event, 2))
    end

    event = events:pop()
  end

  local speed = delta * 10

  if (isDown["a"]) then
    local left = -camera:GetRight()
    local x, y, z = camera:getPosition()
    camera:SetPosition(x + left.x * speed, y + left.y * speed, z + left.z * speed)
  end

  if (isDown["d"]) then
    local right = camera:GetRight()
    local x, y, z = camera:getPosition()
    camera:SetPosition(x + right.x * speed, y + right.y * speed, z + right.z * speed)
  end

  if (isDown["w"]) then
    local forward = camera:GetForward()
    local x, y, z = camera:getPosition()
    camera:SetPosition(x + forward.x * speed, y + forward.y * speed, z + forward.z * speed)
  end

  if (isDown["s"]) then
    local back = -camera:GetForward()
    local x, y, z = camera:getPosition()
    camera:SetPosition(x + back.x * speed, y + back.y * speed, z + back.z * speed)
  end
  if createSnapshot then
    print("Requesting snapshot creation")
  end

  snap.graphics.aquireGraphics(nil, nil, createSnapshot)
  createSnapshot = false
  if not texture then
    snap.graphics.setDefaultFilter("linear", "linear", 4)
    texture = snap.graphics.newTexture(testImgdata, { storage = true, sampler = true, mipmaps = "init" })

    snap.scene.loadModel(scene, "Assets/Terrain/Sponza/Sponza.gltf")

    rendertarget = snap.graphics.newTexture(snap.graphics.getWidth(), snap.graphics.getHeight(),
      { sampler = true, rendertarget = true })
    depthbuffer = snap.graphics.newTexture(snap.graphics.getWidth(), snap.graphics.getHeight(),
      { rendertarget = true, format = "depth32f" })
  end

  local dt = snap.timer.getTime() - t
  t = snap.timer.getTime()

  snap.gui.newFrame(dt)

  draw()

  local commands, newSnapshot = snap.graphics.submitGraphics()
  if newSnapshot then
    snapshot = newSnapshot
  end

  commandBufferChannel:push(commands)
end

print("THREAD #1 EXITING")
