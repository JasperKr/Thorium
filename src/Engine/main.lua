Imgui = require("cimgui.init")
local bit = require("bit")

local i = 0

-- local thread = Thorium.thread.newThread([[
-- print('varargs: ', ...)
-- local channel = ...
-- print(channel:demand())
-- ]])
-- local channel = Thorium.thread.newChannel()
-- thread:start(channel)
-- channel:push("Hello from main thread!")

function Thorium.update(dt)
  i = i + 1

  Thorium.gui.newFrame(dt)
end

function Thorium.mousemoved(x, y, dx, dy)
  -- print("Mouse moved", x, y, dx, dy)
  Thorium.gui.mouseMoved(x, y)
end

function Thorium.mousepressed(x, y, button, istouch, presses)
  -- print("Mouse pressed", x, y, button, istouch, presses)
  Thorium.gui.mousePressed(x, y, button)
end

function Thorium.mousereleased(x, y, button, istouch, presses)
  -- print("Mouse released", x, y, button, istouch, presses)
  Thorium.gui.mouseReleased(x, y, button)
end

function Thorium.keypressed(key, scancode, isrepeat)
  -- print("Key pressed: " .. key, scancode, isrepeat)

  -- ctrl + alt + c = capture
  if (Thorium.keyboard.isDown("lctrl") and Thorium.keyboard.isDown("lalt") and key == "c") then
    Thorium.filesystem.write("capture", "")
    print(Thorium.filesystem.getSaveDirectory() .. "/capture created")
  end

  Thorium.gui.keyPressed(key)
end

function Thorium.keyreleased(key, scancode)
  -- print("Key released: " .. key, scancode)
  Thorium.gui.keyReleased(key)
end

function Thorium.textinput(text)
  -- print("Text input: " .. text)
  Thorium.gui.textInput(text)
end

function Thorium.wheelmoved(x, y)
  -- print("Wheel moved", x, y)
  Thorium.gui.mouseWheelMoved(x, y)
end

function Thorium.quit()
  print("Quitting the application.")
end

local image = Thorium.graphics.newTexture("test.png", { sampler = true })
-- print(image:getFilter())
local target = Thorium.graphics.newTexture(612, 512, { sampler = true, rendertarget = true })
local shader = Thorium.graphics.newShader("default2D")
local compute = Thorium.graphics.newShader("testCS");
local inputBuffer = Thorium.graphics.newBuffer("uint32", 1024, { shaderstorage = true })
local outputBuffer = Thorium.graphics.newBuffer("uint32", 1024, { shaderstorage = true })

inputBuffer:clear()
inputBuffer:setData({ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 })

print("Setting input buffer")
compute:send("Input", inputBuffer)

print("Setting output buffer")
compute:send("Output", outputBuffer)

print(compute:getThreadgroupSize())
print(compute:getWaveSize())

compute:send("Push", "count", 10)
compute:send("Push", "increment", 1)

Thorium.graphics.setShader(compute)
print("Dispatching compute shader")
Thorium.graphics.dispatch(1, 1, 1)

local vertexFormat = {
  { name = "position", format = "floatvec2",  location = 0 },
  { name = "texcoord", format = "floatvec2",  location = 1 },
  { name = "color",    format = "unorm8vec4", location = 2 },
}

local vertices = {
  { -0.5, -0.5, 0, 0, 1, 0, 0, 1 },
  { 0.5,  -0.5, 1, 0, 0, 1, 0, 1 },
  { 0.5,  0.5,  1, 1, 0, 0, 1, 1 },
  { -0.5, 0.5,  0, 1, 1, 1, 1, 1 },
}

local indices = {
  1, 2, 3,
  3, 4, 1,
}

local mesh = Thorium.graphics.newMesh(vertexFormat, vertices, "triangles")
local data = Thorium.data.newImagedata(512, 512, "rgba16f")
for y = 0, 512 - 1 do
  for x = 0, 512 - 1 do
    -- size of half = 2, 4 components
    local offset = (y * 512 + x) * 2 * 4
    local grayscale = (x / 512) * (y / 512)
    data:setHalf(offset, grayscale)
    data:setHalf(offset + 2, grayscale)
    data:setHalf(offset + 4, grayscale)
    data:setHalf(offset + 6, 1.0)
  end
end
local texture = Thorium.graphics.newTexture(data, { sampler = true })

function hslToRgb(h, s, l)
  local r, g, b;

  if s == 0 then
    r, g, b = l, l, l; -- achromatic
  else
    local function hue2rgb(p, q, t)
      if t < 0 then t = t + 1 end
      if t > 1 then t = t - 1 end
      if t < 1 / 6 then return p + (q - p) * 6 * t end
      if t < 1 / 2 then return q end
      if t < 2 / 3 then return p + (q - p) * (2 / 3 - t) * 6 end
      return p;
    end

    local q = l < 0.5 and l * (1 + s) or l + s - l * s;
    local p = 2 * l - q;
    r = hue2rgb(p, q, h + 1 / 3);
    g = hue2rgb(p, q, h);
    b = hue2rgb(p, q, h - 1 / 3);
  end

  if not a then a = 1 end
  return r, g, b, a
end

local ffi = require("ffi")

Thorium.keyboard.setEnableTextInput(true)

local lastDrawTime = 0
local lastImDrawTime = 0
local lastShownTime = 0
local lastShownImDrawTime = 0
local count = 0

function Thorium.draw()
  local startTime = Thorium.timer.getTime()
  -- Thorium.graphics.setShader(shader)
  -- Thorium.graphics.setRenderTarget({ texture = target, loadas = { hslToRgb((Thorium.timer.getTime() * 0.5) % 1, 0.5, 0.5) } })
  -- Thorium.graphics.draw(texture)
  -- Thorium.graphics.setRenderTarget({ loadas = { 0, 0, 0, 1 } })
  -- Thorium.graphics.draw(target)

  Imgui.Begin("Test window")

  -- Imgui.Image(image, Imgui.ImVec2_Float(256, 256))
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
