print("AAAAAAAAAAAA")

Imgui = require("cimgui.init")

local lastDrawTime = 0
local lastImDrawTime = 0
local lastShownTime = 0
local lastShownImDrawTime = 0
local count = 0

local doneChannel, canStartChannel = ...

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

while true do
  if not (canStartChannel:demand(5)) then
    break
  end

  Thorium.graphics.demandRenderingPermission()
  Thorium.graphics.aquireGraphics("gui")

  Thorium.gui.newFrame(1 / 60)

  draw()
  doneChannel:push(true)

  Thorium.graphics.submitGraphics()
end

print("THREAD #1 EXITING")
