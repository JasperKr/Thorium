local i = 0
snap.threaderror = error

print("Starting thread 1")
require("Modules.vec")
require("Modules.quaternions")
require("Modules.matrices")
require("Modules.math")
require("Graphics.camera")
require("Modules.helpers")

local thread = snap.thread.newThread("src/Scripting/thread.lua", "Render thread 1")
local commandsChannel = snap.thread.newChannel()
local startThreadChannel = snap.thread.newChannel()
local events = snap.thread.newChannel()
local scene = snap.scene.newScene("Main")
print(scene:getName())
local commandBuffers = {}

snap.graphics.aquireGraphics("load")

--[[
    float4 sv_position : SV_Position;
    float4x2 TexCoords : Texture_Coordinates;
    half4 normal : Normal;
    half4 tangent : Tangent;
    float4 color       : Color;
]]

local vertexformat = {
  { name = "position",      format = "floatvec3",  location = 0 },
  { name = "textureCoords", format = "floatvec2",  location = 1 },
  { name = "normal",        format = "uint32",     location = 2 },
  { name = "tangent",       format = "uint32",     location = 3 },
  { name = "color",         format = "unorm8vec4", location = 4 },
}

-- local indices = { 1, 2, 3, 1, 3, 4 }
-- local vertices = {
--   --x,   y,   z, w, t0,t1,n, t, r, g, b, a
--   { 0,   0,   0, 0, 0, 0, 0, 0, 1, 1, 1, 1 },
--   { 100, 0,   0, 0, 0, 0, 0, 0, 1, 1, 1, 1 },
--   { 100, 100, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1 },
--   { 0,   100, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1 },
-- }

local r = snap.math.random

local vertices = {
  -- x,  y,  z, t0,t1,n, t, r, g, b, a
  { -1, -1, -1, 0, 1, 1, 1, r(), r(), r(), 1 },
  { 1,  -1, -1, 1, 1, 1, 1, r(), r(), r(), 1 },
  { 1,  1,  -1, 1, 0, 1, 1, r(), r(), r(), 1 },
  { -1, 1,  -1, 0, 0, 1, 1, r(), r(), r(), 1 },
  { -1, -1, 1,  0, 1, 1, 1, r(), r(), r(), 1 },
  { 1,  -1, 1,  1, 1, 1, 1, r(), r(), r(), 1 },
  { 1,  1,  1,  1, 0, 1, 1, r(), r(), r(), 1 },
  { -1, 1,  1,  0, 0, 1, 1, r(), r(), r(), 1 },
}

local indices = {
  1, 3, 2, 1, 4, 3, -- back face
  5, 6, 7, 5, 7, 8, -- front face
  1, 2, 6, 1, 6, 5, -- bottom face
  2, 3, 7, 2, 7, 6, -- right face
  3, 4, 8, 3, 8, 7, -- top face
  4, 1, 5, 4, 5, 8, -- left face
}


local mesh = snap.graphics.newMesh(vertexformat, vertices, "triangles")
local indicesData = snap.data.newBytedata(#indices * 4)
for j = 1, #indices do
  indicesData:setUInt32((j - 1) * 4, indices[j] - 1)
end
mesh:setIndices(indicesData)

table.insert(commandBuffers, snap.graphics.submitGraphics())

local geometries = {}

for i = 1, 4 do
  local geometry = scene:createGeometry("Test geometry " .. i, mesh)
  table.insert(geometries, geometry)
end

local lod = scene:createLOD("Test LOD")
local lod2 = scene:createLOD("Test LOD 2")
local lod3 = scene:createLOD("Test LOD 3", 0.25)
local lod4 = scene:createLOD("Test LOD 4", 0.125)

lod:addGeometry(geometries[1])
lod2:addGeometry(geometries[2])
lod3:addGeometry(geometries[3])
lod4:addGeometry(geometries[4])

local shape = scene:createShape("Test shape", { lod, lod2, lod3 })
local shape2 = scene:createShape("Test shape 2", { lod4 })

local model = scene:createModel("Test model", { 100, 10, 0 }, { 0, 0, 0, 1 }, { 1, 1, 1 }, { shape, shape2 })

thread:start(commandsChannel, startThreadChannel, scene, events)

function snap.any(...)
  events:push({ ... })
end

function snap.update(dt)
  i = i + 1

  scene:update(dt)
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

  local buffer = commandsChannel:demand(10)
  table.insert(commandBuffers, buffer)

  local buffers = commandBuffers
  commandBuffers = {}

  return buffers
end
