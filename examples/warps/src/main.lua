local warps = menu{ title = "@title", rows = config.rows }

local function icon(p, slot, warp)
    local visits = p.data["visits_" .. slot] or 0
    return { id = warp.icon, name = warp.name, count = math.max(visits, 1), lore = { t("visits", { count = visits }) } }
end

local function draw(p)
    for slot, warp in pairs(config.warp) do
        warps:set(tonumber(slot), icon(p, slot, warp), function(e)
            local w = worlds.by_name(warp.world ~= "" and warp.world or worlds.default().name)
            if not w then return e.player:send("@missing") end
            e.player.data["visits_" .. slot] = (e.player.data["visits_" .. slot] or 0) + 1
            e.player:teleport(pos(w.name, warp.x, warp.y, warp.z))
            e.player:send("@went", { name = warp.name })
            warps:close(e.player)
        end)
    end
end

cmd("warps", function(src)
    local p = players.by_name(src.name)
    -- Drawn for whoever opens it, so the counters on the icons are theirs.
    draw(p)
    warps:open(p)
end)
