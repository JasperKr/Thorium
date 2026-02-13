function Thorium.config(config)
  config.window.width = 1200
  config.window.height = 1000

  config.window.title = "Configured Title"
  config.window.resizable = true

  -- "linear" | "gammacorrect" | "hdr"
  config.window.colorspace = "linear"

  -- "adaptive" | "immediate" | "replace" | "enabled"
  ---@type Thorium.VsyncMode
  config.window.vsync = "adaptive"

  config.filesystem.identity = "ConfiguredIdentity"

  -- "debug" > "info" > "warning" > "error" > "fatal"
  config.loglevel = "warning"
end
