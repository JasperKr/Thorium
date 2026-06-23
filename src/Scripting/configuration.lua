function snap.config(config)
  config.window.width = 1800
  config.window.height = 1200

  config.window.title = "Configured Title"
  config.window.resizable = true

  -- "linear" | "gammacorrect" | "hdr"
  config.window.colorspace = "gammacorrect"

  -- "adaptive" | "immediate" | "replace" | "enabled"
  ---@type snap.VsyncMode
  config.window.vsync = "enabled"

  -- "required", "optional", "disabled"
  config.graphics.hardwareRaytracing = "disabled"
  config.graphics.inlineRaytracing = "disabled"

  local sourceDir = snap.filesystem.getSourceDirectory()
  config.graphics.shaderIncludePaths = {
    sourceDir .. "Graphics/Shaders/",
    sourceDir .. "Graphics/Shaders/Lighting/",
    sourceDir .. "Graphics/Shaders/Lighting/Lights/",
  }

  config.filesystem.identity = "ConfiguredIdentity"

  -- "debug" > "info" > "warning" > "error" > "fatal"
  config.loglevel = "warning"
end
