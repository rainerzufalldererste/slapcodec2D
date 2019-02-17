ProjectName = "AdvancedDecoder"
project(ProjectName)

  --Settings
  kind "ConsoleApp"
  language "C++"
  flags { "StaticRuntime", "FatalWarnings" }
  dependson { "slapcodec2D" }

  buildoptions { '/Gm-' }
  buildoptions { '/MP' }
  ignoredefaultlibraries { "msvcrt" }

  filter {}
  defines { "_CRT_SECURE_NO_WARNINGS" }

  objdir "intermediate/obj"

  files { "src/**.c", "src/**.cpp", "src/**.h", "src/**.inl" }
  files { "project.lua" }

  includedirs { "../../slapcodec2D/include/**" }
  includedirs { "../../slapcodec2D/include" }
  includedirs { "../decoder/3rdParty/SDL2/include" }

  filter { "configurations:Release" }
    links { "../../slapcodec2D/lib/slapcodec2D.lib" }
  filter { "configurations:Debug" }
    links { "../../slapcodec2D/lib/slapcodec2DD.lib" }
  filter { }

  links { "../decoder/3rdParty/SDL2/lib/SDL2.lib" }
  links { "../decoder/3rdParty/SDL2/lib/SDL2main.lib" }
  postbuildcommands { "{COPY} ../decoder/3rdParty/SDL2/bin/ bin/" }
  
  filter { "configurations:Debug", "system:Windows" }
    ignoredefaultlibraries { "libcmt" }
  filter { }
  
  configuration { }
  
  targetname(ProjectName)
  targetdir "bin"
  debugdir "bin"
  
filter {}
configuration {}

warnings "Extra"

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
  optimize "Full"
  flags { "NoFramePointer", "NoBufferSecurityCheck" }
  symbols "On"

filter { "system:windows" }
	defines { "WIN32", "_WINDOWS" }
	links { "kernel32.lib", "user32.lib", "gdi32.lib", "winspool.lib", "comdlg32.lib", "advapi32.lib", "shell32.lib", "ole32.lib", "oleaut32.lib", "uuid.lib", "odbc32.lib", "odbccp32.lib" }

filter { "system:windows", "configurations:Release", "action:vs2012" }
	buildoptions { "/d2Zi+" }

filter { "system:windows", "configurations:Release", "action:vs2013" }
	buildoptions { "/Zo" }

filter { "system:windows", "configurations:Release" }
	flags { "NoIncrementalLink" }

filter {}
  flags { "NoFramePointer", "NoBufferSecurityCheck" }
