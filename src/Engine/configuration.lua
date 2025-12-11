function Thorium.config(config)
  print("Configuring...")
  config.window.width = 900
  config.window.height = 100

  config.window.title = "Configured Title"
  config.window.vsync = true

  config.filesystem.identity = "ConfiguredIdentity"

  -- "debug" > "info" > "warning" > "error" > "fatal"
  config.loglevel = ""
end
