local ffi = require("ffi")
local bit = require("bit")

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
local io, platform_io

local Alpha8_shader

function L.Init(format)
  Alpha8_shader = Thorium.graphics.newShader("ImGuiA8", { debugname = "imgui alpha-8 shader" })
  DefaultShader = Thorium.graphics.newShader("ImGuiRGBA8", { debugname = "imgui default shader" })

  format = format or "RGBA32"
  C.igCreateContext(nil)
  io = C.igGetIO()
  platform_io = C.igGetPlatformIO()
  L.BuildFontAtlas(format)

  cliboard_callback_get = ffi.cast("const char* (*)(void*)", function(userdata)
    return love.system.getClipboardText()
  end)
  cliboard_callback_set = ffi.cast("void (*)(void*, const char*)", function(userdata, text)
    love.system.setClipboardText(ffi.string(text))
  end)

  platform_io.Platform_GetClipboardTextFn = cliboard_callback_get
  platform_io.Platform_SetClipboardTextFn = cliboard_callback_set

  local dpiscale = love.window.getDPIScale()
  io.DisplayFramebufferScale.x, io.DisplayFramebufferScale.y = dpiscale, dpiscale

  love.filesystem.createDirectory("/")
  strings.ini_filename = love.filesystem.getSaveDirectory() .. "/imgui.ini"
  io.IniFilename = strings.ini_filename

  strings.impl_name = "cimgui-love"
  io.BackendPlatformName = strings.impl_name
  io.BackendRendererName = strings.impl_name

  io.BackendFlags = bit.bor(C.ImGuiBackendFlags_HasMouseCursors, C.ImGuiBackendFlags_HasSetMousePos)
end

local custom_shader

function L.SetShader(shader)
  custom_shader = shader
end

local altasBuildCount = 0
function L.BuildFontAtlas(format)
  format = format or "RGBA32"
  local pixels, width, height = ffi.new("unsigned char*[1]"), ffi.new("int[1]"), ffi.new("int[1]")
  local imgdata

  if format == "RGBA32" then
    C.ImFontAtlas_GetTexDataAsRGBA32(io.Fonts, pixels, width, height, nil)
    imgdata = love.image.newImageData(width[0], height[0], "rgba8", ffi.string(pixels[0], width[0] * height[0] * 4))
    textureShader = nil
  elseif format == "Alpha8" then
    C.ImFontAtlas_GetTexDataAsAlpha8(io.Fonts, pixels, width, height, nil)
    imgdata = love.image.newImageData(width[0], height[0], "r8", ffi.string(pixels[0], width[0] * height[0]))
    textureShader = Alpha8_shader
  else
    error([[Format should be either "RGBA32" or "Alpha8".]], 2)
  end

  altasBuildCount = altasBuildCount + 1
  local currentBuildCount = altasBuildCount
  if textureObject then
    table.insert(Rhodium.internal.garbage, textureObject)
  end
  textureObject = love.graphics.newTexture(imgdata)
  Rhodium.internal.compressor.compressAsync(imgdata, true, "veryslow",
    format == "Alpha8" and "BC5" or "BC7", function(compressedData)
      if currentBuildCount ~= altasBuildCount then
        return
      end
      table.insert(Rhodium.internal.garbage, textureObject)
      textureObject = love.graphics.newTexture(compressedData)
    end)
end

function L.Update(dt)
  io.DisplaySize.x, io.DisplaySize.y = love.graphics.getDimensions()
  io.DeltaTime = dt

  if io.WantSetMousePos then
    love.mouse.setPosition(io.MousePos.x, io.MousePos.y)
  end
end

local function love_texture_test(t)
  return t:typeOf("Texture")
end

local cursors = {
  [C.ImGuiMouseCursor_Arrow] = love.mouse.getSystemCursor("arrow"),
  [C.ImGuiMouseCursor_TextInput] = love.mouse.getSystemCursor("ibeam"),
  [C.ImGuiMouseCursor_ResizeAll] = love.mouse.getSystemCursor("sizeall"),
  [C.ImGuiMouseCursor_ResizeNS] = love.mouse.getSystemCursor("sizens"),
  [C.ImGuiMouseCursor_ResizeEW] = love.mouse.getSystemCursor("sizewe"),
  [C.ImGuiMouseCursor_ResizeNESW] = love.mouse.getSystemCursor("sizenesw"),
  [C.ImGuiMouseCursor_ResizeNWSE] = love.mouse.getSystemCursor("sizenwse"),
  [C.ImGuiMouseCursor_Hand] = love.mouse.getSystemCursor("hand"),
  [C.ImGuiMouseCursor_NotAllowed] = love.mouse.getSystemCursor("no"),
}

local cmdBuffers = {}
local ptrSize = ffi.sizeof("void*")

