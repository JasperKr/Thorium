Imgui = require("Editor.cimgui.init")

local lastDrawTime = 0
local lastImDrawTime = 0
local lastShownTime = 0
local lastShownImDrawTime = 0
local count = 0

local doneChannel, canStartChannel = ...

local t = Thorium.timer.getTime()
local testImgdata = Thorium.data.newImagedata(16, 16)
for y = 0, 15 do
  for x = 0, 15 do
    -- local value = Thorium.math.noiseWrapped(x / 16, y / 16, 4, 4);
    -- value = (value + 1) / 2
    local value = Thorium.math.random()
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

local shader = Thorium.graphics.newShader("Graphics/Shaders/test.slang")

local texture
print("Generated noise texture in " .. tostring(Thorium.timer.getTime() - t) .. " seconds")

local function draw()
  local startTime = Thorium.timer.getTime()
  Imgui.Begin("Test window")

  Imgui.Separator()
  Imgui.Text("FPS: " .. tostring(Thorium.timer.getFPS()))
  Imgui.Text("DT: " .. tostring(Thorium.timer.getDelta()))
  Imgui.Text("Last Draw Time (ms): " .. tostring(lastShownTime * 1000))
  Imgui.Text("Last ImGui Draw Time (ms): " .. tostring(lastShownImDrawTime * 1000))

  Imgui.End()
  Imgui.ShowDemoWindow()

  Thorium.gui.endFrame()
  local imStartTime = Thorium.timer.getTime()
  Thorium.gui.draw()
  Thorium.graphics.setScissor();
  lastImDrawTime = lastImDrawTime + Thorium.timer.getTime() - imStartTime

  Thorium.graphics.setShader(shader)
  shader:send("MainTexture", texture)
  for i = 1, 1000 do
    Thorium.graphics.draw(mesh)
  end
  Thorium.graphics.setShader()

  lastDrawTime = lastDrawTime + Thorium.timer.getTime() - startTime
  count = count + 1
  if (count >= 50) then
    lastShownTime = lastDrawTime / count
    lastShownImDrawTime = lastImDrawTime / count
    count = 0
    lastDrawTime = 0
    lastImDrawTime = 0
  end
end

local t = Thorium.timer.getTime()

while true do
  if not (canStartChannel:demand(1)) then
    break
  end

  Thorium.graphics.aquireGraphics("gui")
  if not texture then
    texture = Thorium.graphics.newTexture(testImgdata)

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

    mesh = Thorium.graphics.newMesh(vertexformat, vertices, "triangles")

    local indicesData = Thorium.data.newBytedata(#indices * 4)
    for j = 1, #indices do
      indicesData:setUInt32((j - 1) * 4, indices[j] - 1)
    end
    mesh:setIndices(indicesData)
  end

  ---@type Thorium.DetailedBlendMode
  local blendmode = {
    srccolor = "one",
    dstcolor = "oneminussrcalpha",
    colorop = "add",
    srcalpha = "one",
    dstalpha = "zero",
    alphaop = "add",
  }

  Thorium.graphics.setRenderTarget({ loadas = "clear", blendmode = blendmode })

  local dt = Thorium.timer.getTime() - t
  t = Thorium.timer.getTime()

  Thorium.gui.newFrame(dt)

  draw()

  Thorium.graphics.submitGraphics()

  doneChannel:push(true)
end

print("THREAD #1 EXITING")
