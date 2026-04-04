snap.internal.buffers = {}

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

---@class snap.WrappedBuffer
---@field buffer snap.Buffer the buffer object
---@field data snap.Bytedata the data of the buffer
---@field type "buffer" the type of the object
local bufferFunctions = {}
bufferMetatable.__index = bufferFunctions

function snap.graphics.newWrappedBuffer(format, cformat, elementCount, settings)
  local buffer = snap.graphics.newBuffer(format, elementCount, settings)
  assert(buffer:getElementStride() == ffi.sizeof(cformat),
    "The size of the C format must match the element stride of the buffer")
  local byteData = snap.data.newBytedata(buffer:getSize())

  local wrappedBuffer = {
    buffer = buffer,
    data = byteData,
    type = "buffer"
  }

  setmetatable(wrappedBuffer, bufferMetatable)

  return wrappedBuffer
end

--- updates the buffer with the data and sends it to the GPU
--- @param index integer? the index to start updating from
--- @param count integer? the amount of components to update
function bufferFunctions:flush(index, count)
  self.buffer:setData(self.data, index, index, count)

  return self
end

function bufferFunctions:release()
  self.buffer:release()
  self.data:release()
end
