local ffi = require("ffi")

Editor = {
  focussedCamera = nil,
}

local function drawDockingWidget()
  if Imgui.Begin("DockSpace", nil, Imgui.ImGuiWindowFlags_AlwaysUseWindowPadding) then
    Imgui.SetWindowPos_Vec2(SnapEngine.math.tempImVec2(0, 0))
    Imgui.SetWindowSize_Vec2(SnapEngine.math.tempImVec2(snap.graphics.getDimensions()))
    Imgui.SetCursorScreenPos(SnapEngine.math.tempImVec2(0, 0))
    Imgui.DockSpace(1, SnapEngine.math.tempImVec2(snap.graphics.getDimensions()),
      bit.bor(Imgui.ImGuiDockNodeFlags_NoDockingOverCentralNode, Imgui.ImGuiDockNodeFlags_AutoHideTabBar))
  end
  Imgui.End()
end

local function drawSceneWindow(camera)
  Imgui.Begin("Scene")
  Imgui.Image(rendertarget, ffi.new("ImVec2", cameraWidth, cameraHeight))
  Imgui.End()
end

function Editor.drawGUI()
  drawDockingWidget()
end
