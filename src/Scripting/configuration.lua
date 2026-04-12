function snap.config(config)
  config.window.width = 1200
  config.window.height = 1000

  config.window.title = "Configured Title"
  config.window.resizable = true

  -- "linear" | "gammacorrect" | "hdr"
  config.window.colorspace = "gammacorrect"

  -- "adaptive" | "immediate" | "replace" | "enabled"
  ---@type snap.VsyncMode
  config.window.vsync = "immediate"

  config.graphics.hardwareRaytracing = "optional"
  config.graphics.inlineRaytracing = "optional"

  config.filesystem.identity = "ConfiguredIdentity"

  -- "debug" > "info" > "warning" > "error" > "fatal"
  config.loglevel = "warning"
end
