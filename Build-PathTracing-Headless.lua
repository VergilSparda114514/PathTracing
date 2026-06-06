IncludeDir = {}
IncludeDir["glm"] = "../vendor/glm"

group "Core"
    include "PTCore/Build-PTCore-Headless.lua"

    -- Optional modules
    if os.isfile("Modules/Walnut-Networking/Build-Walnut-Networking.lua") then
        include "Modules/Walnut-Networking/Build-Walnut-Networking.lua"
    end
group ""