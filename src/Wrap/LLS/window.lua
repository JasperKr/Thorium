---@meta Timer

error("Do not require this file")

--- Hides the window.
function Thorium.window.hide() end

--- Returns the number of available displays.
---@return integer
function Thorium.window.getDisplayCount() end

--- Returns the name of the specified display.
---@param displayIndex integer
---@return string
function Thorium.window.getDisplayName(displayIndex) end

--- Returns whether the window is in fullscreen mode.
---@return boolean
function Thorium.window.isFullscreen() end

--- Returns the dimensions of the specified fullscreen display.
---@param displayIndex integer
---@return integer, integer
function Thorium.window.getFullscreenDimensions(displayIndex) end

--- Sets whether the window is in fullscreen mode.
---@param fullscreen boolean
function Thorium.window.setFullscreen(fullscreen) end

--- Returns the width of the window.
---@return integer
function Thorium.window.getWidth() end

--- Returns the height of the window.
---@return integer
function Thorium.window.getHeight() end

--- Returns the dimensions of the window.
---@return integer, integer
function Thorium.window.getDimensions() end

--- Sets the dimensions of the window.
---@param width integer
---@param height integer
function Thorium.window.setDimensions(width, height) end

--- Sets the icon of the window.
---@param imageData string
function Thorium.window.setIcon(imageData) end

---@alias Thorium.VsyncMode
---| "enabled" # Standard VSync enabled.
---| "immediate" # VSync disabled. Tearing may occur.
---| "adaptive" # Adaptive VSync. Disable Vsync when framerate is lower than the monitor refreshrate.
---| "replace" # Vsync enabled, no framerate limit.

---@class Thorium.WindowSettings
---@field vsync Thorium.VsyncMode? VSync mode.
---@field resizable boolean? Whether the window is resizable.
---@field borderless boolean? Whether the window is borderless.
---@field fullscreen boolean? Whether the window is fullscreen.
---@field displayIndex integer? The display index to use.
---@field width integer? The window width.
---@field height integer? The window height.
---@field minimumWidth integer? The minimum window width.
---@field minimumHeight integer? The minimum window height.
---@field xPosition integer? The X position of the window.
---@field yPosition integer? The Y position of the window.

--- Returns the current window settings.
---@return Thorium.WindowSettings
function Thorium.window.getSettings() end

--- Sets the window settings.
---@param settings Thorium.WindowSettings
function Thorium.window.setSettings(settings) end

--- Updates specific window settings.
---@param settings Thorium.WindowSettings
function Thorium.window.updateSettings(settings) end

--- Returns the window title.
---@return string
function Thorium.window.getTitle() end

--- Sets the window title.
---@param title string
function Thorium.window.setTitle(title) end

--- Returns the window position.
---@return integer, integer
function Thorium.window.getPosition() end

--- Sets the window position.
---@param x integer
---@param y integer
function Thorium.window.setPosition(x, y) end

--- Sets the VSync mode.
---@param mode Thorium.VsyncMode
function Thorium.window.setVSync(mode) end

--- Returns whether the window has mouse focus.
---@return boolean
function Thorium.window.hasMouseFocus() end

--- Returns whether the window has keyboard focus.
---@return boolean
function Thorium.window.hasKeyboardFocus() end

--- Returns whether the window is visible.
---@return boolean
function Thorium.window.isVisible() end

--- Returns whether the window is open.
---@return boolean
function Thorium.window.isOpen() end

--- Returns whether the window is minimized.
---@return boolean
function Thorium.window.isMinimized() end

--- Returns whether the window is maximized.
---@return boolean
function Thorium.window.isMaximized() end

--- Minimizes the window.
function Thorium.window.minimize() end

--- Maximizes the window.
function Thorium.window.maximize() end

--- Restores the window from minimized or maximized state.
function Thorium.window.restore() end

--- Enables or disables display sleep.
---@param enable boolean
function Thorium.window.enableDisplaySleep(enable) end

--- Returns whether display sleep is enabled.
---@return boolean
function Thorium.window.isDisplaySleepEnabled() end

--- Requests user attention to the window.
---@param continuous boolean Whether to continuously request attention until the window gains focus.
function Thorium.window.requestAttention(continuous) end
