---@param scene snap.Scene
function snap.renderer.renderScene(scene)

end

function snap.renderer.buildOpaqueCommandBuffer(scene)
  snap.graphics.aquireGraphics("opaque")

  snap.graphics.submitGraphics()
end
