print("AAAAAAAAAAAAa")
local lastDrawTime = 0
local lastImDrawTime = 0
local lastShownTime = 0
local lastShownImDrawTime = 0
local count = 0

print("Rendering thread started.")

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

  print("Ending frame...")

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
  print("Thread waiting for rendering permission...")
  Thorium.graphics.demandRenderingPermission()
  print("Rendering thread granted permission.")
  Thorium.graphics.aquireGraphics("gui")
  print("Rendering thread acquired graphics context.")

  Thorium.gui.newFrame(1 / 60)
  print("Rendering thread drawing...")

  draw()
  print("Rendering thread submitting graphics...")

  Thorium.graphics.submitGraphics()
end
