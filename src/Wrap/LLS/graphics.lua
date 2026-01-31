---@meta Graphics

error("Do not require this file")

---@class Thorium.SimpleBlendMode
---@field blendmode "none"|"alpha"|"add"|"sub"|"mul" The blend mode to use.
---@field alphamode "alphamultiply"|"premultiplied" The alpha mode to use.

---@alias Thorium.BlendFactor
---| "zero" # (0, 0, 0, 0)
---| "one" # (1, 1, 1, 1)
---| "srccolor" # (Rs, Gs, Bs, As)
---| "oneminussrccolor" # (1 - Rs, 1 - Gs, 1 - Bs, 1 - As)
---| "dstcolor" # (Rd, Gd, Bd, Ad)
---| "oneminusdstcolor" # (1 - Rd, 1 - Gd, 1 - Bd, 1 - Ad)
---| "srcalpha" # (As, As, As, As)
---| "oneminussrcalpha" # (1 - As, 1 - As, 1 - As, 1 - As)
---| "dstalpha" # (Ad, Ad, Ad, Ad)
---| "oneminusdstalpha" # (1 - Ad, 1 - Ad, 1 - Ad, 1 - Ad)

---@alias Thorium.BlendOperation
---| "add" # Source + Destination
---| "sub" # Source - Destination
---| "revsub" # Destination - Source
---| "min" # Minimum of Source and Destination
---| "max" # Maximum of Source and Destination

---@class Thorium.DetailedBlendMode
---@field srccolor Thorium.BlendFactor The source color blend factor.
---@field dstcolor Thorium.BlendFactor The destination color blend factor.
---@field srcalpha Thorium.BlendFactor The source alpha blend factor.
---@field dstalpha Thorium.BlendFactor The destination alpha blend factor.
---@field colorop Thorium.BlendOperation The color blend operation.
---@field alphaop Thorium.BlendOperation The alpha blend operation.

---@alias Thorium.BlendMode
---| Thorium.SimpleBlendMode
---| Thorium.DetailedBlendMode

---@class Thorium.Color
---@field [1] number Red component (0-1)
---@field [2] number Green component (0-1)
---@field [3] number Blue component (0-1)
---@field [4] number Alpha component (0-1)

---@class Thorium.RenderTarget
---@field texture Thorium.Texture? The texture attached to the rendertarget.
---@field layer number? The layer of the texture to use.
---@field blendmode Thorium.BlendMode? The blend mode to use when rendering to this rendertarget.
---@field loadas Thorium.Color|"clear"|"load"|"none"|nil How to load the rendertarget before rendering to it.


--- Presents backbuffer and waits for vsync if enabled.
function Thorium.graphics.present() end

--- Pushes the current graphics state.
function Thorium.graphics.push() end

--- Pops the current graphics state.
function Thorium.graphics.pop() end

--- Resets the graphics state.
function Thorium.graphics.reset() end

---@alias Thorium.DepthMode
---| "never" # No depth testing.
---| "less" # Passes if the incoming depth value is less than the stored depth value.
---| "equal" # Passes if the incoming depth value is equal to the stored depth value.
---| "lequal" # Passes if the incoming depth value is less than or equal to the stored depth value.
---| "greater" # Passes if the incoming depth value is greater than the stored depth value.
---| "notequal" # Passes if the incoming depth value is not equal to the stored depth value.
---| "gequal" # Passes if the incoming depth value is greater than or equal to the stored depth value.
---| "always" # Always passes.

--- Sets the depth mode.
---@param mode Thorium.DepthMode The depth mode to set.
---@param write boolean Whether to write to the depth buffer.
function Thorium.graphics.setDepthMode(mode, write) end

---@alias Thorium.Cullmode
---| "none" # No culling.
---| "front" # Cull front faces.
---| "back" # Cull back faces.
---| "always" # Always cull.

--- Sets the cull mode.
---@param mode Thorium.Cullmode The cull mode to set.
function Thorium.graphics.setCullMode(mode) end

---@alias Thorium.PolygonMode
---| "fill" # Fill polygons.
---| "line" # Draw polygon edges as lines.
---| "point" # Draw polygon vertices as points.

--- Sets the polygon mode.
---@param mode Thorium.PolygonMode The polygon mode to set.
function Thorium.graphics.setPolygonMode(mode) end

--- Sets the current viewport.
---@param x number The x coordinate of the viewport.
---@param y number The y coordinate of the viewport.
---@param width number The width of the viewport.
---@param height number The height of the viewport.
---@param minDepth number The minimum depth of the viewport.
---@param maxDepth number The maximum depth of the viewport.
function Thorium.graphics.setViewport(x, y, width, height, minDepth, maxDepth) end

