local i = 0
snap.threaderror = error

print("Starting thread 1")
require("Scripting.Modules.vec")
require("Scripting.Modules.quaternions")
require("Scripting.Modules.matrices")
require("Scripting.Modules.math")
require("Scripting.Graphics.camera")
require("Scripting.Graphics.helpers")

local thread = snap.thread.newThread("src/Engine/Scripting/thread.lua", "Render thread 1")
local threadDoneChannel = snap.thread.newChannel()
local startThreadChannel = snap.thread.newChannel()
local scene = snap.scene.newScene()

local material = snap.renderer.newMaterial()
print("Name", material:getName())
print("AlphaCutoff", material:getAlphaCutoff())
print("Shader", material:getShader())

print("Preview", material:getPreview())
print("AlbedoTexture", material:getAlbedoTexture())
print("NormalTexture", material:getNormalTexture())
print("MetallicRoughnessTexture",
  material:getMetallicRoughnessTexture())
print("AmbientOcclusionTexture",
  material:getAmbientOcclusionTexture())
print("ReflectanceTexture", material:getReflectanceTexture())
print("EmissiveTexture", material:getEmissiveTexture())

print("AlbedoFactor", material:getAlbedoFactor())
print("RoughnessFactor", material:getRoughnessFactor())
print("MetallicFactor", material:getMetallicFactor())
print("ReflectanceFactor", material:getReflectanceFactor())
print("EmissiveFactor", material:getEmissiveFactor())

print("AlphaMode", material:getAlphaMode())
print("CullMode", material:getCullMode())

thread:start(threadDoneChannel, startThreadChannel, scene)

local camera = snap.graphics.newCamera("main camera", vec3(0, 0, 0), vec3(0, 0, 0),
  vec2(snap.graphics.getDimensions()))
print(scene:getName())

function snap.update(dt)
  i = i + 1
end

function snap.mousemoved(x, y, dx, dy)
  snap.gui.mouseMoved(x, y)
end

function snap.mousepressed(x, y, button, istouch, presses)
  snap.gui.mousePressed(x, y, button)
end

function snap.mousereleased(x, y, button, istouch, presses)
  snap.gui.mouseReleased(x, y, button)
end

function snap.keypressed(key, scancode, isrepeat)
  -- ctrl + alt + c = capture
  if (snap.keyboard.isDown("lctrl") and snap.keyboard.isDown("lalt") and key == "c") then
    snap.filesystem.write("capture", "")
    print(snap.filesystem.getSaveDirectory() .. "/capture created")
  end

  snap.gui.keyPressed(key)
end

function snap.keyreleased(key, scancode)
  snap.gui.keyReleased(key)
end

function snap.textinput(text)
  snap.gui.textInput(text)
end

function snap.wheelmoved(x, y)
  snap.gui.mouseWheelMoved(x, y)
end

function snap.quit()
  startThreadChannel:push(false)
  startThreadChannel:push(false)

  thread:wait()

  print("Quitting the application.")
  return 1
end

snap.keyboard.setEnableTextInput(true)

function snap.draw()
  startThreadChannel:push(true)
  threadDoneChannel:demand(1)

  snap.graphics.useCommands("gui")
end
