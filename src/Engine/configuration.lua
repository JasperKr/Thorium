function Configuration(config)
  print("Configuring...")
  config.window.width = 900
  config.window.height = 100

  config.window.title = "Configured Title"
  config.window.vsync = true

  config.filesystem.identity = "ConfiguredIdentity"
end
