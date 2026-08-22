local standing = {}

on("server.load", function(e)
    for _, entry in pairs(config.guide) do
        local w = worlds.by_name(entry.world) or worlds.default()
        local guide = w:npc{
            name = entry.name,
            pos = pos(w.name, entry.x, entry.y, entry.z),
            skin = config.skin.texture ~= "" and config.skin or nil,
            on_click = function(p)
                p:send("@" .. entry.says, { name = p.name })
                p:sound("minecraft:entity.villager.ambient", 1, 1)
            end,
        }
        guide:look(entry.yaw, 0)
        standing[#standing + 1] = guide
    end
end)

-- Nobody sees them until you ask for them back, which is per player and not per server.
cmd("guides hide", function(src)
    for _, guide in ipairs(standing) do guide:show_only_to({}) end
    src:send("@hidden")
end)

cmd("guides show", function(src)
    for _, guide in ipairs(standing) do guide:show_to_all() end
    src:send("@shown")
end)
