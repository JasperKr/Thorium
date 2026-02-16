---@meta Gui

error("Do not require this file")

--- Creates a new frame
--- @param dt number Delta time
function Thorium.gui.newFrame(dt) end

--- Ends the current frame
function Thorium.gui.endFrame() end

--- Renders the current frame
function Thorium.gui.draw() end

--[[
    {"mousePressed", MousePressed}, -- x, y, button
    {"mouseReleased", MouseReleased}, -- x, y, button
    {"mouseMoved", MouseMoved}, -- x, y
    {"mouseWheelMoved", MouseWheelMoved}, -- x, y
    {"keyPressed", KeyPressed}, -- key
    {"keyReleased", KeyReleased}, -- key
    {"textInput", TextInput}, -- text
]]

--- Handles a mouse pressed event
--- @param x number X position
--- @param y number Y position
--- @param button number Button index
function Thorium.gui.mousePressed(x, y, button) end

--- Handles a mouse released event
--- @param x number X position
--- @param y number Y position
--- @param button number Button index
function Thorium.gui.mouseReleased(x, y, button) end

--- Handles a mouse moved event
--- @param x number X position
--- @param y number Y position
function Thorium.gui.mouseMoved(x, y) end

--- Handles a mouse wheel moved event
--- @param x number X position
--- @param y number Y position
function Thorium.gui.mouseWheelMoved(x, y) end

--- Handles a key pressed event
--- @param key number Key code
function Thorium.gui.keyPressed(key) end

--- Handles a key released event
--- @param key number Key code
function Thorium.gui.keyReleased(key) end

--- Handles a text input event
--- @param text string Input text
function Thorium.gui.textInput(text) end
