VULKAN_SDK = os.getenv("VULKAN_SDK")

IncludeDir = {}
IncludeDir["VulkanSDK"] = "%{VULKAN_SDK}/Include"
IncludeDir["glm"] = "../vendor/glm"

LibraryDir = {}
LibraryDir["VulkanSDK"] = "%{VULKAN_SDK}/Lib"

Library = {}
Library["Vulkan"] = "%{LibraryDir.VulkanSDK}/vulkan-1.lib"

group "Dependencies"
   include "vendor/imgui"
   include "vendor/glfw"
group ""

group "Core"
    include "PTCore/Build-PTCore.lua"

    -- Optional modules
    if os.isfile("Modules/Walnut-Networking/Build-Walnut-Networking.lua") then
        include "Modules/Walnut-Networking/Build-Walnut-Networking.lua"
    end
group ""