local ffi = require("ffi")
local bit = require("bit")
require("cdef")

local vertexformat
local common = {}

vertexformat = {
  { location = 0, format = "floatvec2" },
  { location = 1, format = "floatvec2" },
  { location = 2, format = "unorm8vec4" }
}

local textureObject, textureShader
local strings = {}

common.textures = setmetatable({}, { __mode = "v" })
common.callbacks = setmetatable({}, { __mode = "v" })

local cliboard_callback_get, cliboard_callback_set
local io, platform_io, atlas

local Alpha8_shader
ImguiWrapper = {}

function ImguiWrapper.Init(format)
  Alpha8_shader = Thorium.graphics.newShader("ImGuiA8", { debugname = "imgui alpha-8 shader" })
  DefaultShader = Thorium.graphics.newShader("ImGuiRGBA8", { debugname = "imgui default shader" })

  format = format or "rgba8"
  io = ffi.cast("ImGuiIO*", Thorium.gui.getIO())
  atlas = Thorium.gui.getFontAtlas()
  platform_io = ffi.cast("ImGuiPlatformIO*", Thorium.gui.getPlatformIO())
  ImguiWrapper.BuildFontAtlas(format)

  cliboard_callback_get = ffi.cast("const char* (*)(void*)", function(userdata)
    return Thorium.system.getClipboardText()
  end)
  cliboard_callback_set = ffi.cast("void (*)(void*, const char*)", function(userdata, text)
    Thorium.system.setClipboardText(ffi.string(text))
  end)

  platform_io.Platform_GetClipboardTextFn = cliboard_callback_get
  platform_io.Platform_SetClipboardTextFn = cliboard_callback_set

  -- local dpiscale = Thorium.window.getDPIScale()
  -- io.DisplayFramebufferScale.x, io.DisplayFramebufferScale.y = dpiscale, dpiscale

  Thorium.filesystem.createDirectory("/")
  strings.ini_filename = Thorium.filesystem.getSaveDirectory() .. "/imgui.ini"
  io.IniFilename = strings.ini_filename

  strings.impl_name = "cimgui-Thorium"
  io.BackendPlatformName = strings.impl_name
  io.BackendRendererName = strings.impl_name

  io.BackendFlags = bit.bor(ffi.C.ImGuiBackendFlags_HasMouseCursors, ffi.C.ImGuiBackendFlags_HasSetMousePos)
end

-- local altasBuildCount = 0
function ImguiWrapper.BuildFontAtlas(format)
  format = format or "rgba8"
  local imgdata

  if format == "rgba8" then
    imgdata = Thorium.gui.getFontAtlasAsRGBA32(atlas)
    textureShader = nil
  elseif format == "r8" then
    imgdata = Thorium.gui.getFontAtlasAsAlpha8(atlas)
    textureShader = Alpha8_shader
  else
    error([[Format should be either "rgba8" or "r8".]], 2)
  end

  textureObject = Thorium.graphics.newTexture(imgdata)
end

function ImguiWrapper.Update(dt)

end

local cursors = {
  [ffi.C.ImGuiMouseCursor_Arrow] = Thorium.mouse.getHardwareCursor("arrow"),
  [ffi.C.ImGuiMouseCursor_TextInput] = Thorium.mouse.getHardwareCursor("ibeam"),
  [ffi.C.ImGuiMouseCursor_ResizeAll] = Thorium.mouse.getHardwareCursor("move"),
  [ffi.C.ImGuiMouseCursor_ResizeNS] = Thorium.mouse.getHardwareCursor("resizens"),
  [ffi.C.ImGuiMouseCursor_ResizeEW] = Thorium.mouse.getHardwareCursor("resizewe"),
  [ffi.C.ImGuiMouseCursor_ResizeNESW] = Thorium.mouse.getHardwareCursor("resizenesw"),
  [ffi.C.ImGuiMouseCursor_ResizeNWSE] = Thorium.mouse.getHardwareCursor("resizenwse"),
  [ffi.C.ImGuiMouseCursor_Hand] = Thorium.mouse.getHardwareCursor("pointer"),
  [ffi.C.ImGuiMouseCursor_NotAllowed] = Thorium.mouse.getHardwareCursor("notallowed"),
}

function ImguiWrapper.MouseMoved(x, y)
  if Thorium.window.hasMouseFocus() then
    io:AddMousePosEvent(x, y)
  end
end

local mouse_buttons = { true, true, true }

function ImguiWrapper.MousePressed(button)
  if mouse_buttons[button] then
    io:AddMouseButtonEvent(button - 1, true)
  end
end

function ImguiWrapper.MouseReleased(button)
  if mouse_buttons[button] then
    io:AddMouseButtonEvent(button - 1, false)
  end
end

function ImguiWrapper.WheelMoved(x, y)
  io:AddMouseWheelEvent(x, y)
end

