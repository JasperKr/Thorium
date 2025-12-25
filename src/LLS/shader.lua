---@meta

error("Do not require this file")

---@class Thorium.Shader : Thorium.Data
local Shader = {}

---@alias Thorium.ShaderResource
---| number
---| integer
---| Thorium.Texture
---| Thorium.Bytedata
---| table
---| Thorium.Buffer

--- Sends a uniform value to the shader.
---@param ... string|Thorium.ShaderResource The name keys of the uniform followed by the values to send.
function Shader:send(...) end

--- Checks if a shader has a uniform.
---@param ... string The name keys of the uniform.
---@return boolean hasUniform True if the uniform exists, false otherwise.
function Shader:hasUniform(...) end

--- Gets all of the uniforms in the shader.
---@return table<string[], Thorium.ShaderResource> uniforms A table of uniform names to their values.
function Shader:getUniforms() end
