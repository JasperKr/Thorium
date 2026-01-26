function Thorium.config(config)
  config.window.width = 1200
  config.window.height = 1000

  config.window.title = "Configured Title"
  config.window.vsync = true

  config.filesystem.identity = "ConfiguredIdentity"

  -- "debug" > "info" > "warning" > "error" > "fatal"
  config.loglevel = "warning"
end
