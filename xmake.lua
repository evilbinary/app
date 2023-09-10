if #apps >0 then 
    for _, v in ipairs(apps) do
        includes("./"..v.."/xmake.lua")
    end
end

if #cpp_apps >0 then 
    for _, v in ipairs(cpp_apps) do
        includes("./"..v.."/xmake.lua")
    end
end