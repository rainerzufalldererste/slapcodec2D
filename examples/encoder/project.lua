ProjectName = "Encoder"
project(ProjectName)

  --Settings
  kind "ConsoleApp"
  language "C++"
  flags { "StaticRuntime", "FatalWarnings" }
  dependson { "slapcodec2D" }

  buildoptions { '/Gm-' }
  buildoptions { '/MP' }
  linkoptions { '/ignore:4006' } -- ignore multiple libraries defining the same symbol

  ignoredefaultlibraries { "msvcrt" }
  
  filter { }
  
  defines { "_CRT_SECURE_NO_WARNINGS", "SSE2" }
  
  objdir "intermediate/obj"

  files { "src/**.c", "src/**.cpp", "src/**.h", "src/**.inl", "src/**rc" }
  files { "include/**.cpp", "include/**.h", "include/**.inl", "src/**rc" }
  files { "project.lua" }
  
  includedirs { "include" }
  includedirs { "include/**" }
  includedirs { "stb", "3rdParty/ffmpeg/include", "../../slapcodec2D/include" }

  filter { "configurations:Debug", "system:Windows" }
    ignoredefaultlibraries { "libcmt" }
  filter { }
  
  targetname(ProjectName)
  targetdir "bin"
  debugdir "bin"
  
filter {}
configuration {}

links { "3rdParty/ffmpeg/lib/avcodec.lib", "3rdParty/ffmpeg/lib/avutil.lib", "3rdParty/ffmpeg/lib/avformat.lib" }

filter { "configurations:Release" }
  links { "../../slapcodec2D/lib/slapcodec2D.lib" }
filter { "configurations:Debug" }
  links { "../../slapcodec2D/lib/slapcodec2DD.lib" }
filter { }

postbuildcommands { "{COPY} 3rdParty/ffmpeg/bin bin" }

warnings "Extra"

filter {}
  targetname "%{prj.name}"

flags { "NoMinimalRebuild", "NoPCH" }
exceptionhandling "Off"
rtti "Off"
floatingpoint "Fast"

filter { "configurations:Debug*" }
	defines { "_DEBUG" }
	optimize "Off"
	symbols "On"

filter { "configurations:Release" }
	defines { "NDEBUG" }
	optimize "Speed"
	flags { "NoFramePointer", "NoBufferSecurityCheck" }
	symbols "On"

filter { "system:windows", "configurations:Release", "action:vs2012" }
	buildoptions { "/d2Zi+" }

filter { "system:windows", "configurations:Release", "action:vs2013" }
	buildoptions { "/Zo" }

filter { "system:windows", "configurations:Release" }
	flags { "NoIncrementalLink" }

filter {}
  flags { "NoFramePointer", "NoBufferSecurityCheck" }
