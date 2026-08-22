local playing, floor = {}, {}

local function left()
    local n, last = 0, nil
    for name in pairs(playing) do n, last = n + 1, name end
    return n, last
end

on("block.break", function(e)
    if not playing[e.player_name] then return end
    floor[#floor + 1] = { world = e.world, x = e.x, y = e.y, z = e.z, block = e.block }
end)

watch("player.move", { world = config.arena.world }, function(e)
    if not playing[e.player_name] or e.y > config.arena.floor_y - 2 then return end
    playing[e.player_name] = nil
    broadcast("@lost", { who = e.player_name })
    if left() < 2 then finish() end
end)

function finish()
    local _, winner = left()
    if winner then
        broadcast("@won", { who = winner })
        local bank = service("economy")
        if bank then bank.give(players.by_name(winner), config.arena.prize) end
    end
    for _, block in ipairs(floor) do
        worlds.by_name(block.world):set_block(block.x, block.y, block.z, block.block)
    end
    playing, floor = {}, {}
end

cmd("spleef join", function(src)
    local p = players.by_name(src.name)
    playing[p.name] = true
    p:teleport(config.arena.world, config.arena.x, config.arena.y, config.arena.z)
    broadcast("@joined", { who = p.name })
end)

cmd("spleef stop", function(src) finish() end, { permission = "spleef.admin" })
