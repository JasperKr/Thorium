Thorium.internal.buffers = {}

local ffi = require("ffi")

--[[

buffer wrapper, each buffer is a list of elements,
the format describes how the elements are structured
each item in the format describes an attribute of the element
and each attribute has one or more components:

format = {
    { name = "Position", format = "floatvec3" },
    { name = "Color", format = "floatvec4" }
} --->

{ x, y, z, r, g, b, a, ... }

[x, y, z], [r, g, b, a] are the attributes
x, y, ... are the the components
[x, y, z, r, g, b, a] is one element

]]

local bufferMetatable = {}

---@class  Thorium.buffer
---@field buffer Thorium.Buffer the buffer object
---@field componentCount integer the amount of components in an element
---@field data Thorium.Bytedata the data of the buffer
---@field format table the format of the buffer
---@field formatIndexTable { string:integer } a table of component names to their index in the buffer
---@field engineData table a table for engine data
---@field writeIndex integer the current component write index
---@field elementCount integer the amount of elements in the buffer
---@field dataReferences { float:ffi.cdata*, int32_t:ffi.cdata*, uint32_t:ffi.cdata* } the data references for each component type
---@field componentTypes { [1]:"float"|"int32_t"|"uint32_t" } the component types in the buffer
---@field componentIndexToArrayIndex { [1]:integer } the index of the component in the buffer array
---@field byteDataPtr ffi.cdata* the pointer to the byte data
---@field elementComponentStride integer the stride of an element in components, since the final stride can be different due to internal padding
---@field invComponentCount number the inverse of the component count
---@field bufferCreationSettings table the settings used to create the buffer object
---@field encodeFunction function? the encoding function for the buffer
---@field type "buffer" the type of the object
local bufferFunctions = {}
bufferMetatable.__index = bufferFunctions

--- the amount of components in an attribute
local attributeComponentCount = {
  float = 1,
  floatvec2 = 2,
  floatvec3 = 3,
  floatvec4 = 4,
  floatmat2x2 = 4,
  floatmat2x3 = 6,
  floatmat2x4 = 8,
  floatmat3x2 = 6,
  floatmat3x3 = 9,
  floatmat3x4 = 12,
  floatmat4x2 = 8,
  floatmat4x3 = 12,
  floatmat4x4 = 16,
  int32 = 1,
  int32vec2 = 2,
  int32vec3 = 3,
  int32vec4 = 4,
  uint32 = 1,
  uint32vec2 = 2,
  uint32vec3 = 3,
  uint32vec4 = 4,
  unorm8vec2 = 2,
  unorm8vec4 = 4,
}

--- the ffi types for each buffer component type
local ffiTypes = {
  float       = "float",
  floatvec2   = "float",
  floatvec3   = "float",
  floatvec4   = "float",
  floatmat2x2 = "float",
  floatmat2x3 = "float",
  floatmat2x4 = "float",
  floatmat3x2 = "float",
  floatmat3x3 = "float",
  floatmat3x4 = "float",
  floatmat4x2 = "float",
  floatmat4x3 = "float",
  floatmat4x4 = "float",
  int32       = "int32_t",
  int32vec2   = "int32_t",
  int32vec3   = "int32_t",
  int32vec4   = "int32_t",
  uint32      = "uint32_t",
  uint32vec2  = "uint32_t",
  uint32vec3  = "uint32_t",
  uint32vec4  = "uint32_t"
}

Thorium.internal.attributeComponentCounts = attributeComponentCount
Thorium.internal.attributeTypeToFFIType = ffiTypes

