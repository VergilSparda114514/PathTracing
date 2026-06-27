project "Path Tracing"
   kind "ConsoleApp"
   language "C++"
   cppdialect "C++20"
   targetdir "bin/%{cfg.buildcfg}"
   staticruntime "off"

   files
   {
       "Source/**.h",
       "Source/**.cpp",
   }

   includedirs
   {
       "../vendor/GLFW/include",
       "../vendor/stb_image",
       "../vendor/tiny_obj_loader",
       "../vendor/imgui",
       "../vendor/yaml-cpp/include",
       
       "../PTCore/Source",
       
       "%{IncludeDir.VulkanSDK}",
       "%{IncludeDir.glm}",
   }

   links
   {
       "PTCore",
   }
   
   targetdir ("../bin/" .. outputdir .. "/%{prj.name}")
   objdir ("../bin-int/" .. outputdir .. "/%{prj.name}")

   filter "system:windows"
      systemversion "latest"
      defines { "WL_PLATFORM_WINDOWS" }
      buildoptions { "/utf-8" }

   filter "configurations:Debug"
      defines { "WL_DEBUG" }
      runtime "Debug"
      symbols "On"

   filter "configurations:Release"
      defines { "WL_RELEASE" }
      runtime "Release"
      optimize "On"
      symbols "On"

   filter "configurations:Dist"
      kind "WindowedApp"
      defines { "WL_DIST" }
      runtime "Release"
      optimize "On"
      symbols "Off"