if has_config("apps") then 
    for _, v in ipairs(apps) do
        includes("./"..v.."/xmake.lua")
    end
end

if has_config("cpp-apps") then 
    for _, v in ipairs(cpp_apps) do
        includes("./"..v.."/xmake.lua")
    end
end