--- Wrapper for Thorium.graphics.newBuffer
---@param format table|string
---@param elementCount number
---@param settings { usage: "static"|"dynamic"|"stream", shaderstorage:boolean, index:boolean, vertex:boolean, indirectarguments:boolean, texel:boolean}
---@return Thorium.buffer
function Thorium.graphics.newWrappedBuffer(format, elementCount, settings)
  Thorium.internal.assert(elementCount > 0, "Buffer item count must be greater than 0")

  if type(format) == "string" then
    format = { { format = format } }
  end

  local buffer = Thorium.graphics.newBuffer(format, elementCount, settings)

  local byteData = Thorium.data.newByteData(buffer:getSize())
  local byteDataPtr = byteData:getPointer()

  local floatArray = ffi.cast("float*", byteDataPtr)
  local int32Array = ffi.cast("int32_t*", byteDataPtr)
  local uint32Array = ffi.cast("uint32_t*", byteDataPtr)

  local componentTypes = {}

  local index = 0

  for i, component in ipairs(format) do
    local componentType = ffiTypes[component.format]

    Thorium.internal.assert(componentType, "Invalid buffer component format: " .. component.format)

    for j = 1, attributeComponentCount[component.format] do
      table.insert(componentTypes, componentType)

      if componentType == "float" then
        floatArray[index] = 0
      elseif componentType == "int32_t" then
        int32Array[index] = 0
      elseif componentType == "uint32_t" then
        uint32Array[index] = 0
      end

      index = index + 1
    end
  end

  local dataFormat = buffer:getFormat()
  local formatIndexTable = {}
  local componentIndexToArrayIndex = {}

  local componentCount = index
  local componentIndex = 0

  index = 0

  local hasIndexTable = true
  for i, component in ipairs(dataFormat) do
    if component.name then
      formatIndexTable[component.name] = componentIndex
    else
      hasIndexTable = false
    end

    for j = 1, attributeComponentCount[component.format] do
      componentIndexToArrayIndex[componentIndex] = component.offset / 4 + j - 1

      componentIndex = componentIndex + 1
    end

    index = index + 1
  end

  return setmetatable({
    buffer = buffer,
    data = byteData,

    dataFormat = dataFormat,
    byteDataPtr = byteDataPtr,

    dataReferences = {
      float = floatArray,
      int32_t = int32Array,
      uint32_t = uint32Array
    },

    componentTypes = componentTypes,
    componentIndexToArrayIndex = componentIndexToArrayIndex,

    format = format,

    formatIndexTable = hasIndexTable and formatIndexTable or nil, -- only set if all components have names
    engineData = {},

    writeIndex = 0,
    elementCount = elementCount,

    componentCount = componentCount,

    elementComponentStride = buffer:getElementStride() / 4,
    invComponentCount = 1.0 / componentCount,

    bufferCreationSettings = settings,
    type = "buffer"
  }, bufferMetatable)
end

--- resizes the buffer
--- @param count integer
--- @param useGPUData boolean? whether to use the GPU data or use the CPU data
--- @return Thorium.GraphicsBuffer
function bufferFunctions:resize(count, useGPUData)
  if self.elementCount == count then
    return self.buffer -- no need to resize
  end

  self.elementCount = count

  local buffer = Thorium.graphics.newBuffer(self.format, count, self.bufferCreationSettings)

  local byteData = Thorium.data.newByteData(buffer:getSize())
  local byteDataPtr = byteData:getPointer()

  local floatArray = ffi.cast("float*", byteDataPtr)
  local int32Array = ffi.cast("int32_t*", byteDataPtr)
  local uint32Array = ffi.cast("uint32_t*", byteDataPtr)

  ffi.copy(byteDataPtr, self.byteDataPtr, math.min(self.data:getSize(), byteData:getSize()))

  self.data:release()

  if not useGPUData then
    self.buffer:release()
  else
    self.buffer:copyTo(buffer, 0, 0, math.min(self.buffer:getSize(), buffer:getSize()))

    self.buffer:release()
  end

  self.buffer = buffer
  self.data = byteData
  self.byteDataPtr = byteDataPtr

  self.dataReferences = {
    float = floatArray,
    int32_t = int32Array,
    uint32_t = uint32Array
  }

  if not useGPUData then
    self:flush()
  end

  return buffer
end

function bufferFunctions:release()
  self.buffer:release()
  self.data:release()

  self.destroyed = true
end

--- gets the löve buffer object of the buffer
--- @return Thorium.GraphicsBuffer
function bufferFunctions:getBuffer()
  return self.buffer
end

--- gets the data of the buffer
--- @return Thorium.ByteData
function bufferFunctions:getData()
  return self.data
