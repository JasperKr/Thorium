---@meta Shader

error("Do not require this file")

---@class snap.Shader : snap.Data
local Shader = {}

---@alias snap.ShaderResource
---| number
---| integer
---| snap.Texture
---| snap.Bytedata
---| table
---| snap.Buffer

--- Sends a uniform value to the shader.
---@param ... string|snap.ShaderResource The name keys of the uniform followed by the values to send.
function Shader:send(...) end

--- Checks if a shader has a uniform.
---@param ... string The name keys of the uniform.
---@return boolean hasUniform True if the uniform exists, false otherwise.
function Shader:hasUniform(...) end

--- Gets all of the uniforms in the shader.
---@return table<string[], snap.ShaderResource> uniforms A table of uniform names to their values.
function Shader:getUniforms() end