function L.RenderDrawLists()
  -- Avoid rendering when minimized
  if io.DisplaySize.x == 0 or io.DisplaySize.y == 0 or not love.window.isVisible() then return end
  Rhodium.profiler.push("Cursor Set")

  love.graphics.push("all")

  love.graphics.setBlendMode("alpha", "alphamultiply")

  common.RunShortcuts()
  local data = C.igGetDrawData()

  -- change mouse cursor
  if bit.band(io.ConfigFlags, C.ImGuiConfigFlags_NoMouseCursorChange) ~= C.ImGuiConfigFlags_NoMouseCursorChange then
    local cursor = cursors[C.igGetMouseCursor()]
    if io.MouseDrawCursor or not cursor then
      love.mouse.setVisible(false) -- Hide OS mouse cursor if ImGui is drawing it
    else
      love.mouse.setVisible(true)
      love.mouse.setCursor(cursor)
    end
  end

  Rhodium.profiler.pop("Cursor Set")
  Rhodium.profiler.push("perpare mesh")
  for i = 0, data.CmdListsCount - 1 do
    local cmd_list = data.CmdLists.Data[i]

    local cmdBuffer = cmdBuffers[i + 1]

    if not cmdBuffer then
      cmdBuffer = {
        max_vertexcount = -math.huge,
        max_indexcount = -math.huge,
      }

      cmdBuffers[i + 1] = cmdBuffer
    end

    cmdBuffer.cmd_list = cmd_list

    local vertexcount = cmd_list.VtxBuffer.Size
    local data_size = vertexcount * ffi.sizeof("ImDrawVert")
    if vertexcount > cmdBuffer.max_vertexcount then
      cmdBuffer.max_vertexcount = vertexcount
      if cmdBuffer.mesh then cmdBuffer.mesh:release() end
      if cmdBuffer.meshdata then cmdBuffer.meshdata:release() end
      cmdBuffer.meshdata = love.data.newByteData(math.max(data_size, ffi.sizeof("ImDrawVert")))
      cmdBuffer.meshdata_ptr = cmdBuffer.meshdata:getFFIPointer()
      ffi.copy(cmdBuffer.meshdata_ptr, cmd_list.VtxBuffer.Data, data_size)
      cmdBuffer.mesh = love.graphics.newMesh(vertexformat, cmdBuffer.meshdata, "triangles", "stream")
    else
      ffi.copy(cmdBuffer.meshdata_ptr, cmd_list.VtxBuffer.Data, data_size)
      cmdBuffer.mesh:setVertices(cmdBuffer.meshdata)
    end

    local indices_data_size = cmd_list.IdxBuffer.Size * ffi.sizeof("ImDrawIdx")

    if cmd_list.IdxBuffer.Size > cmdBuffer.max_indexcount then
      cmdBuffer.max_indexcount = cmd_list.IdxBuffer.Size
      if cmdBuffer.idx_buffer then cmdBuffer.idx_buffer:release() end
      cmdBuffer.idx_buffer = love.data.newByteData(math.max(indices_data_size, ffi.sizeof("ImDrawIdx")))
      cmdBuffer.idx_buffer_ptr = cmdBuffer.idx_buffer:getFFIPointer()
    end

    ffi.copy(cmdBuffer.idx_buffer_ptr, cmd_list.IdxBuffer.Data, indices_data_size)

    cmdBuffer.mesh:setVertexMap(cmdBuffer.idx_buffer, "uint16")
  end
  Rhodium.profiler.pop("perpare mesh")

  Rhodium.profiler.push("draw")
  for i = 0, data.CmdListsCount - 1 do
    local cmdBuffer = cmdBuffers[i + 1]

    local cmd_list = cmdBuffer.cmd_list
    local mesh = cmdBuffer.mesh

    for k = 0, cmd_list.CmdBuffer.Size - 1 do
      local cmd = cmd_list.CmdBuffer.Data[k]
      if cmd.UserCallback ~= nil then
        local callback = common.callbacks[ffi.string(ffi.cast("void*", cmd.UserCallback), ptrSize)] or
            cmd.UserCallback
        callback(cmd_list, cmd)
      elseif cmd.ElemCount > 0 then
        local clipX, clipY = cmd.ClipRect.x, cmd.ClipRect.y
        local clipW = cmd.ClipRect.z - clipX
        local clipH = cmd.ClipRect.w - clipY

        local texture_id = C.ImDrawCmd_GetTexID(cmd)
        -- local texture_id = cmd.TextureId
        if texture_id ~= 0 then
          local obj = common.textures[tostring(texture_id)]
          love.graphics.setShader(DefaultShader)
          DefaultShader:send("Texture", obj)
        else
          local shader = (custom_shader or textureShader) or DefaultShader
          love.graphics.setShader(shader)
          shader:send("Texture", textureObject)
        end

        love.graphics.setScissor(clipX, clipY, clipW, clipH)
        mesh:setDrawRange(cmd.IdxOffset + 1, cmd.ElemCount)

        love.graphics.draw(mesh)
      end
    end
  end
  love.graphics.pop()

  Rhodium.profiler.pop("draw")
