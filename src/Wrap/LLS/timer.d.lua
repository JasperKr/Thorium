---@meta Timer

error("Do not require this file")

-- Timer module for Thorium

--- Gets the current time in seconds.
--- @return number Current time in seconds.
function Thorium.timer.getTime() end

--- Gets the time elapsed since the last frame in seconds.
--- @return number Delta time in seconds.
function Thorium.timer.getDelta() end

--- Gets the current frames per second (FPS).
--- @return number Current FPS.
function Thorium.timer.getFPS() end

--- Sleeps the current thread for the specified duration in seconds.
--- @param seconds number Duration to sleep in seconds.
function Thorium.timer.sleep(seconds) end

--- Gets the average delta time.
--- @return number Average delta time in seconds.
function Thorium.timer.getAverageDelta() end

--- Steps the timer one frame. Not thread-local.
function Thorium.timer.step() end
