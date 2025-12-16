local i = 0

function Thorium.update(dt)
  if i > 100 then
    print("Dt: " .. dt, "FPS: " .. Thorium.timer.getFPS())
    i = 0
  end

  i = i + 1
end

function Thorium.mousemoved(x, y, dx, dy)
  print("Mouse moved", x, y, dx, dy)
end

function Thorium.mousepressed(x, y, button, istouch, presses)
  print("Mouse pressed", x, y, button, istouch, presses)
end

function Thorium.keypressed(key, scancode, isrepeat)
  print("Key pressed: " .. key, scancode, isrepeat)
end

function Thorium.keyreleased(key, scancode)
  print("Key released: " .. key, scancode)
end

function Thorium.textinput(text)
  print("Text input: " .. text)
end

function Thorium.wheelmoved(x, y)
  print("Wheel moved", x, y)
end

function Thorium.quit()
  print("Quitting the application.")
end

local sampler = 1
local rendertarget = 2
local ssbo = 4

local image = Thorium.graphics.newTexture("leek.png", { usage = sampler })
local target = Thorium.graphics.newTexture(image:getWidth(), image:getHeight(), { usage = sampler + rendertarget })
function Thorium.draw()
  Thorium.graphics.setRenderTarget(target)
  Thorium.graphics.draw(image)
  Thorium.graphics.setRenderTarget()
  Thorium.graphics.draw(target)
end