end

function L.MouseMoved(x, y)
  if love.window.hasMouseFocus() then
    io:AddMousePosEvent(x, y)
  end
end

local mouse_buttons = { true, true, true }

function L.MousePressed(button)
  if mouse_buttons[button] then
    io:AddMouseButtonEvent(button - 1, true)
  end
end

function L.MouseReleased(button)
  if mouse_buttons[button] then
    io:AddMouseButtonEvent(button - 1, false)
  end
end

function L.WheelMoved(x, y)
  io:AddMouseWheelEvent(x, y)
end

function L.KeyPressed(key)
  local t = lovekeymap[key]
  if type(t) == "table" then
    io:AddKeyEvent(t[1], true)
    io:AddKeyEvent(t[2], true)
  else
    io:AddKeyEvent(t or C.ImGuiKey_None, true)
  end
end

function L.KeyReleased(key)
  local t = lovekeymap[key]
  if type(t) == "table" then
    io:AddKeyEvent(t[1], false)
    io:AddKeyEvent(t[2], false)
  else
    io:AddKeyEvent(t or C.ImGuiKey_None, false)
  end
end

function L.TextInput(text)
  C.ImGuiIO_AddInputCharactersUTF8(io, text)
end

function L.Shutdown()
  C.igDestroyContext(nil)
  io = nil
  cliboard_callback_get:free()
  cliboard_callback_set:free()
  cliboard_callback_get, cliboard_callback_set = nil, nil
end

function L.JoystickAdded(joystick)
  if not joystick:isGamepad() then return end
  io.BackendFlags = bit.bor(io.BackendFlags, C.ImGuiBackendFlags_HasGamepad)
end

function L.JoystickRemoved()
  for _, joystick in ipairs(love.joystick.getJoysticks()) do
    if joystick:isGamepad() then return end
  end
  io.BackendFlags = bit.band(io.BackendFlags, bit.bnot(C.ImGuiBackendFlags_HasGamepad))
end

local gamepad_map = {
  start = C.ImGuiKey_GamepadStart,
  back = C.ImGuiKey_GamepadBack,
  a = C.ImGuiKey_GamepadFaceDown,
  b = C.ImGuiKey_GamepadFaceRight,
  y = C.ImGuiKey_GamepadFaceUp,
  x = C.ImGuiKey_GamepadFaceLeft,
  dpleft = C.ImGuiKey_GamepadDpadLeft,
  dpright = C.ImGuiKey_GamepadDpadRight,
  dpup = C.ImGuiKey_GamepadDpadUp,
  dpdown = C.ImGuiKey_GamepadDpadDown,
  leftshoulder = C.ImGuiKey_GamepadL1,
  rightshoulder = C.ImGuiKey_GamepadR1,
  leftstick = C.ImGuiKey_GamepadL3,
  rightstick = C.ImGuiKey_GamepadR3,
  --analog
  triggerleft = C.ImGuiKey_GamepadL2,
  triggerright = C.ImGuiKey_GamepadR2,
  leftx = { C.ImGuiKey_GamepadLStickLeft, C.ImGuiKey_GamepadLStickRight },
  lefty = { C.ImGuiKey_GamepadLStickUp, C.ImGuiKey_GamepadLStickDown },
  rightx = { C.ImGuiKey_GamepadRStickLeft, C.ImGuiKey_GamepadRStickRight },
  righty = { C.ImGuiKey_GamepadRStickUp, C.ImGuiKey_GamepadRStickDown },
}

function L.GamepadPressed(button)
  io:AddKeyEvent(gamepad_map[button] or C.ImGuiKey_None, true)
end

function L.GamepadReleased(button)
  io:AddKeyEvent(gamepad_map[button] or C.ImGuiKey_None, false)
end

function L.GamepadAxis(axis, value, threshold)
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

function L.GetWantCaptureMouse()
  return io.WantCaptureMouse
end

function L.GetWantCaptureKeyboard()
  return io.WantCaptureKeyboard
end

function L.GetWantTextInput()
  return io.WantTextInput
end

-- flag helpers
local flags = {}

for name in pairs(M) do
  name = name:match("^(%w+Flags)_")
  if name and not flags[name] then
    flags[name] = true
  end
end

for name in pairs(flags) do
  local shortname = name:gsub("^ImGui", "")
  shortname = shortname:gsub("^Im", "")
  L[shortname] = function(...)
    local t = {}
    for _, flag in ipairs({ ... }) do
      t[#t + 1] = M[name .. "_" .. flag]
    end
    return bit.bor(unpack(t))
  end
end

-- revert to old implementation names, i.e., imgui.RenderDrawLists instead of imgui.love.RenderDrawLists, etc.
local old_names = {}

for k, v in pairs(L) do
  old_names[k] = v
end

function L.RevertToOldNames()
  for k, v in pairs(old_names) do
    M[k] = v
  end
end