function ImguiWrapper.KeyPressed(key)
  local t = Thoriumkeymap[key]
  if type(t) == "table" then
    io:AddKeyEvent(t[1], true)
    io:AddKeyEvent(t[2], true)
  else
    io:AddKeyEvent(t or Thorium.gui.ImGuiKey_None, true)
  end
end

function ImguiWrapper.KeyReleased(key)
  local t = Thoriumkeymap[key]
  if type(t) == "table" then
    io:AddKeyEvent(t[1], false)
    io:AddKeyEvent(t[2], false)
  else
    io:AddKeyEvent(t or Thorium.gui.ImGuiKey_None, false)
  end
end

function ImguiWrapper.TextInput(text)
  Thorium.gui.ImGuiIO_AddInputCharactersUTF8(io, text)
end

function ImguiWrapper.Shutdown()
  Thorium.gui.igDestroyContext(nil)
  io = nil
  cliboard_callback_get:free()
  cliboard_callback_set:free()
  cliboard_callback_get, cliboard_callback_set = nil, nil
end

function ImguiWrapper.JoystickAdded(joystick)
  if not joystick:isGamepad() then return end
  io.BackendFlags = bit.bor(io.BackendFlags, Thorium.gui.ImGuiBackendFlags_HasGamepad)
end

function ImguiWrapper.JoystickRemoved()
  for _, joystick in ipairs(Thorium.joystick.getJoysticks()) do
    if joystick:isGamepad() then return end
  end
  io.BackendFlags = bit.band(io.BackendFlags, bit.bnot(Thorium.gui.ImGuiBackendFlags_HasGamepad))
end

local gamepad_map = {
  start = Thorium.gui.ImGuiKey_GamepadStart,
  back = Thorium.gui.ImGuiKey_GamepadBack,
  a = Thorium.gui.ImGuiKey_GamepadFaceDown,
  b = Thorium.gui.ImGuiKey_GamepadFaceRight,
  y = Thorium.gui.ImGuiKey_GamepadFaceUp,
  x = Thorium.gui.ImGuiKey_GamepadFaceLeft,
  dpleft = Thorium.gui.ImGuiKey_GamepadDpadLeft,
  dpright = Thorium.gui.ImGuiKey_GamepadDpadRight,
  dpup = Thorium.gui.ImGuiKey_GamepadDpadUp,
  dpdown = Thorium.gui.ImGuiKey_GamepadDpadDown,
  leftshoulder = Thorium.gui.ImGuiKey_GamepadL1,
  rightshoulder = Thorium.gui.ImGuiKey_GamepadR1,
  leftstick = Thorium.gui.ImGuiKey_GamepadL3,
  rightstick = Thorium.gui.ImGuiKey_GamepadR3,
  --analog
  triggerleft = Thorium.gui.ImGuiKey_GamepadL2,
  triggerright = Thorium.gui.ImGuiKey_GamepadR2,
  leftx = { Thorium.gui.ImGuiKey_GamepadLStickLeft, Thorium.gui.ImGuiKey_GamepadLStickRight },
  lefty = { Thorium.gui.ImGuiKey_GamepadLStickUp, Thorium.gui.ImGuiKey_GamepadLStickDown },
  rightx = { Thorium.gui.ImGuiKey_GamepadRStickLeft, Thorium.gui.ImGuiKey_GamepadRStickRight },
  righty = { Thorium.gui.ImGuiKey_GamepadRStickUp, Thorium.gui.ImGuiKey_GamepadRStickDown },
}

function ImguiWrapper.GamepadPressed(button)
  io:AddKeyEvent(gamepad_map[button] or Thorium.gui.ImGuiKey_None, true)
end

function ImguiWrapper.GamepadReleased(button)
  io:AddKeyEvent(gamepad_map[button] or Thorium.gui.ImGuiKey_None, false)
end

function ImguiWrapper.GamepadAxis(axis, value, threshold)
  threshold = threshold or 0
  local imguikey = gamepad_map[axis]
  if type(imguikey) == "table" then
    if value > threshold then
      io:AddKeyAnalogEvent(imguikey[2], true, value)
      io:AddKeyAnalogEvent(imguikey[1], false, 0)
    elseif value < -threshold then
      io:AddKeyAnalogEvent(imguikey[1], true, -value)
      io:AddKeyAnalogEvent(imguikey[2], false, 0)
    else
      io:AddKeyAnalogEvent(imguikey[1], false, 0)
      io:AddKeyAnalogEvent(imguikey[2], false, 0)
    end
  elseif imguikey then
    io:AddKeyAnalogEvent(imguikey, value ~= 0, value)
  end
end

-- input capture

function ImguiWrapper.GetWantCaptureMouse()
  return io.WantCaptureMouse
end

function ImguiWrapper.GetWantCaptureKeyboard()
  return io.WantCaptureKeyboard
end

function ImguiWrapper.GetWantTextInput()
  return io.WantTextInput
end
