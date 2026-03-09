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

local ptrAsNumber = snap.gui.getContextPtr()
local ptr = ffi.new("void *[1]")
ptr[0] = ffi.cast("void *", ptrAsNumber)

M.SetCurrentContext(ptr[0])

ptrAsNumber = snap.gui.getFontAtlasPtr()
ptr[0] = ffi.cast("void *", ptrAsNumber)

M.GetIO().Fonts = ffi.cast("ImFontAtlas *", ptr[0])

-- remove access to M._common
M._common = nil

return M
