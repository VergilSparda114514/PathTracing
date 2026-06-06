workspace "Path Tracing"
   architecture "x64"
   configurations { "Debug", "Release", "Dist" }
   startproject "Path Tracing"

   -- Workspace-wide build options for MSVC
   filter "system:windows"
      buildoptions { "/EHsc", "/Zc:preprocessor", "/Zc:__cplusplus" }

outputdir = "%{cfg.buildcfg}-%{cfg.system}-%{cfg.architecture}"

include "Build-PathTracing-External.lua"
include "PTApp/Build-PTApp.lua"