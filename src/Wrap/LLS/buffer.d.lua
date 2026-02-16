---@meta Buffer

error("Do not require this file")

---@class Thorium.Buffer
local Buffer = {}

--- Gets the size of the buffer in bytes
--- @return integer size
function Buffer:getSize() end

--- Gets the number of elements in the buffer
--- @return integer count
function Buffer:getElementCount() end

--- Gets the stride of each element in the buffer in bytes
--- @return integer stride
function Buffer:getElementStride() end

--- Clears the buffer data
--- @param value number? The value to clear the buffer with
--- @param offset integer? The offset in bytes to start clearing from
--- @param size integer? The number of bytes to clear
function Buffer:clear(value, offset, size) end

---TODO: Fix this fucking name
---@alias Thorium.BufferFormatComponentFormat
---|"uint8"    # 1 byte unsigned integer
---|"uint8vec2" # 2 byte unsigned integer vector
---|"uint8vec3" # 3 byte unsigned integer vector
---|"uint8vec4" # 4 byte unsigned integer vector
---
---|"uint16"   # 2 byte unsigned integer
---|"uint16vec2" # 4 byte unsigned integer vector
---|"uint16vec3" # 6 byte unsigned integer vector
---|"uint16vec4" # 8 byte unsigned integer vector
---
---|"uint32"   # 4 byte unsigned integer
---|"uint32vec2" # 8 byte unsigned integer vector
---|"uint32vec3" # 12 byte unsigned integer vector
---|"uint32vec4" # 16 byte unsigned integer vector
---
---|"int8"     # 1 byte signed integer
---|"int8vec2"  # 2 byte signed integer vector
---|"int8vec3"  # 3 byte signed integer vector
---|"int8vec4"  # 4 byte signed integer vector
---
---|"int16"    # 2 byte signed integer
---|"int16vec2"  # 4 byte signed integer vector
---|"int16vec3"  # 6 byte signed integer vector
---|"int16vec4"  # 8 byte signed integer vector
---
---|"int32"    # 4 byte signed integer
---|"int32vec2"  # 8 byte signed integer vector
---|"int32vec3"  # 12 byte signed integer vector
---|"int32vec4"  # 16 byte signed integer vector
---
---|"half"  # 2 byte half-precision float
---|"halfvec2" # 4 byte half-precision float vector
---|"halfvec3" # 6 byte half-precision float vector
---|"halfvec4" # 8 byte half-precision float vector
---
---|"float" # 4 byte single-precision float
---|"floatvec2" # 8 byte single-precision float vector
---|"floatvec3" # 12 byte single-precision float vector
---|"floatvec4" # 16 byte single-precision float vector
---
---|"unorm8"    # 1 byte unsigned normalized integer
---|"unorm8vec2" # 2 byte unsigned normalized integer vector
---|"unorm8vec3" # 3 byte unsigned normalized integer vector
---|"unorm8vec4" # 4 byte unsigned normalized integer vector
---
---|"unorm16"   # 2 byte unsigned normalized integer
---|"unorm16vec2" # 4 byte unsigned normalized integer vector
---|"unorm16vec3" # 6 byte unsigned normalized integer vector
---|"unorm16vec4" # 8 byte unsigned normalized integer vector
---
---|"snorm8"    # 1 byte signed normalized integer
---|"snorm8vec2" # 2 byte signed normalized integer vector
---|"snorm8vec3" # 3 byte signed normalized integer vector
---|"snorm8vec4" # 4 byte signed normalized integer vector
---
---|"snorm16"   # 2 byte signed normalized integer
---|"snorm16vec2" # 4 byte signed normalized integer vector
---|"snorm16vec3" # 6 byte signed normalized integer vector
---|"snorm16vec4" # 8 byte signed normalized integer vector

---@alias Thorium.BufferFormatElement { name: string, format: Thorium.BufferFormatComponentFormat }
---@alias Thorium.BufferFormat Thorium.BufferFormatElement[]

---@alias Thorium.EvaluatedBufferFormatElement { name: string, offset: integer, format: Thorium.BufferFormatComponentFormat }
---@alias Thorium.EvaluatedBufferFormat Thorium.EvaluatedBufferFormatElement[]

--- Gets the format of the buffer
--- @return Thorium.EvaluatedBufferFormat format
function Buffer:getFormat() end

--- Sets the data of the buffer
--- @overload fun(data: number[], offset?: integer, size?: integer)
--- @param data Thorium.Bytedata The data to set
--- @param offset integer? The offset in bytes to start setting from
--- @param size integer? The number of bytes to set
function Buffer:setData(data, offset, size) end

--- Creates a new buffer
--- @param format Thorium.BufferFormat|Thorium.BufferFormatComponentFormat The format of the buffer
--- @param elementCount integer The number of elements in the buffer
--- @param usage { shaderstorage: boolean, uniform: boolean, vertex: boolean, index: boolean, cpupersistent: boolean }? The usage flags for the buffer
--- @return Thorium.Buffer buffer
function Thorium.graphics.newBuffer(format, elementCount, usage) end