end

function bufferFunctions:getComponentType(index)
  local typeIndex = index % self.componentCount + 1

  return self.componentTypes[typeIndex]
end

function bufferFunctions:getArrayIndex(index)
  local arrayIndex = self.componentIndexToArrayIndex[index % self.componentCount]
  arrayIndex = arrayIndex + math.floor(index * self.invComponentCount + 0.001) * self.elementComponentStride

  return arrayIndex
end

--- sets a number at a specific index in the buffer
---@param index integer
---@param value number
function bufferFunctions:setNumberAt(index, value)
  local arrayIndex = self:getArrayIndex(index)

  self.dataReferences.float[arrayIndex] = value

  return self
end

--- sets a int at a specific index in the buffer
--- @param index integer
--- @param value number
function bufferFunctions:setIntAt(index, value)
  local arrayIndex = self:getArrayIndex(index)

  self.dataReferences.int32_t[arrayIndex] = value

  return self
end

--- sets a uint at a specific index in the buffer
--- @param index integer
--- @param value number
function bufferFunctions:setUintAt(index, value)
  local arrayIndex = self:getArrayIndex(index)

  self.dataReferences.uint32_t[arrayIndex] = value

  return self
end

--- sets a value at a specific index in the buffer
--- @param index integer
--- @param value number|vec2|vec3|vec4|quaternion|matrix3x3|matrix4x4|table
--- @return integer # the new index, incremented by the amount of components written
function bufferFunctions:setAt(index, value)
  local t = type(value)
  local RType = Thorium.type(value)

  Thorium.internal.assert(index >= 0)

  if t == "number" then
    local array = self.dataReferences[self:getComponentType(index)]

    local arrayIndex = self:getArrayIndex(index)

    array[arrayIndex] = value

    return index + 1
  elseif RType == "vec2" then
    index = self:setAt(index, value.x)
    index = self:setAt(index, value.y)
  elseif RType == "vec3" then
    index = self:setAt(index, value.x)
    index = self:setAt(index, value.y)
    index = self:setAt(index, value.z)
  elseif RType == "vec4" then
    index = self:setAt(index, value.x)
    index = self:setAt(index, value.y)
    index = self:setAt(index, value.z)
    index = self:setAt(index, value.w)
  elseif RType == "quaternion" then
    index = self:setAt(index, value.x)
    index = self:setAt(index, value.y)
    index = self:setAt(index, value.z)
    index = self:setAt(index, value.w)
  elseif RType == "mat3" then
    index = self:setAt(index, value[1][1])
    index = self:setAt(index, value[1][2])
    index = self:setAt(index, value[1][3])
    index = self:setAt(index, value[2][1])
    index = self:setAt(index, value[2][2])
    index = self:setAt(index, value[2][3])
    index = self:setAt(index, value[3][1])
    index = self:setAt(index, value[3][2])
    index = self:setAt(index, value[3][3])
  elseif RType == "mat4" then
    index = self:setAt(index, value[1][1])
    index = self:setAt(index, value[1][2])
    index = self:setAt(index, value[1][3])
    index = self:setAt(index, value[1][4])
    index = self:setAt(index, value[2][1])
    index = self:setAt(index, value[2][2])
    index = self:setAt(index, value[2][3])
    index = self:setAt(index, value[2][4])
    index = self:setAt(index, value[3][1])
    index = self:setAt(index, value[3][2])
    index = self:setAt(index, value[3][3])
    index = self:setAt(index, value[3][4])
    index = self:setAt(index, value[4][1])
    index = self:setAt(index, value[4][2])
    index = self:setAt(index, value[4][3])
    index = self:setAt(index, value[4][4])
  elseif t == "table" then
    for i, v in ipairs(value) do
      index = self:setAt(index, v)
    end
  else
    error("Engine Error: Invalid buffer write value type: " .. t)
  end

  return index
end

local values = {}

