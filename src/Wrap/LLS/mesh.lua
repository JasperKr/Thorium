---@meta Mesh

error("Do not require this file")

---@class Thorium.Mesh : Thorium.Data
local Mesh = {}

--- Sets the vertices of the mesh.
---@param vertices Thorium.Bytedata The vertex data.
---@param offset number|nil The offset in the vertex data to start from.
---@param range number|nil The number of vertices to set.
function Mesh:setVertices(vertices, offset, range) end

--- Sets the indices of the mesh.
---@param indices Thorium.Bytedata The index data.
---@param offset number|nil The offset in the index data to start from.
---@param range number|nil The number of indices to set.
function Mesh:setIndices(indices, offset, range) end

--- Sets the vertex buffer used by the mesh.
---@param buffer Thorium.Buffer The vertex buffer.
function Mesh:setVertexBuffer(buffer) end

--- Sets the index buffer used by the mesh.
---@param buffer Thorium.Buffer The index buffer.
function Mesh:setIndexBuffer(buffer) end

--- Sets the draw range of the mesh.
---@param offset number The offset to start drawing from.
---@param range number The number of vertices or indices to draw.
function Mesh:setDrawRange(offset, range) end

--- Getst the draw range of the mesh.
---@return number offset The offset to start drawing from.
---@return number range The number of vertices or indices to draw.
function Mesh:getDrawRange() end

--- Gets the number of vertices in the mesh.
---@return number count The number of vertices.
function Mesh:getVertexCount() end

--- Gets the number of indices in the mesh. Vertex count if no indices are set.
---@return number count The number of indices.
function Mesh:getIndexCount() end
