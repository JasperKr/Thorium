do
  require("init")
  local ffi = require("ffi")

  local lastDrawTime = 0
  local lastImDrawTime = 0
  local lastShownTime = 0
  local lastShownImDrawTime = 0
  local count = 0

  local commandBufferChannel, canStartChannel, scene, events = ...

  local cameraWidth = snap.graphics.getWidth() * 3 / 4
  local cameraHeight = snap.graphics.getHeight() * 3 / 4

  -- scene:newCamera(name, verticalFOV, width, height, near, far)
  local camera = scene:newCamera("main camera", 90, cameraWidth, cameraHeight, 0.01, 1000)

  local snapshot

  local function draw()
    snap.graphics.setCullMode("none")
    snap.graphics.setCullMode("none")
    snap.graphics.setWindingOrder("ccw")
    snap.graphics.setDepthMode("greater", true)
    camera:render(scene)
    snap.graphics.setShader()
    if Imgui.Begin("Viewport") then
      Imgui.Image(camera:getRendertarget("PostProcessed"), ffi.new("ImVec2", { cameraWidth, cameraHeight }))
    end
    Imgui.End()

    Editor.drawGUI()
    scene:drawUIElement();

    local startTime = snap.timer.getTime()


    Imgui.ShowDemoWindow()

    if snapshot then
      snapshot:draw()
    end

    snap.gui.endFrame()
    local imStartTime = snap.timer.getTime()

    ---@type snap.DetailedBlendMode
    local imguiBlendState = {
      alphaop = "add",
      colorop = "add",
      srcalpha = "srcalpha",
      srccolor = "srcalpha",
      dstalpha = "oneminussrcalpha",
      dstcolor = "oneminussrcalpha",
    }

    snap.graphics.setRenderTarget({ loadas = "clear", blendmode = imguiBlendState })
    snap.gui.draw()
    snap.graphics.setScissor();
    lastImDrawTime = lastImDrawTime + snap.timer.getTime() - imStartTime

    lastDrawTime = lastDrawTime + snap.timer.getTime() - startTime
    count = count + 1
    if (count >= 50) then
      lastShownTime = lastDrawTime / count
      lastShownImDrawTime = lastImDrawTime / count
      count = 0
      lastDrawTime = 0
      lastImDrawTime = 0
    end
  end

  local createSnapshot = false

  local isDown = {}
  function snap.mousepressed(x, y, button)
    isDown[button] = true
  end

  function snap.mousereleased(x, y, button)
    isDown[button] = false
  end

  function snap.keypressed(key)
    isDown[key] = true
    if key == "f5" then
      createSnapshot = true
    end
  end

  function snap.keyreleased(key)
    isDown[key] = false
  end

  function snap.mousemoved(x, y, dx, dy)
    if not isDown[3] then
      return
    end

    local quat = quaternion(snap.math.eulerToQuaternion(dx * 0.0015, -dy * 0.0015, 0))
    local currentQuat = quaternion(camera:getRotation())
    local newQuat = currentQuat * quat
    camera:setRotation(newQuat.x, newQuat.y, newQuat.z, newQuat.w)
  end

  local t = snap.timer.getTime()
  local deltaTimestamp = snap.timer.getTime()
  local firstFrame = true

  while true do
    if not (canStartChannel:demand(1)) then
      break
    end

    local delta = snap.timer.getTime() - deltaTimestamp
    deltaTimestamp = snap.timer.getTime()

    local event = events:pop()
    while event do
      if snap[event[1]] then
        snap[event[1]](unpack(event, 2))
      end

      event = events:pop()
    end

    local speed = delta * 10

    if (isDown["a"]) then
      local leftX, leftY, leftZ = camera:getInverseRight()
      leftX, leftY, leftZ = -leftX, -leftY, -leftZ
      local x, y, z = camera:getPosition()
      camera:setPosition(x + leftX * speed, y + leftY * speed, z + leftZ * speed)
    end

    if (isDown["d"]) then
      local rightX, rightY, rightZ = camera:getInverseRight()
      local x, y, z = camera:getPosition()
      camera:setPosition(x + rightX * speed, y + rightY * speed, z + rightZ * speed)
    end

    if (isDown["w"]) then
      local forwardX, forwardY, forwardZ = camera:getInverseForward()
      local x, y, z = camera:getPosition()
      camera:setPosition(x + forwardX * speed, y + forwardY * speed, z + forwardZ * speed)
    end

    if (isDown["s"]) then
      local backX, backY, backZ = camera:getInverseForward()
      backX, backY, backZ = -backX, -backY, -backZ
      local x, y, z = camera:getPosition()
      camera:setPosition(x + backX * speed, y + backY * speed, z + backZ * speed)
    end

    if createSnapshot then
      print("Requesting snapshot creation")
    end

    snap.graphics.aquireGraphics(nil, nil, createSnapshot)
    createSnapshot = false
    if firstFrame then
      snap.graphics.setDefaultFilter("linear", "linear", 4)
      snap.scene.loadModel(scene, "Assets/Terrain/sponza.glb")

      firstFrame = false
    end

    local dt = snap.timer.getTime() - t
    t = snap.timer.getTime()

    snap.gui.newFrame(dt)

    draw()

    local commands, newSnapshot = snap.graphics.submitGraphics()
    if newSnapshot then
      snapshot = newSnapshot
    end

    commandBufferChannel:push(commands)
  end

  print("THREAD #1 EXITING")
end

collectgarbage("collect")
collectgarbage("collect")
