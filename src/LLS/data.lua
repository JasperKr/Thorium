---@meta

error("Do not require this file")

--- Global Data class
---@class Thorium.Data
Data = {}

--- Releases the data from memory.
function Data:release() end

--- ByteData class
---@class Thorium.Bytedata : Thorium.Data
local Bytedata = {}

--- Gets the size of the Bytedata in bytes.
---@return number size The size in bytes.
function Bytedata:getSize() end

--- Gets a pointer to the Bytedata.
---@return integer pointer The pointer to the Bytedata.
function Bytedata:getPointer() end
