Imgui = require("Editor.cimgui.init")

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
    -- local value = snap.math.noiseWrapped(x / 16, y / 16, 4, 4);
    -- value = (value + 1) / 2
    local value = snap.math.random()
    local r = value
    local g = value
    local b = value
    testImgdata:setPixel(x, y, r, g, b, 1.0)
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

local shader = snap.graphics.newShader("Graphics/Shaders/test.slang")
local computeshader = snap.graphics.newShader("Graphics/Shaders/test2.slang")
local value = 0.25

local texture
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
  scene:drawUiElement()

  Imgui.End()
  Imgui.ShowDemoWindow()

  snap.gui.endFrame()
  local imStartTime = snap.timer.getTime()
  snap.gui.draw()
  snap.graphics.setScissor();
  lastImDrawTime = lastImDrawTime + snap.timer.getTime() - imStartTime

  snap.graphics.setShader(computeshader)
  computeshader:send("PushConstants", "valueToAdd", value)
  computeshader:send("PushConstants", "textureSize", { 16, 16 })
  computeshader:send("outTexture", texture)
  snap.graphics.dispatch(1, 1, 1)
  value = -value

  snap.graphics.setShader(shader)
  shader:send("MainTexture", texture)
  for i = 1, 100 do
    snap.graphics.draw(mesh)
  end
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

  snap.graphics.setRenderTarget({ loadas = "clear", blendmode = blendmode })

  local dt = snap.timer.getTime() - t
  t = snap.timer.getTime()

  snap.gui.newFrame(dt)

  draw()

  snap.graphics.submitGraphics()

  doneChannel:push(true)
end

print("THREAD #1 EXITING")
