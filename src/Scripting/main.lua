local i = 0
snap.threaderror = error
print("Starting thread 1")
require("init")

local thread = snap.thread.newThread("src/Scripting/thread.lua", "Render thread 1")
local commandsChannel = snap.thread.newChannel()
local startThreadChannel = snap.thread.newChannel()
local events = snap.thread.newChannel()
local scene = snap.scene.newScene("Main")
print(scene:getName())

snap.graphics.aquireGraphics("load")

snap.renderer.initialize()


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

local bit = require("bit")
local function encodeNormal(x, y, z)
  x = x * 0.5 + 0.5
  y = y * 0.5 + 0.5
  z = z * 0.5 + 0.5

  x = x * 1023
  y = y * 1023
  z = z * 1023

  return bit.bor(
    bit.lshift(z, 20),
    bit.lshift(y, 10),
    x
  )
end

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

local unpackedVertices = {}

for i = 1, #indices, 3 do
  local v1 = vertices[indices[i]]
  local v2 = vertices[indices[i + 1]]
  local v3 = vertices[indices[i + 2]]

  local edge1 = { v2[1] - v1[1], v2[2] - v1[2], v2[3] - v1[3] }
  local edge2 = { v3[1] - v1[1], v3[2] - v1[2], v3[3] - v1[3] }

  -- local normal = {
  --   edge1[2] * edge2[3] - edge1[3] * edge2[2],
  --   edge1[3] * edge2[1] - edge1[1] * edge2[3],
  --   edge1[1] * edge2[2] - edge1[2] * edge2[1],
  -- }
  local normalX = edge1[2] * edge2[3] - edge1[3] * edge2[2]
  local normalY = edge1[3] * edge2[1] - edge1[1] * edge2[3]
  local normalZ = edge1[1] * edge2[2] - edge1[2] * edge2[1]

  -- local length = math.sqrt(normal[1] ^ 2 + normal[2] ^ 2 + normal[3] ^ 2)
  local length = math.sqrt(normalX ^ 2 + normalY ^ 2 + normalZ ^ 2)
  -- normal = { normal[1] / length, normal[2] / length, normal[3] / length }
  normalX = normalX / length
  normalY = normalY / length
  normalZ = normalZ / length

  table.insert(unpackedVertices, {
    v1[1],
    v1[2],
    v1[3],
    v1[4],
    v1[5],
    encodeNormal(normalX, normalY, normalZ),
    encodeNormal(0, 0, 0),
    v1[8],
    v1[9],
    v1[10],
    v1[11],
  })

  table.insert(unpackedVertices, {
    v2[1],
    v2[2],
    v2[3],
    v2[4],
    v2[5],
    encodeNormal(normalX, normalY, normalZ),
    encodeNormal(0, 0, 0),
    v2[8],
    v2[9],
    v2[10],
    v2[11],
  })

  table.insert(unpackedVertices, {
    v3[1],
    v3[2],
    v3[3],
    v3[4],
    v3[5],
    encodeNormal(normalX, normalY, normalZ),
    encodeNormal(0, 0, 0),
    v3[8],
    v3[9],
    v3[10],
    v3[11],
  })
end


-- local mesh = snap.graphics.newMesh(vertexformat, unpackedVertices, "triangles")

-- local lod = scene:createLOD("Test LOD")
-- local lod2 = scene:createLOD("Test LOD 2")
-- local lod3 = scene:createLOD("Test LOD 3", 0.25)

-- lod:addGeometry(scene:createGeometry("Test geometry " .. i, mesh))
-- lod2:addGeometry(scene:createGeometry("Test geometry " .. i, mesh))
-- lod3:addGeometry(scene:createGeometry("Test geometry " .. i, mesh))

-- local shape = scene:createShape("Test shape", { lod })
-- local shape2 = scene:createShape("Test shape 2", { lod2 })
-- local shape3 = scene:createShape("Test shape 3", { lod3 })

-- local model = scene:createModel("Test model", { 0, 0, 0 }, { 0, 0, 0, 1 }, { 1, 1, 1 }, { shape })
-- local model2 = scene:createModel("Test model 2", { 0, 5, 0 }, { 0, 0, 0, 1 }, { 1, 1, 1 }, { shape2 })
-- local model3 = scene:createModel("Test model 3", { 0, 10, 0 }, { 0, 0, 0, 1 }, { 1, 1, 1 }, { shape3 })

local qx, qy, qz, qw = snap.math.eulerToQuaternion(0.3, -math.pi / 1.5, 0);
scene:newDirectionalLight("Test directional light", qx, qy, qz, qw, 1, 1, 1, 5)

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
  snap.gui.shutdown()

  print("Quitting the application.")
  return 1
end

snap.keyboard.setEnableTextInput(true)

local firstFrame = true
local commandBuffers = {}

function snap.draw()
  startThreadChannel:push(true)

  -- for i, buffer in ipairs(commandBuffers) do
  --   buffer:release()
  -- end

  table.clear(commandBuffers)

  if firstFrame then
    firstFrame = false
    table.insert(commandBuffers, snap.graphics.submitGraphics())
  end

  local gotBuffer = false

  while not gotBuffer do
    if thread:getStatus() ~= "running" then
      print("Render thread status: " .. thread:getStatus() .. ", exiting main thread.")
      print("Render thread error: " .. tostring(thread:getError()))
      snap.event.quit()
      collectgarbage("collect")
      collectgarbage("collect")
      return
    end

    local buffer = commandsChannel:demand(0.01)
    while buffer do
      table.insert(commandBuffers, buffer)
      buffer = commandsChannel:pop()
      gotBuffer = true
    end
  end

  -- snap.graphics.aquireGraphics()
  -- snap.graphics.clear(0, 0, 0, 1)
  -- table.insert(commandBuffers, snap.graphics.submitGraphics())

  return commandBuffers
end
