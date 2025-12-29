local i = 0

function Thorium.update(dt)
  -- if i > 100 then
  print("Dt: " .. dt, "FPS: " .. Thorium.timer.getFPS())
  -- i = 0
  -- end

  i = i + 1
end

function Thorium.mousemoved(x, y, dx, dy)
  -- print("Mouse moved", x, y, dx, dy)
end

function Thorium.mousepressed(x, y, button, istouch, presses)
  -- print("Mouse pressed", x, y, button, istouch, presses)
end

function Thorium.keypressed(key, scancode, isrepeat)
  -- print("Key pressed: " .. key, scancode, isrepeat)
end

function Thorium.keyreleased(key, scancode)
  -- print("Key released: " .. key, scancode)
end

function Thorium.textinput(text)
  -- print("Text input: " .. text)
end

function Thorium.wheelmoved(x, y)
  -- print("Wheel moved", x, y)
end

function Thorium.quit()
  print("Quitting the application.")
end

local sampler = 1
local rendertarget = 2
local ssbo = 4

local image = Thorium.graphics.newTexture("leek.png", { usage = sampler })
-- print(image:getFilter())
local target = Thorium.graphics.newTexture(612, 512, { usage = sampler + rendertarget })
local shader = Thorium.graphics.newShader("default2D")

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

local mesh = Thorium.graphics.newMesh(vertexFormat, vertices, "triangles", indices)
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
local texture = Thorium.graphics.newTexture(data, { usage = sampler })

function Thorium.draw()
  Thorium.graphics.setShader(shader)
  Thorium.graphics.setRenderTarget({ texture = target, loadas = { 1, 1, 0, 1 } })
  shader:send("test", i)
  shader:send("testColor", { 1, 0, 0 })
  shader:send("testMatrix", { 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1 })
  Thorium.graphics.draw(texture)
  Thorium.graphics.setRenderTarget({ loadas = { 0, 0, 0, 1 } })
  Thorium.graphics.draw(target)
end
