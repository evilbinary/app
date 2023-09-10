if #apps >0 then 
    for _, v in ipairs(apps) do
        includes("./"..v.."/xmake.lua")
    end
end