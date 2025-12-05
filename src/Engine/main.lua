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
  print("Key pressed: " .. key,scancode, isrepeat)
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

local target = Thorium.graphics.newTexture(32, 32, {usage = sampler + rendertarget})
local image = Thorium.graphics.newTexture(32, 32, {usage = sampler})
Thorium.graphics.setRenderTarget(target)
Thorium.graphics.draw(image)
Thorium.graphics.setRenderTarget()