--- Sets the scissor rectangle.
---@param x number The x coordinate of the scissor rectangle.
---@param y number The y coordinate of the scissor rectangle.
---@param width number The width of the scissor rectangle.
---@param height number The height of the scissor rectangle.
function Thorium.graphics.setScissorRect(x, y, width, height) end

--- Clips the scissor rectangle.
---@param x number The x coordinate of the scissor rectangle.
---@param y number The y coordinate of the scissor rectangle.
---@param width number The width of the scissor rectangle.
---@param height number The height of the scissor rectangle.
function Thorium.graphics.clipScissorRect(x, y, width, height) end

--- Sets the shader used for rendering.
--- @param shader Thorium.Shader The shader to set.
function Thorium.graphics.setShader(shader) end

--- Sets the line width.
--- @param width number The line width to set.
function Thorium.graphics.setLineWidth(width) end

---@alias Thorium.WindingOrder
---| "cw" # Clockwise winding order.
---| "ccw" # Counter-clockwise winding order.

--- Sets the winding order for front face determination.
--- @param clockwise Thorium.WindingOrder The winding order to set.
function Thorium.graphics.setFrontFace(clockwise) end

--- Gets the depth mode.
--- @return Thorium.DepthMode depthmode The current depth mode.
function Thorium.graphics.getDepthMode() end

--- Gets the cull mode.
--- @return Thorium.Cullmode cullmode The current cull mode.
function Thorium.graphics.getCullMode() end

--- Gets the polygon mode.
--- @return Thorium.PolygonMode polygonmode The current polygon mode.
function Thorium.graphics.getPolygonMode() end

--- Gets the current viewport.
--- @return number x The x coordinate of the viewport.
--- @return number y The y coordinate of the viewport.
--- @return number width The width of the viewport.
--- @return number height The height of the viewport.
--- @return number minDepth The minimum depth of the viewport.
--- @return number maxDepth The maximum depth of the viewport.
function Thorium.graphics.getViewport() end

--- Gets the current scissor rectangle.
--- @return number x The x coordinate of the scissor rectangle.
--- @return number y The y coordinate of the scissor rectangle.
--- @return number width The width of the scissor rectangle.
--- @return number height The height of the scissor rectangle.
function Thorium.graphics.getScissorRect() end

--- Gets the currently set shader.
--- @return Thorium.Shader shader The currently set shader.
function Thorium.graphics.getShader() end

--- Gets the current line width.
--- @return number width The current line width.
function Thorium.graphics.getLineWidth() end

--- Gets the current winding order for front face determination.
--- @return Thorium.WindingOrder windingorder The current winding order.
function Thorium.graphics.getFrontFace() end

--- Gets the currently set rendertargets.
--- @return Thorium.RenderTarget ... The currently set rendertargets
function Thorium.graphics.getRenderTargets() end

--- Sets the rendertargets.
---@overload fun() Sets the backbuffer as the rendertarget.
---@overload fun(targets: Thorium.RenderTarget[]) Sets multiple rendertargets.
---@overload fun(target: Thorium.Texture) Sets a single texture as the rendertarget, using default settings.
---@overload fun(targets: Thorium.Texture[]) Sets multiple textures as rendertargets, using default settings.
---@param ... Thorium.RenderTarget The rendertargets to set.
function Thorium.graphics.setRenderTarget(...) end

--- Aquires a local graphics context for the current thread.
--- @param identifier string | integer The identifier of the graphics context to aquire.
--- @param priority number? The priority, Tie-breaker for commands with matching identifiers. Higher priority contexts are executed first.
function Thorium.graphics.aquireGraphics(identifier, priority) end

--- Submits the current thread's graphics commands.
function Thorium.graphics.submitGraphics() end

--- Uses commands created by aquire - submit graphics events.
--- @param identifier string | integer The identifier of the graphics context to use.
function Thorium.graphics.useGraphics(identifier) end

--- Draws the mesh or texture.
--- @param mesh Thorium.Mesh|Thorium.Texture The mesh or texture to draw.
function Thorium.graphics.draw(mesh, instanceCount) end

--- Draws the mesh using indirect parameters.
--- @param mesh Thorium.Mesh The mesh to draw.
--- @param indirectBuffer Thorium.Buffer The buffer containing the draw parameters.
--- @param offset number The offset in the buffer to read the parameters from.
--- @param count number The number of draws to perform.
function Thorium.graphics.drawIndirect(mesh, indirectBuffer, offset, count) end

