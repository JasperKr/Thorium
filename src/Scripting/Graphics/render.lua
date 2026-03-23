---@param scene snap.Scene
function snap.renderer.renderScene(scene)

end

local function itemIterator(source, getter, getCount)
  local count = getCount(source)
  local index = 0

  return function()
    index = index + 1
    if index <= count then
      local item = getter(source, index)
      return index, item
    end
  end
end

---@param scene snap.Scene
function snap.renderer.buildOpaqueCommandBuffer(scene)
  snap.graphics.aquireGraphics("opaque")
  for i, model in itemIterator(scene, scene.getHierarchyObject, scene.getHierarchyObjectCount) do
    ---@cast model snap.Model
    for j, shape in itemIterator(model, model.getShapeAt, model.getShapeCount) do
      ---@cast shape snap.Shape

      snap.graphics.draw(shape:getMesh())
    end
  end

  snap.graphics.submitGraphics()
end
