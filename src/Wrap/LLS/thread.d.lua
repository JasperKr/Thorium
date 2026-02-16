---@meta Thread

error("Do not require this file")

---@class Thorium.Thread
local Thread = {}

--- Creates a new thread
---@param source string Filepath or Source string
---@param debugName string? Optional debug name for tracy
---@return Thorium.Thread thread
function Thorium.thread.newThread(source, debugName) end

--- Starts the thread
---@param ... any Arguments to pass to the thread
function Thread:start(...) end

--- Waits for the thread to finish
function Thread:wait() end

--- Gets a thread's status
---@return string status
function Thread:getStatus() end

--- Gets an error message from the thread if available
---@return string|nil errorMessage
function Thread:getError() end
