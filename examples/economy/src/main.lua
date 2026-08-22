local function balance(p)
    return p.data.balance or config.starting_balance
end

service("economy", {
    balance = balance,
    take = function(p, amount)
        if balance(p) < amount then return false end
        p.data.balance = balance(p) - amount
        return true
    end,
    give = function(p, amount) p.data.balance = balance(p) + amount end,
})

cmd("balance", function(src)
    src:send("@balance", { amount = balance(players.by_name(src.name)) })
end)

cmd("pay <who: word> <amount: int 1..1000000>", function(src, who, amount)
    local from, to = players.by_name(src.name), players.by_name(who)
    if not to then return src:send("@missing", { who = who }) end
    if balance(from) < amount then return src:send("@poor", { amount = balance(from) }) end
    from.data.balance = balance(from) - amount
    to.data.balance = balance(to) + amount
    src:send("@paid", { amount = amount, who = to.name })
    to:send("@received", { who = from.name, amount = amount })
end)

cmd("baltop", function(src)
    for i, row in ipairs(store.top("balance", 10)) do
        src:send("<gray>" .. i .. ". " .. row.name .. " " .. row.value)
    end
end)

placeholder("balance", function(p) return p and tostring(balance(p)) or "0" end)
