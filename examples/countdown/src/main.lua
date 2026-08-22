local left, total = 0, 0

boss_bar{
    priority = config.bar_priority,
    when = function(p) return left > 0 end,
    title = function(p) return t("bar", { seconds = left }) end,
    lines = function(p) return { tostring(left / total) } end,
}

cmd("countdown [seconds: int 3..300]", function(src, seconds)
    if left > 0 then return src:send("@already") end
    total = seconds or config.seconds
    left = total
    -- A loop with a pause in it, not a scheduler with a state machine beside it.
    while left > 0 do
        if left <= 5 then
            for _, p in ipairs(players.online()) do
                p:title(t("tick", { n = left }), "", { fade_in = 0, stay = 25, fade_out = 5 })
                p:sound("minecraft:block.note_block.hat", 1, 1)
            end
        end
        wait(20)
        left = left - 1
    end
    for _, p in ipairs(players.online()) do
        p:title("@go", "@go_subtitle")
        p:sound("minecraft:entity.player.levelup", 1, 1)
    end
end, { permission = "countdown.start" })