--- gets a number at a specific index in the buffer
--- @param index integer
--- @param count? integer
--- @return number|integer ...
function bufferFunctions:getAt(index, count)
  local arrayIndex = self:getArrayIndex(index)
  local componentType = self:getComponentType(index)

  if count then
    table.clear(values)

    for i = 1, count do
      table.insert(values, self.dataReferences[componentType][arrayIndex + i - 1])
    end

    return unpack(values, 1, count)
  else
    return self.dataReferences[componentType][arrayIndex]
  end
end

--- writes to the buffer iteratively,
--- can be used in a for-loop to write all the data
--- @param value number|vec2|vec3|vec4|quaternion|matrix3x3|matrix4x4|table
function bufferFunctions:write(value)
  self.writeIndex = self:setAt(self.writeIndex, value)

  return self
end

--- writes many values to the buffer iteratively,
--- can be used in a for-loop to write all the data
--- @param ... number|vec2|vec3|vec4|quaternion|matrix3x3|matrix4x4|table
--- @return Thorium.buffer
function bufferFunctions:writeMany(...)
  for i = 1, select("#", ...) do
    self.writeIndex = self:setAt(self.writeIndex, select(i, ...))
  end

  return self
end

--- writes to the buffer iteratively,
--- can be used in a for-loop to write all the data
--- @param value number float value
function bufferFunctions:writeFloat(value)
  local arrayIndex = self:getArrayIndex(self.writeIndex)

  self.dataReferences.float[arrayIndex] = value

  self.writeIndex = self.writeIndex + 1

  return self
end

--- writes many values to the buffer iteratively,
--- can be used in a for-loop to write all the data
--- @param ... number|ffi.cdata*
--- @return Thorium.buffer
function bufferFunctions:writeFloatMany(...)
  for i = 1, select("#", ...) do
    self:writeFloat(select(i, ...))
  end

  return self
end

--- writes to the buffer iteratively,
--- can be used in a for-loop to write all the data
--- @param value number int value
function bufferFunctions:writeInt(value)
  local arrayIndex = self:getArrayIndex(self.writeIndex)

  self.dataReferences.int32_t[arrayIndex] = value

  self.writeIndex = self.writeIndex + 1

  return self
end

--- writes to the buffer iteratively,
--- can be used in a for-loop to write all the data
--- @param ... number|ffi.cdata*
function bufferFunctions:writeIntMany(...)
  for i = 1, select("#", ...) do
    self:writeInt(select(i, ...))
  end

  return self
end

--- writes to the buffer iteratively,
--- can be used in a for-loop to write all the data
--- @param value number uint value
function bufferFunctions:writeUint(value)
  local arrayIndex = self:getArrayIndex(self.writeIndex)

  self.dataReferences.uint32_t[arrayIndex] = value

  self.writeIndex = self.writeIndex + 1

  return self
end

--- writes to the buffer iteratively,
--- can be used in a for-loop to write all the data
--- @param ... number|ffi.cdata*
function bufferFunctions:writeUIntMany(...)
  for i = 1, select("#", ...) do
    self:writeUint(select(i, ...))
  end

  return self
end

--- sets the component write index of the buffer, 0-based indexing
--- @param index integer
function bufferFunctions:setComponentWriteIndex(index)
  self.writeIndex = index
  Thorium.internal.assert(self.writeIndex >= 0, "Buffer write index out of bounds")

  return self
end

--- gets the component write index of the buffer
--- @return integer
function bufferFunctions:getWriteIndex()
  return self.writeIndex
end

--- gets the item write index of the buffer
--- @return integer
function bufferFunctions:getElementWriteIndex()
  return math.floor(self.writeIndex / self.componentCount + 0.001)
end

--- sets the write index of the buffer to the start of an item [1-n]
function bufferFunctions:setElementWriteIndex(index)
  self.writeIndex = (index - 1) * self.componentCount

  return self
end

--- updates the buffer with the data and sends it to the GPU
--- @param index integer? the index to start updating from
--- @param count integer? the amount of components to update
function bufferFunctions:flush(index, count)
  if self.destroyed then error("Buffer has been destroyed") end

  self.buffer:setData(self.data, index, index, count)

  return self
end

--- adds 1 to the write index
function bufferFunctions:skipComponent()
  self.writeIndex = self.writeIndex + 1

  return self
end

