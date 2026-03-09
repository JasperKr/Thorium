---@meta Mesh

error("Do not require this file")

---@class snap.Transform
Transform = {}

--- Sets the world position.
---@param x number
---@param y number
---@param z number
function Transform:setPosition(x, y, z) end

--- Gets the world position.
---@return number x
---@return number y
---@return number z
function Transform:getPosition() end

--- Sets the rotation.
---@param x number
---@param y number
---@param z number
---@param w number
function Transform:setRotation(x, y, z, w) end

--- Gets the rotation.
---@return number x
---@return number y
---@return number z
---@return number w
function Transform:getRotation() end

--- Sets the scale.
---@param x number
---@param y number
---@param z number
function Transform:setScale(x, y, z) end

--- Gets the scale.
---@return number x
---@return number y
---@return number z
function Transform:getScale() end

---@alias snap.SceneObject snap.Node | snap.Model | snap.Shape

---@class snap.Node : snap.Transform
Node = {}

--- Adds a child to this node
---@param child snap.SceneObject
function Node:addChild(child) end

--- Gets a child at the specified index
---@return snap.SceneObject child
---@param index number
function Node:getChild(index) end

--- Removes a child at the specified index
---@param index number
function Node:removeChild(index) end

--- Gets the children of this node
---@return snap.SceneObject[] children
function Node:getChildren() end

--- Sets the name of this node
---@param name string
function Node:setName(name) end

--- Gets the name of this node
---@return string name
function Node:getName() end

---Sets userdata for this node
---@param userdata any
function Node:setUserdata(userdata) end

--- Gets the userdata for this node
---@return any userdata
function Node:getUserdata() end

---@class snap.Model : snap.Transform
Model = {}

--- Adds a shape to this model
---@param shape snap.Shape
function Model:addShape(shape) end

--- Gets a shape at the specified index
---@return snap.Shape shape
---@param index number
function Model:getShape(index) end

--- Removes a shape at the specified index
--- @param index number
function Model:removeShape(index) end

--- Gets the shapes of this model
---@return snap.Shape[] shapes
function Model:getShapes() end

--- Sets the name of this model
---@param name string
function Model:setName(name) end

--- Gets the name of this model
---@return string name
function Model:getName() end

---@class snap.Shape : snap.Transform
Shape = {}
