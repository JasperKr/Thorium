local thisfilepath = ...
local thisdirectory = ""
local i = 1

local start, ending = string.find(thisfilepath, "%.", i)
while start do
    thisdirectory = thisdirectory .. string.sub(thisfilepath, i, start - 1) .. "."
    i = ending + 1
    start, ending = string.find(thisfilepath, "%.", i)
end

M = {
    love = {},
    _common = {}
}


require(thisdirectory .. "cdef")

local ffi = require("ffi")
M.C = ffi.load("./bin/cimgui/cimgui.so")

require(thisdirectory .. "enums")
require(thisdirectory .. "wrap")

-- remove access to M._common
M._common = nil

return M