--- Dispatches a compute shader.
--- @param threadgroupsX number The number of threadgroups to dispatch in the X dimension.
--- @param threadgroupsY number The number of threadgroups to dispatch in the Y dimension.
--- @param threadgroupsZ number The number of threadgroups to dispatch in the Z dimension.
function Thorium.graphics.dispatch(threadgroupsX, threadgroupsY, threadgroupsZ) end

--- Dispatches a compute shader using indirect parameters.
--- @param indirectBuffer Thorium.Buffer The buffer containing the dispatch parameters.
--- @param offset number The offset in the buffer to read the parameters from.
function Thorium.graphics.dispatchIndirect(indirectBuffer, offset) end

--- Clears the screen or the currently set rendertargets.
--- @overload fun(r: number, g: number, b: number, a: number)
--- @overload fun(color: boolean, depth: boolean, stencil: boolean)
--- @overload fun(targets: {[1]: {[1]: number, [2]: number, [3]: number, [4]: number}}, depth: number, stencil: number)
--- @param r number The red component of the clear color (0-1).
--- @param g number The green component of the clear color (0-1).
--- @param b number The blue component of the clear color (0-1).
--- @param a number The alpha component of the clear color (0-1).
--- @param depth number The depth value to clear to (0-1).
--- @param stencil number The stencil value to clear to (0-255).
function Thorium.graphics.clear(r, g, b, a, depth, stencil) end

---@alias Thorium.PrimitiveTopology
---| "points" # Points.
---| "lines" # Lines.
---| "linestrip" # Line strip.
---| "triangles" # Triangles.
---| "strip" # Triangle strip.

---@alias Thorium.VertexComponentFormat
---| "float"
---| "floatvec2"
---| "floatvec3"
---| "floatvec4"
---
---| "half"
---| "halfvec2"
---| "halfvec3"
---| "halfvec4"
---
---| "uint8"
---| "uint8vec2"
---| "uint8vec3"
---| "uint8vec4"
---
---| "uint16"
---| "uint16vec2"
---| "uint16vec3"
---| "uint16vec4"
---
---| "uint32"
---| "uint32vec2"
---| "uint32vec3"
---| "uint32vec4"
---
---| "int8"
---| "int8vec2"
---| "int8vec3"
---| "int8vec4"
---
---| "int16"
---| "int16vec2"
---| "int16vec3"
---| "int16vec4"
---
---| "int32"
---| "int32vec2"
---| "int32vec3"
---| "int32vec4"
---
---| "unorm8"
---| "unorm8vec2"
---| "unorm8vec3"
---| "unorm8vec4"
---
---| "unorm16"
---| "unorm16vec2"
---| "unorm16vec3"
---| "unorm16vec4"
---
---| "snorm8"
---| "snorm8vec2"
---| "snorm8vec3"
---| "snorm8vec4"
---
---| "snorm16"
---| "snorm16vec2"
---| "snorm16vec3"
---| "snorm16vec4"

-- I was hoping these descriptions would show up when hovering over the types in an IDE, but alas.
-- I'll leave them here for reference.

---@alias VertexAttributeName string Name for buffer formats

---@alias VertexAttributeLocation
---| integer Location index for vertex formats, must match vertex input struct definition order in shader code.

--- An object defining a vertex attribute.
---@alias Thorium.VertexAttribute
---| {name: VertexAttributeName, format: Thorium.VertexComponentFormat, location: VertexAttributeLocation}

--- An object defining a vertex format.
---@alias Thorium.VertexFormat
---| Thorium.VertexAttribute[] An array of vertex attributes defining the vertex format.

--- Creates a mesh.
--- @overload fun(vertexFormat: Thorium.VertexFormat, vertices: table, topology?: Thorium.PrimitiveTopology, indices?: table): Thorium.Mesh
--- @overload fun(vertexFormat: Thorium.VertexFormat, vertices: integer, topology?: Thorium.PrimitiveTopology, indices?: integer): Thorium.Mesh
--- @param vertexFormat Thorium.VertexFormat The vertex format of the mesh.
--- @param vertices Thorium.Bytedata The vertex data of the mesh.
--- @param topology Thorium.PrimitiveTopology? The primitive topology of the mesh. Defaults to "triangles".
--- @param indices Thorium.Bytedata? The index data of the mesh.
--- @return Thorium.Mesh mesh The created mesh.
function Thorium.graphics.newMesh(vertexFormat, vertices, topology, indices) end
