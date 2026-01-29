local i = 0
Thorium.threaderror = error

print("Starting thread 1")

local thread = Thorium.thread.newThread("src/Engine/thread.lua", "Render thread 1")
local threadDoneChannel = Thorium.thread.newChannel()
local startThreadChannel = Thorium.thread.newChannel()
thread:start(threadDoneChannel, startThreadChannel)

function Thorium.update(dt)
  i = i + 1
end

function Thorium.mousemoved(x, y, dx, dy)
  Thorium.gui.mouseMoved(x, y)
end

function Thorium.mousepressed(x, y, button, istouch, presses)
  Thorium.gui.mousePressed(x, y, button)
end

function Thorium.mousereleased(x, y, button, istouch, presses)
  Thorium.gui.mouseReleased(x, y, button)
end

function Thorium.keypressed(key, scancode, isrepeat)
  -- ctrl + alt + c = capture
  if (Thorium.keyboard.isDown("lctrl") and Thorium.keyboard.isDown("lalt") and key == "c") then
    Thorium.filesystem.write("capture", "")
    print(Thorium.filesystem.getSaveDirectory() .. "/capture created")
  end

  Thorium.gui.keyPressed(key)
end

function Thorium.keyreleased(key, scancode)
  Thorium.gui.keyReleased(key)
end

function Thorium.textinput(text)
  Thorium.gui.textInput(text)
end

function Thorium.wheelmoved(x, y)
  Thorium.gui.mouseWheelMoved(x, y)
end

function Thorium.quit()
  startThreadChannel:push(false)
  startThreadChannel:push(false)

  thread:wait()

  print("Quitting the application.")
  return 1
end

Thorium.keyboard.setEnableTextInput(true)

function Thorium.draw()
  startThreadChannel:push(true)
  threadDoneChannel:demand(1)

  Thorium.graphics.useCommands("gui")
end
