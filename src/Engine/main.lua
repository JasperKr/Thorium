local i = 0

function Thorium.update(dt)
  if i > 100 then
    print("Dt: " .. dt, "FPS: " .. Thorium.timer.getFPS())
    i = 0
  end

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
local target = Thorium.graphics.newTexture(1920, 1080, { usage = sampler + rendertarget })
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

function Thorium.draw()
  Thorium.graphics.setShader(shader)
  Thorium.graphics.setRenderTarget(target)
  shader:send("test", i)
  shader:send("testColor", { 1, 0, 0 })
  shader:send("testMatrix", { 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1 })
  Thorium.graphics.drawInstanced(mesh, 1000)
  Thorium.graphics.setRenderTarget({ loadas = { 1, 0, 0, 1 } })
  Thorium.graphics.draw(target)
end
