local Graph = {}

local graphMetatable = {}

---@class SnapEngine.graph
---@field values table<number>
---@field min number
---@field max number
---@field average number
---@field maxValues number
---@field padding number
---@field minimumValueDifference number
---@field canvas snap.Texture?
---@field line table?
---@field flipX boolean
---@field flipY boolean
---@field color {[1]:number, [2]:number, [3]:number, [4]:number}
---@field height integer?
---@field invSize number
---@field renderOutline boolean?
---@field averageDifference number
local graphFunctions = {}

graphMetatable.__index = graphFunctions

---@class SnapEngine.graphSettings
---@field maxValues integer?
---@field padding number?
---@field minimumValueDifference number?
---@field hasGraphics boolean?
---@field height integer?
---@field flipX boolean?
---@field flipY boolean?
---@field color {[1]:number, [2]:number, [3]:number, [4]:number}?
---@field renderOutline boolean?


--- Creates a new graph
---@param settings SnapEngine.graphSettings
---@return SnapEngine.graph
function Graph.newGraph(settings)
  settings = settings or {}

  if settings.renderOutline == nil then
    settings.renderOutline = true
  end

  local self = {
    values = {},
    min = 0,
    max = 0,
    average = 0,
    maxValues = settings.maxValues or 400,
    padding = settings.padding or 0.1,
    minimumValueDifference = settings.minimumValueDifference or 0.0001,
    flipX = settings.flipX or false,
    flipY = settings.flipY or false,
    color = settings.color or { 1, 1, 1, 1 },
    invSize = 1,
    height = settings.height or 100,
    renderOutline = settings.renderOutline,
    averageDifference = 0,
  }

  if settings.hasGraphics then
    local rgba4Supported = snap.graphics.getTextureFormats({ canvas = true }).rgba4

    self.canvas = snap.graphics.newTexture(settings.maxValues or 400, settings.height or 100,
      { canvas = true, format = rgba4Supported and "rgba4" or "rgba8" })
    self.line = table.new(settings.maxValues or 400, 0)
  end

  setmetatable(self, graphMetatable)

  return self
end

function graphFunctions:push(value)
  table.insert(self.values, value)

  self.min = math.huge
  self.max = -math.huge

  self.averageDifference = 0

  local sum = 0
  local last = nil

  for i = 1, #self.values do
    sum = sum + self.values[i]

    self.min = math.min(self.min, self.values[i])
    self.max = math.max(self.max, self.values[i])

    if last then
      self.averageDifference = self.averageDifference + (self.values[i] - last)
    end

    last = self.values[i]
  end

  self.averageDifference = self.averageDifference / #self.values

  local size = self.max - self.min

  self.min = self.min - self.padding * size * 0.5
  self.max = self.max + self.padding * size * 0.5

  size = self.max - self.min

  if size < self.minimumValueDifference then
    self.min = self.min - (self.minimumValueDifference - size) / 2.0
    self.max = self.max + (self.minimumValueDifference - size) / 2.0
  end

  self.invSize = 1.0 / (self.max - self.min)

  self.average = sum / #self.values

  if #self.values > self.maxValues then
    table.remove(self.values, 1)
  end
end

function graphFunctions:clear()
  table.clear(self.values)
  self.min = 0
  self.max = 0
  self.average = 0
end

function graphFunctions:setMaxValues(amount)
  self.maxValues = amount

  if self.canvas and self.canvas:getWidth() ~= amount then
    self.canvas = snap.graphics.newTexture(amount, self.canvas:getHeight(), { canvas = true, format = "rgba4" })
    self.line = table.new(amount, 0)
  end
end

function graphFunctions:get(index)
  return self.values[index]
end

function graphFunctions:getNormalized(index)
  return (self.values[index] - self.min) * self.invSize
end

function graphFunctions:render(reset)
  assert(self.line ~= nil, "No line table")
  assert(self.canvas ~= nil, "No canvas")

  if reset ~= false then
    snap.graphics.push("all")
  end

  snap.graphics.setCanvas(self.canvas)
  snap.graphics.clear()
  snap.graphics.setColor(self.color)

  for i = 1, self:getSampleCount() do
    self.line[(i - 1) * 2 + 1] = self.flipX and (self.maxValues - i) or i
    local value = self:getNormalized(i)

    self.line[i * 2] = (self.flipY and (1.0 - value) or value) * self.height
  end

  if self:getSampleCount() > 3 then
    snap.graphics.line(self.line)
  end

  if self.renderOutline then
    snap.graphics.setColor(1, 1, 1, 0.5)
    snap.graphics.rectangle("line", 0, 0, self.maxValues, self.height)
  end

  if reset ~= false then
    snap.graphics.pop()
  end
end

function graphFunctions:getAverage()
  return self.average
end

function graphFunctions:getMin()
  return self.min
end

function graphFunctions:getMax()
  return self.max
end

function graphFunctions:getSampleCount()
  return #self.values
end

function graphFunctions:getDimensions()
  return self.maxValues, self.height
end

function graphFunctions:getAverageDifference()
  return self.averageDifference
end

function graphFunctions:getAtX(x)
  local index = math.floor(x)

  if self.flipX then
    index = self.maxValues - index
  end

  if index < 1 then
    return self.values[1]
  end

  if index > #self.values then
    return self.values[#self.values]
  end

  return self.values[index]
end

return Graph
