solution "slapcodec2D"
  
  editorintegration "On"
  configurations { "Debug", "Release" }
  platforms { "x64" }

  dofile "slapcodec2D/project.lua"

  group "examples"
    dofile "examples/Decoder/project.lua"
    dofile "examples/Encoder/project.lua"