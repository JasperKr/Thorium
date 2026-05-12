function Imgui.ButtonRight(name, size)
  local style = Imgui.style
  local width = size and size.x or (Imgui.CalcTextSize(name).x + style.FramePadding.x * 2)
  local widthNeeded = width + style.ItemSpacing.x
  Imgui.SetCursorPosX(Imgui.GetCursorPos().x + Imgui.GetContentRegionAvail().x - widthNeeded)
  return Imgui.Button(name, size)
end

function Imgui.TextRight(name)
  local style = Imgui.style
  local width = Imgui.CalcTextSize(name).x + style.FramePadding.x * 2
  local widthNeeded = width + style.ItemSpacing.x
  Imgui.SetCursorPosX(Imgui.GetCursorPos().x + Imgui.GetContentRegionAvail().x - widthNeeded)
  return Imgui.Text(name)
end

function Imgui.TextCentered(name)
  local style = Imgui.style
  local width = Imgui.CalcTextSize(name).x + style.FramePadding.x * 2
  local widthNeeded = width + style.ItemSpacing.x
  Imgui.SetCursorPosX(Imgui.GetCursorPos().x + (Imgui.GetContentRegionAvail().x - widthNeeded) / 2)
  return Imgui.Text(name)
end

function snap.gui.InputFloat(name, value, step, stepFast, format, flags)
  value = snap.math.tempFloat1(value)
  local changed = Imgui.InputFloat(name, value, step, stepFast, format, flags)
  return value[0], changed
end

function snap.gui.InputInt(name, value, step, stepFast, flags)
  value = snap.math.tempInt1(value)
  local changed = Imgui.InputInt(name, value, step, stepFast, flags)
  return value[0], changed
end

function snap.gui.InputFloat2(name, value, step, stepFast)
  value = snap.math.tempFloat2(unpack(value))
  local changed = Imgui.InputFloat2(name, value, step, stepFast)
  return value[0], value[1], changed
end

function snap.gui.InputFloat3(name, value, step, stepFast)
  value = snap.math.tempFloat3(unpack(value))
  local changed = Imgui.InputFloat3(name, value, step, stepFast)
  return value[0], value[1], value[2], changed
end

function snap.gui.InputFloat4(name, value, step, stepFast)
  value = snap.math.tempFloat4(unpack(value))
  local changed = Imgui.InputFloat4(name, value, step, stepFast)
  return value[0], value[1], value[2], value[3], changed
end

function snap.gui.Checkbox(name, value)
  local temp = snap.math.tempBool1(value)
  local changed = Imgui.Checkbox(name, temp)
  return temp[0], changed
end

function snap.gui.DragFloat1(name, value, speed, min, max, format, flags)
  value = snap.math.tempFloat1(value)
  local changed = Imgui.DragFloat(name, value, speed, min, max, format, flags)
  return value[0], changed
end

function snap.gui.DragFloat2(name, value, speed, min, max, format, flags)
  value = snap.math.tempFloat2(unpack(value))
  local changed = Imgui.DragFloat2(name, value, speed, min, max, format, flags)
  return value[0], value[1], changed
end

function snap.gui.DragFloat3(name, value, speed, min, max, format, flags)
  value = snap.math.tempFloat3(unpack(value))
  local changed = Imgui.DragFloat3(name, value, speed, min, max, format, flags)
  return value[0], value[1], value[2], changed
end

function snap.gui.DragFloat4(name, value, speed, min, max, format, flags)
  value = snap.math.tempFloat4(unpack(value))
  local changed = Imgui.DragFloat4(name, value, speed, min, max, format, flags)
  return value[0], value[1], value[2], value[3], changed
end
