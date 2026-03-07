Imgui = require("cimgui.init")

local doneChannel, stopChannel = ...

local meshes = {} ---@type snap.Mesh[]
local colors = {
  { 1, 0, 0, 1 },
  { 0, 1, 0, 1 },
  { 0, 0, 1, 1 },
  { 1, 1, 1, 1 },
}

local drawCount = 4

print("Creating meshes...")

snap.graphics.aquireGraphics("load")

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

  local indicesData = snap.data.newBytedata(#indices * 4)
  for j = 1, #indices do
    indicesData:setUInt32((j - 1) * 4, indices[j] - 1)
  end

  meshes[i] = snap.graphics.newMesh(
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
snap.graphics.submitGraphics()
doneChannel:push(true)

snap.timer.sleep(0.1)

local function draw(i)
  snap.graphics.draw(meshes[i])
end

while true do
  if stopChannel:pop() then
    break
  end

  for i = 1, drawCount do
    print("THREAD #2 drawing " .. tostring(i))
    snap.graphics.aquireGraphics("square-" .. tostring(i))

    snap.timer.sleep(0.1)

    draw(i)
    snap.graphics.submitGraphics()
    doneChannel:push(i)
  end
end

print("THREAD #2 EXITING")
