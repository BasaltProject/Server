local claims = data.json("claims")

local function key(at) return at.world .. ":" .. at.chunk_x .. ":" .. at.chunk_z end
local function ownerOf(e) return claims[key(e.pos)] end
local function mayBuild(e)
    local owner = ownerOf(e)
    return owner == nil or owner == e.player_name
end

on("block.break", function(e) if not mayBuild(e) then e:cancel("@protected", { who = ownerOf(e) }) end end)
on("block.place", function(e) if not mayBuild(e) then e:cancel("@protected", { who = ownerOf(e) }) end end)

cmd("claim", function(src)
    local at = key(players.by_name(src.name).pos)
    if claims[at] then return src:send("@taken", { who = claims[at] }) end
    claims[at] = src.name
    data.save("claims")
    src:send("@claimed")
end)

cmd("unclaim", function(src)
    local at = key(players.by_name(src.name).pos)
    if claims[at] ~= src.name then return src:send("@not_yours") end
    claims[at] = nil
    data.save("claims")
    src:send("@unclaimed")
end)
