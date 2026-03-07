---@meta Channel

error("Do not require this file")

---@class snap.Channel
local Channel = {}

--- Creates a new channel
--- @return snap.Channel channel
function snap.thread.newChannel() end

--- Pushes a value into the channel
---@param value any
function Channel:push(value) end

--- Demands a value from the channel, blocking if necessary
---@param timeout number? Timeout in seconds
---@return any value
function Channel:demand(timeout) end

--- pops a value from the channel, nil if none available
---@return any? value
function Channel:pop() end

--- Gets the number of items in the channel
---@return number count
function Channel:getCount() end

--- Clears the channel
function Channel:clear() end