--- adds a number to the write index
function bufferFunctions:skipComponents(count)
  self.writeIndex = self.writeIndex + count

  return self
end

--- copies an item from the buffer to another item
---@param source number
---@param destination number
function bufferFunctions:copyElementTo(source, destination)
  local sourceIndex = source * self.componentCount
  local destinationIndex = destination * self.componentCount

  for i = 0, self.componentCount - 1 do
    local array = self.dataReferences[self:getComponentType(i)]

    local destComponentIndex = self:getArrayIndex(destinationIndex + i)
    local sourceComponentIndex = self:getArrayIndex(sourceIndex + i)

    array[destComponentIndex] = array[sourceComponentIndex]
  end
end

--- gets the component count of the buffer
--- @return integer
function bufferFunctions:getComponentCount()
  return self.componentCount
end

--- gets the element stride of the buffer
--- @return integer
function bufferFunctions:getElementStride()
  return self.buffer:getElementStride()
end

--- Clear the buffer data
--- @return Thorium.buffer
function bufferFunctions:clear()
  self.buffer:clear()
  ffi.fill(self.byteDataPtr, self.data:getSize(), 0)

  return self
end

--- Copy data from another buffer to this buffer
--- @param other Thorium.buffer|Thorium.byteData
--- @param count? integer byte count to copy, defaults to the size of the other buffer
--- @param from? integer the offset in bytes from which to start copying in the other buffer, defaults to 0
--- @param to? integer the byte offset to start copying to, defaults to 0
function bufferFunctions:copyFrom(other, count, from, to)
  to = to or 0
  count = count or other.data:getSize()
  from = from or 0

  local dstPtr = ffi.cast("char*", self.data:getPointer())
  local srcPtr = ffi.cast("char*", other.data:getPointer())

  ffi.copy(dstPtr + to, srcPtr + from, count)

  return self
end

--- Copy data from another buffer to this buffer
--- @param other ffi.ct*|ffi.cdata*
--- @param count integer byte count to copy, defaults to the size of the other buffer
--- @param from? integer the offset in bytes from which to start copying in the other buffer, defaults to 0
--- @param to? integer the byte offset to start copying to, defaults to 0
function bufferFunctions:copyFromPtr(other, count, from, to)
  to = to or 0
  from = from or 0

  local dstPtr = ffi.cast("char*", self.data:getPointer())
  local srcPtr = ffi.cast("char*", other)

  ffi.copy(dstPtr + to, srcPtr + from, count)

  return self
end

--- formats a number to a string
--- @param itemType "float"|"int32_t"|"uint32_t"
--- @param num number
--- @return string
local formatNum = function(itemType, num)
  num = num or "INVALID VALUE"

  if itemType == "float" then
    return string.format("%.4f", num)
  elseif itemType == "int32_t" then
    return num .. "i"
  elseif itemType == "uint32_t" then
    return num .. "ui"
  end

  error("Engine error: Invalid item type")
end

--- draws a matrix in the buffer
---@param self Thorium.buffer
---@param sx integer
---@param sy integer
---@param index integer
---@return integer
local function drawMatrix(self, sx, sy, index)
  for y = 1, sy do
    local text = ""
    for x = 1, sx do
      local arrayIndex = self:getArrayIndex(index)

      text = text .. formatNum("float", self.dataReferences.float[arrayIndex])
      text = text .. (x ~= sx and ", " or "")

      index = index + 1
    end
    Thorium.internal.imgui.Text(text)
  end

  return index
end

