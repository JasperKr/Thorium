Imgui = require("Editor.cimgui.init")

local lastDrawTime = 0
local lastImDrawTime = 0
local lastShownTime = 0
local lastShownImDrawTime = 0
local count = 0

local doneChannel, canStartChannel = ...

local testImgdata = Thorium.data.newImagedata(256, 256)
for y = 0, 255 do
  for x = 0, 255 do
    local r = x / 256
    local g = x / 256
    local b = x / 256
    testImgdata:setPixel(x, y, r, g, b, 1.0)
  end
end
local texture

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
  lastDrawTime = lastDrawTime + Thorium.timer.getTime() - startTime
  count = count + 1
  if (count >= 20) then
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
  Thorium.graphics.setShader()
  Thorium.graphics.draw(texture)
  Thorium.graphics.submitGraphics()

  doneChannel:push(true)
end

print("THREAD #1 EXITING")
