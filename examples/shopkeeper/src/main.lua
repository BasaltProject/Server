local shop = menu{ title = "@title", rows = 3 }

for slot, entry in pairs(config.stock) do
    shop:set(tonumber(slot), { id = entry.item, lore = { t("price", { price = entry.price }) } }, function(e)
        local bank = service("economy")
        if not bank then return e.player:send("@no_economy") end
        if not bank.take(e.player, entry.price) then return e.player:send("@poor") end
        e.player.inventory:add(entry.item)
        e.player:send("@bought", { what = entry.item })
        e.player:sound("minecraft:entity.villager.yes", 1, 1)
    end)
end

on("server.load", function(e)
    local w = worlds.by_name(config.keeper.world) or worlds.default()
    w:npc{
        name = config.keeper.name,
        pos = pos(w.name, config.keeper.x, config.keeper.y, config.keeper.z),
        on_click = function(p) shop:open(p) end,
    }
end)

cmd("shop", function(src) shop:open(players.by_name(src.name)) end)