-- draws a gui overlay with imGui
function bufferFunctions:draw()
  local imgui = Thorium.internal.imgui

  if imgui.IsWindowAppearing() then
    imgui.SetNextItemWidth(Thorium.graphics.getWidth() * 0.75)
  end

  if imgui.Begin("Buffer '" .. self.buffer:getDebugName() .. "' Data") then
    imgui.Text("Element Count: " .. self.buffer:getElementCount())
    imgui.Text("Calculated element stride: " .. self.componentCount * 4)
    imgui.Text("Final element stride: " .. self.buffer:getElementStride())
    imgui.Text("Calculated component amount: " .. self.componentCount)
    imgui.Text("Final size in bytes: " .. self.buffer:getSize())

    if imgui.BeginTable("Buffer Data", #self.format, bit.bor(imgui.ImGuiTableFlags_Borders, imgui.ImGuiTableFlags_SizingFixedFit)) then
      imgui.TableNextRow()
      for i = 1, #self.format do
        imgui.TableSetColumnIndex(i - 1)
        imgui.Text(self.format[i].name or "Unknown")
      end

      imgui.TableNextRow()
      local index = 0
      for i = 1, #self.format do
        imgui.TableSetColumnIndex(i - 1)

        imgui.Text(self.format[i].format)
      end

      imgui.TableSetColumnIndex(0)
      index = 0
      for row = 1, self.buffer:getElementCount() do
        imgui.TableNextRow()
        for i, form in ipairs(self.format) do
          local format = form.format
          imgui.TableSetColumnIndex(i - 1)
          if format == "floatmat2x2" then
            index = drawMatrix(self, 2, 2, index)
          elseif format == "floatmat2x3" then
            index = drawMatrix(self, 2, 3, index)
          elseif format == "floatmat2x4" then
            index = drawMatrix(self, 2, 4, index)
          elseif format == "floatmat3x2" then
            index = drawMatrix(self, 3, 2, index)
          elseif format == "floatmat3x3" then
            index = drawMatrix(self, 3, 3, index)
          elseif format == "floatmat3x4" then
            index = drawMatrix(self, 3, 4, index)
          elseif format == "floatmat4x2" then
            index = drawMatrix(self, 4, 2, index)
          elseif format == "floatmat4x3" then
            index = drawMatrix(self, 4, 3, index)
          elseif format == "floatmat4x4" then
            index = drawMatrix(self, 4, 4, index)
          else
            local text = ""
            for j = 1, attributeComponentCount[format] do
              local arrayIndex = self:getArrayIndex(index)
              local arrayType = self:getComponentType(index)

              text = text .. formatNum(arrayType, self.dataReferences[arrayType][arrayIndex])
              text = text .. (j ~= attributeComponentCount[format] and ", " or "")

              index = index + 1
            end
            imgui.Text(text)
          end
        end
      end
    end
    imgui.EndTable()
  end
  imgui.End()
end

--- Sets a buffer component value.
--- Does not increment the write index
--- @param name string
--- @param value number|vec2|vec3|vec4|quaternion|matrix3x3|matrix4x4|number[]|integer[]
--- @param offset? integer
function bufferFunctions:set(name, value, offset)
  Thorium.internal.assert(self.formatIndexTable, "One or more buffer components do not have names")
  local index = self.formatIndexTable[name]

  Thorium.internal.assert(index, "Invalid buffer component name: " .. name)

  index = index + (offset or 0) * self.componentCount

  return self:setAt(index, value)
end

--- Gets a buffer component value.
--- Does not increment the write index
--- @param name string
--- @param offset? integer
--- @param count? integer
function bufferFunctions:get(name, offset, count)
  Thorium.internal.assert(self.formatIndexTable, "One or more buffer components do not have names")
  local index = self.formatIndexTable[name]

  Thorium.internal.assert(index, "Invalid buffer component name: " .. name)

  index = index + (offset or 0) * self.componentCount

  return self:getAt(index, count)
end

--- function to encode an element when writing one at once
--- this can be useful if you want to encode the data in a specific way, for example:
---
--- rgba8 to uint32_t, you can supply 4 numbers and encode them into a uint32_t with your function
---
--- only applies when using "writeElement"
--- @param func (fun(...):...)? the encoding function, nil if you want to disable encoding
function bufferFunctions:encodeElementsWith(func)
  Thorium.assertType(func, "function", true)
  self.encodeFunction = func
end

function bufferFunctions:writeElement(...)
  if self.encodeFunction then
    return self:writeMany(self.encodeFunction(...))
  else
    return self:writeMany(...)
  end
end

--- reset all values at an element index to 0
function bufferFunctions:clearElement(index)
  self:setElementWriteIndex(index)

  for i = 1, self.componentCount do
    self:write(0)
  end
end
