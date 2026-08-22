# Reference plugins

Eight small plugins that between them touch most of the API. They are not toys kept in a
document: a test loads these exact files and plays them, so an API change that breaks one
breaks the build.

Drop any of them into `plugins/` and start the server.

| Plugin | Lines | What it shows |
|---|---|---|
| [economy](economy) | 35 | A service other plugins consume, per-player data, a leaderboard query, a placeholder |
| [claims](claims) | 27 | Cancelling an event with a reason, positions and chunks, JSON that survives a restart |
| [spleef](spleef) | 41 | A deciding handler and a watching one, filters, a soft dependency that may be absent |
| [warps](warps) | 26 | A menu drawn per player, click handlers, teleporting, counters kept per player |
| [guide](guide) | 29 | NPCs with a name, a look and a click, and per-player visibility |
| [shopkeeper](shopkeeper) | 23 | An NPC that opens a menu and charges through the economy service |
| [countdown](countdown) | 29 | Titles, sounds, a boss bar claimed by priority, and a loop that pauses |
| [leaderboard](leaderboard) | 27 | A hologram rewritten in place, a repeating task, a placeholder |

## The two worth reading first

**countdown** is the one that shows why the API looks like this. A countdown is a loop that
waits, so that is what it is:

```lua
while left > 0 do
    ...
    wait(20)
    left = left - 1
end
```

No scheduler, no state machine beside it, no split between what runs now and what runs later.

**claims** is the one that shows what cancelling means. The handler answers the question the
server is asking, and the refusal carries its own message:

```lua
on("block.break", function(e)
    if not mayBuild(e) then e:cancel("@protected", { who = ownerOf(e) }) end
end)
```

The block goes back on the client's screen and the player is told why, from three lines.
