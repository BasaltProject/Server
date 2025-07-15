local board

watch("block.break", function(e)
    local p = e.player
    if p then p.data.blocks = (p.data.blocks or 0) + 1 end
end)

local function text()
    local lines = { t("title") }
    for rank, row in ipairs(store.top("blocks", config.places)) do
        lines[#lines + 1] = t("row", { rank = rank, name = row.name, value = row.value })
    end
    if #lines == 1 then lines[2] = t("empty") end
    return table.concat(lines, "\n")
end

on("server.load", function(e)
    local w = worlds.by_name(config.board.world) or worlds.default()
    board = w:hologram(text(), pos(w.name, config.board.x, config.board.y, config.board.z))
end)

-- The hologram is one entity that keeps its identity: the text changes, it is not rebuilt.
every(config.refresh, function()
    if board then board.text = text() end
end)

placeholder("mined", function(p) return tostring(p and p.data.blocks or 0) end)
