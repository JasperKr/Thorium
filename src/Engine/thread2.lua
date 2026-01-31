Imgui = require("cimgui.init")

local doneChannel, stopChannel = ...

local meshes = {} ---@type Thorium.Mesh[]
local colors = {
  { 1, 0, 0, 1 },
  { 0, 1, 0, 1 },
  { 0, 0, 1, 1 },
  { 1, 1, 1, 1 },
}

local drawCount = 4

print("Creating meshes...")

Thorium.graphics.aquireGraphics("load")

for i = 1, drawCount do
  -- x, y, u, v, r, g, b, a
  local vertices = {
    { 0,   0,   0, 0, colors[i][1], colors[i][2], colors[i][3], colors[i][4] },
    { 100, 0,   1, 0, colors[i][1], colors[i][2], colors[i][3], colors[i][4] },
    { 100, 100, 1, 1, colors[i][1], colors[i][2], colors[i][3], colors[i][4] },
    { 0,   100, 0, 1, colors[i][1], colors[i][2], colors[i][3], colors[i][4] },
  }

  for _, vertex in ipairs(vertices) do
    local offset = (i - 1) * 50
    vertex[1] = vertex[1] + offset
    vertex[2] = vertex[2] + offset
  end

  local indices = {
    1, 2, 3,
    3, 4, 1,
  }

  local indicesData = Thorium.data.newBytedata(#indices * 4)
  for j = 1, #indices do
    indicesData:setUInt32((j - 1) * 4, indices[j] - 1)
  end

  meshes[i] = Thorium.graphics.newMesh(
    {
      { name = "position", format = "floatvec2",  location = 0 },
      { name = "texcoord", format = "floatvec2",  location = 1 },
      { name = "color",    format = "unorm8vec4", location = 2 },
    },
    vertices
  )

  meshes[i]:setIndices(indicesData)

  print("Created mesh " .. tostring(i))
  print(" - Mesh has " .. tostring(meshes[i]:getVertexCount()) .. " vertices.")
  print(" - Mesh has " .. tostring(meshes[i]:getIndexCount()) .. " indices.")
end

print("Meshes created.")
Thorium.graphics.submitGraphics()
doneChannel:push(true)

Thorium.timer.sleep(0.1)

local function draw(i)
  Thorium.graphics.draw(meshes[i])
end

while true do
  if stopChannel:pop() then
    break
  end

  for i = 1, drawCount do
    print("THREAD #2 drawing " .. tostring(i))
    Thorium.graphics.aquireGraphics("square-" .. tostring(i))

    Thorium.timer.sleep(0.1)

    draw(i)
    Thorium.graphics.submitGraphics()
    doneChannel:push(i)
  end
end

print("THREAD #2 EXITING")
