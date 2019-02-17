solution "slapcodec2D"
  
  editorintegration "On"
  configurations { "Debug", "Release" }
  platforms { "x64" }

  dofile "slapcodec2D/project.lua"

  group "examples"
    dofile "examples/advancedDecoder/project.lua"
    dofile "examples/decoder/project.lua"
    dofile "examples/encoder/project.lua"