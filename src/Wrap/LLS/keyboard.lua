---@meta

error("Do not require this file")


--- Checks if a key is currently being pressed
---@param ... string
---@return boolean anyDown
---@return boolean ... vararg returns the state of each key
function Thorium.keyboard.isDown(...) end

--- Checks if a scancode is currently being pressed
---@param ... string
---@return boolean anyDown
---@return boolean ... vararg returns the state of each scancode
function Thorium.keyboard.isScancodeDown(...) end
