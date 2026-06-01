local ffi = require("ffi")

Editor = {
  focussedCamera = nil,
  dockId = nil,
}

local function drawDockingWidget()
  Imgui.DockSpaceOverViewport(0, Imgui.GetMainViewport(), Imgui.ImGuiDockNodeFlags_PassthruCentralNode)

  Editor.dockId = Imgui.GetID_Str("DockSpace")
end

function Editor.drawGUI()
  drawDockingWidget()
end
