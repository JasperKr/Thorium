Imgui = require("Editor.cimgui.init")
local ffi = require("ffi")

local lastDrawTime = 0
local lastImDrawTime = 0
local lastShownTime = 0
local lastShownImDrawTime = 0
local count = 0

local doneChannel, canStartChannel, scene = ...

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

  snap.graphics.aquireGraphics("gui")
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

  snap.graphics.submitGraphics()

  doneChannel:push(true)
end

print("THREAD #1 EXITING")
