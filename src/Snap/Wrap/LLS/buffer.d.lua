---@meta Buffer

error("Do not require this file")

---@class snap.Buffer : snap.Data
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
---@alias snap.BufferFormatComponentFormat
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
---
---|"floatmat2" # 16 byte 2x2 single-precision float matrix
---|"floatmat2x2" # 16 byte 2x2 single-precision float matrix
---|"floatmat2x3" # 24 byte 2x3 single-precision float matrix
---|"floatmat2x4" # 32 byte 2x4 single
---
---|"floatmat3" # 36 byte 3x3 single-precision float matrix
---|"floatmat3x2" # 24 byte 3x2 single-precision
---|"floatmat3x3" # 36 byte 3x3 single-precision float matrix
---|"floatmat3x4" # 48 byte 3x4 single-precision
---
---|"floatmat4" # 64 byte 4x4 single-precision float matrix
---|"floatmat4x2" # 32 byte 4x2 single-precision
---|"floatmat4x3" # 48 byte 4x3 single-precision
---|"floatmat4x4" # 64 byte 4x4 single-precision
---
---|"halfmat2" # 8 byte 2x2 half-precision float matrix
---|"halfmat2x2" # 8 byte 2x2 half-precision float matrix
---|"halfmat2x3" # 12 byte 2x3 half
---|"halfmat2x4" # 16 byte 2x4 half
---
---|"halfmat3" # 18 byte 3x3 half-precision float matrix
---|"halfmat3x2" # 12 byte 3x2 half
---|"halfmat3x3" # 18 byte 3x3 half-precision
---|"halfmat3x4" # 24 byte 3x4 half
---
---|"halfmat4" # 32 byte 4x4 half-precision float matrix
---|"halfmat4x2" # 16 byte 4x2 half
---|"halfmat4x3" # 24 byte 4x3 half
---|"halfmat4x4" # 32 byte 4x4 half-precision float matrix

---@alias snap.BufferFormatElement { name: string, format: snap.BufferFormatComponentFormat }
---@alias snap.BufferFormat snap.BufferFormatElement[]

---@alias snap.EvaluatedBufferFormatElement { name: string, offset: integer, format: snap.BufferFormatComponentFormat }
---@alias snap.EvaluatedBufferFormat snap.EvaluatedBufferFormatElement[]

--- Gets the format of the buffer
--- @return snap.EvaluatedBufferFormat format
function Buffer:getFormat() end

--- Sets the data of the buffer
--- @overload fun(data: number[], offset?: integer, size?: integer)
--- @param data snap.Bytedata The data to set
--- @param srcOffset integer? The offset in bytes or index to start reading from
--- @param dstOffset integer? The offset in bytes to start writing from
--- @param size integer? The number of bytes to set
function Buffer:setData(data, srcOffset, dstOffset, size) end

--- Clears the buffer data
--- @param value number? The value to clear the buffer with
--- @param offset integer? The offset in bytes to start clearing from
--- @param size integer? The number of bytes to clear
function Buffer:clear(value, offset, size) end

--- Copies data from this buffer to another buffer
--- @param dstBuffer snap.Buffer The destination buffer to copy to, can be the same as this buffer
--- @param srcIndex integer The index of the element in this buffer to start copying from
--- @param dstIndex integer The index of the element in the destination buffer to start copying to
--- @param size integer The number of elements to copy
function Buffer:copyTo(dstBuffer, srcIndex, dstIndex, size) end

--- Gets the offset of a component in the buffer format
--- @overload fun(index: integer): integer
--- @param name string The name of the component to get the offset of
--- @return integer offset The offset of the component in bytes, or nil if the component does not exist in the buffer format
function Buffer:getComponentOffset(name) end

--- Gets the debug name of the buffer
--- @return string debugName
function Buffer:getDebugName() end

--- Creates a new buffer
--- @param format snap.BufferFormat|snap.BufferFormatComponentFormat The format of the buffer
--- @param elementCount integer The number of elements in the buffer
--- @param usage { shaderstorage: boolean, uniform: boolean, vertex: boolean, index: boolean, cpupersistent: boolean }? The usage flags for the buffer
--- @return snap.Buffer buffer
function snap.graphics.newBuffer(format, elementCount, usage) end
