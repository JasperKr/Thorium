local i = 0
Thorium.threaderror = error

print("Starting thread 1")

local thread = Thorium.thread.newThread("src/Engine/thread.lua")
local threadDoneChannel = Thorium.thread.newChannel()
local startThreadChannel = Thorium.thread.newChannel()
thread:start(threadDoneChannel, startThreadChannel)

print("Starting thread 2")

local secondThread = Thorium.thread.newThread("src/Engine/thread2.lua")
local secondThreadDoneChannel = Thorium.thread.newChannel()
local stopChannel = Thorium.thread.newChannel()
secondThread:start(secondThreadDoneChannel, stopChannel)

function Thorium.update(dt)
  i = i + 1

  -- Thorium.gui.newFrame(dt)
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
  startThreadChannel:push(false)
  startThreadChannel:push(false)

  -- startSecondThreadChannel:push(false)
  stopChannel:push(true)

  thread:wait()
  secondThread:wait()

  print("Quitting the application.")
  return 1
end

Thorium.keyboard.setEnableTextInput(true)

local firstFrame = true

function Thorium.draw()
  if (firstFrame) then
    print("First frame draw")
    firstFrame = false
    secondThreadDoneChannel:demand(1)
    Thorium.graphics.useCommands("load")
  end

  startThreadChannel:push(true)
  threadDoneChannel:demand(1)

  Thorium.graphics.useCommands("gui")

  local idx = secondThreadDoneChannel:pop()
  local drawn = {}

  while idx do
    drawn[idx] = true
    idx = secondThreadDoneChannel:pop()
  end

  for i, _ in pairs(drawn) do
    Thorium.graphics.useCommands("square-" .. tostring(i))
  end
end
