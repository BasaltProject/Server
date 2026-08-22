package org.basaltmc.server.plugins

import org.basaltmc.wire.io.FileIO
import kotlin.test.AfterTest
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFalse
import kotlin.test.assertNotNull
import kotlin.test.assertNull
import kotlin.test.assertTrue

/**
 * The object model from the Lua side: handles, not copies.
 *
 * The point of every one of these is that `p.health = p.health + 1` reads and writes the player who
 * is standing there, not a snapshot taken when the handler started.
 */
class PluginObjectModelTest {
    private val root = "build/test-data/objects-${nextId()}"
    private val services = TestServices(root)
    private val host = PluginHost(services)
    private val casey = TestPlayer("Casey").also { services.players.list += it }

    @AfterTest
    fun cleanUp() {
        deleteTree(root)
    }

    private fun plugin(main: String) {
        val folder = "${services.paths.packages}/objects"
        FileIO.createDirectories("$folder/src")
        FileIO.writeText("$folder/${PackageLayout.MANIFEST}", "id = \"objects\"\nversion = \"1.0.0\"\napi = 1\n")
        FileIO.writeText("$folder/src/main.lua", main)
        val report = host.loadAll()
        assertTrue(report.loaded.contains("objects"), report.refused.toString())
    }

    @Test
    fun `a player is read and written as properties`() {
        plugin(
            """
            cmd("hurt", function(src)
                local p = players.by_name("Casey")
                p.health = p.health - 5
                p.food = 17
                p.gamemode = "creative"
                p:send("<red>ouch")
            end)
            """.trimIndent(),
        )
        services.commands.run("hurt", emptyMap())
        assertEquals(15.0, casey.health)
        assertEquals(17, casey.food)
        assertEquals("creative", casey.gamemode)
        assertEquals(listOf("<red>ouch"), casey.received)
    }

    @Test
    fun `reading a name a player does not have is an error that lists what it has`() {
        plugin("""cmd("oops", function(src) local x = players.by_name("Casey").healthh end)""")
        services.commands.run("oops", emptyMap())
        assertEquals(1, host.statuses.single().errors)
    }

    @Test
    fun `what can be read but not written says so`() {
        plugin("""cmd("nope", function(src) players.by_name("Casey").name = "Someone" end)""")
        services.commands.run("nope", emptyMap())
        assertEquals(1, host.statuses.single().errors)
        assertEquals("Casey", casey.name)
    }

    @Test
    fun `a handle to somebody who left says so instead of lying`() {
        plugin(
            """
            local kept
            cmd("keep", function(src) kept = players.by_name("Casey") end)
            cmd("check", function(src) src:send(tostring(kept.online) .. "/" .. kept.name) end)
            """.trimIndent(),
        )
        services.commands.run("keep", emptyMap())
        casey.online = false
        services.commands.run("check", emptyMap())
        assertEquals(listOf("false/Casey"), services.commands.replies)
    }

    @Test
    fun `the methods of a player do what they say`() {
        plugin(
            """
            cmd("kit", function(src)
                local p = players.by_name("Casey")
                p:give("minecraft:bread", 3)
                p:teleport("world", 10, 65, 20)
                p:action_bar("<gray>ready")
                src:send(tostring(p:may("kit.use")))
            end)
            """.trimIndent(),
        )
        services.commands.run("kit", emptyMap())
        assertEquals(listOf("minecraft:bread" to 3), casey.given)
        assertEquals(10.0, casey.x)
        assertEquals(listOf("<gray>ready"), casey.actionBars)
        assertEquals(listOf("false"), services.commands.replies)
    }

    @Test
    fun `permissions come from the server not from a table of the plugin's own`() {
        casey.permissions += "kit.use"
        plugin("""cmd("may", function(src) src:send(tostring(players.by_name("Casey"):may("kit.use"))) end)""")
        services.commands.run("may", emptyMap())
        assertEquals(listOf("true"), services.commands.replies)
    }

    @Test
    fun `a world reads and writes its blocks`() {
        plugin(
            """
            cmd("build", function(src)
                local w = worlds.by_name("world")
                w:set_block(1, 2, 3, "minecraft:stone")
                src:send(w:block(1, 2, 3) .. "|" .. tostring(w:block(9, 9, 9)) .. "|" .. w.name)
            end)
            """.trimIndent(),
        )
        services.commands.run("build", emptyMap())
        assertEquals(listOf("minecraft:stone|nil|world"), services.commands.replies)
    }

    @Test
    fun `spawning goes through the world and hands back a handle`() {
        plugin(
            """
            cmd("spawn", function(src)
                local w = worlds.default()
                local cow = w:spawn("minecraft:cow", 1, 64, 1)
                cow.health = 5
                src:send(cow.type .. "|" .. cow.health .. "|" .. tostring(cow.alive) .. "|" .. #w:entities())
                cow:remove()
                src:send(tostring(cow.alive) .. "|" .. #w:entities())
            end)
            """.trimIndent(),
        )
        services.commands.run("spawn", emptyMap())
        // Health is a float in Lua because it is one in the game: half a heart is 1.0, not 1.
        assertEquals(listOf("minecraft:cow|5.0|true|1", "false|0"), services.commands.replies)
    }

    @Test
    fun `an event hands over the player as an object`() {
        plugin(
            """
            on("player.chat", function(e)
                e.player:send("you said " .. e.message)
                e.message = "[" .. e.player.name .. "] " .. e.message
            end)
            """.trimIndent(),
        )
        val chat = host.events.event(PluginEventTypes.PLAYER_CHAT)
        chat.set(0, "Casey")
        chat.set(1, casey.uuid)
        chat.set(2, "world")
        chat.set(3, "hello")
        host.events.fire(chat)
        assertEquals(listOf("you said hello"), casey.received)
        assertEquals("[Casey] hello", chat.get("message"))
    }

    @Test
    fun `a placeholder of a plugin is answered by the server's own registry`() {
        plugin(
            """
            local wins = 7
            placeholder("wins", function(p) return tostring(wins) .. ":" .. (p and p.name or "nobody") end)
            cmd("show", function(src) src:send(fill("<gray>%objects_wins%", players.by_name("Casey"))) end)
            """.trimIndent(),
        )
        services.commands.run("show", emptyMap())
        assertEquals(listOf("<gray>7:Casey"), services.commands.replies)
        // And the same identifier answers for nobody, which is what the server list renders with.
        assertEquals("7:nobody", services.placeholders.fill("%objects_wins%", null))
    }

    @Test
    fun `unloading takes the placeholder with it`() {
        plugin("""placeholder("wins", function(p) return "7" end)""")
        assertTrue(services.placeholders.registered.containsKey("objects_wins"))
        host.unload("objects")
        assertFalse(services.placeholders.registered.containsKey("objects_wins"))
    }

    @Test
    fun `broadcast reaches everyone online`() {
        val robin = TestPlayer("Robin").also { services.players.list += it }
        plugin("""cmd("shout", function(src) broadcast("<gold>hello everyone") end)""")
        services.commands.run("shout", emptyMap())
        assertEquals(listOf("<gold>hello everyone"), casey.received)
        assertEquals(listOf("<gold>hello everyone"), robin.received)
    }

    @Test
    fun `at moves the turn to the owner of a position`() {
        plugin(
            """
            cmd("far", function(src)
                at("world", 100, 200)
                worlds.by_name("world"):set_block(100, 64, 200, "minecraft:gold_block")
                src:send("done")
            end)
            """.trimIndent(),
        )
        services.commands.run("far", emptyMap())
        assertEquals(
            "minecraft:gold_block",
            services.worlds.list
                .first()
                .block(100, 64, 200),
        )
        assertEquals(listOf("done"), services.commands.replies)
    }

    @Test
    fun `at cannot be used where waiting is not allowed`() {
        plugin("""on("player.chat", function(e) at("world", 1, 1) end)""")
        val chat = host.events.event(PluginEventTypes.PLAYER_CHAT)
        chat.set(0, "Casey")
        chat.set(1, casey.uuid)
        chat.set(2, "world")
        chat.set(3, "hi")
        host.events.fire(chat)
        assertEquals(1, host.statuses.single().errors)
    }

    @Test
    fun `text is written with its arguments beside it not concatenated`() {
        plugin(
            """
            cmd("hi", function(src)
                local p = players.by_name("Casey")
                p:send("<gold>Welcome, {name}, you have {n} coins", { name = p.name, n = 12 })
            end)
            """.trimIndent(),
        )
        services.commands.run("hi", emptyMap())
        assertEquals(listOf("<gold>Welcome, Casey, you have 12 coins"), casey.received)
    }

    @Test
    fun `an argument that was not passed stays visible instead of vanishing`() {
        plugin("""cmd("hi", function(src) players.by_name("Casey"):send("hello {who}") end)""")
        services.commands.run("hi", emptyMap())
        assertEquals(listOf("hello {who}"), casey.received)
    }

    @Test
    fun `a key with an at in front is looked up in the plugin's own lang`() {
        val folder = "${services.paths.packages}/objects"
        FileIO.createDirectories("$folder/resources/lang")
        FileIO.writeText(
            "$folder/resources/lang/en_us.toml",
            "welcome = \"Hello {name}\"\n[arena]\nfull = \"Arena full\"\n",
        )
        plugin(
            """
            cmd("hi", function(src)
                local p = players.by_name("Casey")
                p:send("@welcome", { name = p.name })
                p:send("@arena.full")
                p:send("@nothing.here")
            end)
            """.trimIndent(),
        )
        services.commands.run("hi", emptyMap())
        // The missing one comes back as its own key: a gap on screen is a mystery, a key is a bug
        // report.
        assertEquals(listOf("Hello Casey", "Arena full", "nothing.here"), casey.received)
    }

    @Test
    fun `an inventory is read and written by slot`() {
        casey.inventory.slots[0] = PluginItem("minecraft:diamond", 3)
        plugin(
            """
            cmd("look", function(src)
                local inv = players.by_name("Casey").inventory
                local item = inv:get(0)
                src:send(item.id .. "x" .. item.count .. "/" .. inv.size .. "/" .. tostring(inv:get(5)))
                inv:set(1, { id = "minecraft:emerald", count = 2, name = "<green>Shiny" })
            end)
            """.trimIndent(),
        )
        services.commands.run("look")
        assertEquals(listOf("minecraft:diamondx3/41/nil"), services.commands.replies)
        assertEquals(PluginItem("minecraft:emerald", 2, name = "<green>Shiny"), casey.inventory.slots[1])
    }

    @Test
    fun `an item id on its own is an item`() {
        plugin("""cmd("give", function(src) players.by_name("Casey").inventory:add("minecraft:bread", 3) end)""")
        services.commands.run("give")
        assertEquals(PluginItem("minecraft:bread", 3), casey.inventory.slots[0])
    }

    @Test
    fun `add answers what did not fit and remove answers what it got`() {
        plugin(
            """
            cmd("fill", function(src)
                local inv = players.by_name("Casey").inventory
                src:send(tostring(inv:add({ id = "minecraft:stone", count = 5 })))
                src:send(tostring(inv:remove("minecraft:stone", 9)))
                src:send(tostring(inv:count("minecraft:stone")))
            end)
            """.trimIndent(),
        )
        services.commands.run("fill")
        // Five fit, so nothing was left over; nine were asked for and five were there.
        assertEquals(listOf("0", "5", "0"), services.commands.replies)
    }

    @Test
    fun `what is in hand is the hotbar slot in hand`() {
        casey.inventory.heldSlot = 2
        casey.inventory.slots[29] = PluginItem("minecraft:netherite_sword", 1, damage = 30)
        plugin(
            """
            cmd("hand", function(src)
                local inv = players.by_name("Casey").inventory
                src:send(inv.held.id .. "/" .. inv.held.damage .. "/" .. inv.held_slot)
            end)
            """.trimIndent(),
        )
        services.commands.run("hand")
        assertEquals(listOf("minecraft:netherite_sword/30/2"), services.commands.replies)
    }

    @Test
    fun `an item carries its name lore and enchantments`() {
        casey.inventory.slots[0] =
            PluginItem(
                "minecraft:diamond_sword",
                1,
                name = "<gold>Excalibur",
                lore = listOf("<gray>Once", "<gray>Twice"),
                enchantments = mapOf("minecraft:sharpness" to 5),
            )
        plugin(
            """
            cmd("read", function(src)
                local item = players.by_name("Casey").inventory:get(0)
                src:send(item.name .. "/" .. #item.lore .. "/" .. item.lore[2] ..
                    "/" .. item.enchantments["minecraft:sharpness"])
            end)
            """.trimIndent(),
        )
        services.commands.run("read")
        assertEquals(listOf("<gold>Excalibur/2/<gray>Twice/5"), services.commands.replies)
    }

    @Test
    fun `an item written without an id says so instead of landing empty`() {
        plugin("""cmd("bad", function(src) players.by_name("Casey").inventory:set(0, { count = 3 }) end)""")
        services.commands.run("bad")
        assertEquals(1, host.statuses.single().errors)
        assertNull(casey.inventory.slots[0])
    }

    @Test
    fun `clearing empties every slot`() {
        casey.inventory.slots[0] = PluginItem("minecraft:stone", 1)
        casey.inventory.slots[40] = PluginItem("minecraft:shield", 1)
        plugin("""cmd("wipe", function(src) players.by_name("Casey").inventory:clear() end)""")
        services.commands.run("wipe")
        assertTrue(casey.inventory.slots.all { it == null })
    }

    @Test
    fun `a position is where somebody was when you asked`() {
        casey.x = 100.5
        casey.y = 64.0
        casey.z = -33.25
        plugin(
            """
            cmd("where", function(src)
                local here = players.by_name("Casey").pos
                src:send(here.world .. " " .. here.x .. "," .. here.y .. "," .. here.z ..
                    " chunk " .. here.chunk_x .. "," .. here.chunk_z)
            end)
            """.trimIndent(),
        )
        services.commands.run("where")
        // Negative coordinates floor towards the ground, not towards zero: -33.25 is in chunk -3.
        assertEquals(listOf("world 100.5,64.0,-33.25 chunk 6,-3"), services.commands.replies)
    }

    @Test
    fun `offset and distance are the two things a plugin does with a position`() {
        plugin(
            """
            cmd("far", function(src)
                local home = pos("world", 0, 64, 0)
                local away = home:offset(3, 0, 4)
                src:send(away.x .. "," .. away.z .. " is " .. home:distance(away) .. " away")
            end)
            """.trimIndent(),
        )
        services.commands.run("far")
        assertEquals(listOf("3.0,4.0 is 5.0 away"), services.commands.replies)
    }

    @Test
    fun `a position goes wherever three numbers went`() {
        plugin(
            """
            cmd("build", function(src)
                local p = players.by_name("Casey")
                local w = worlds.by_name("world")
                local spot = pos("world", 7, 65, 9)
                w:set_block(spot, "minecraft:gold_block")
                p:teleport(spot)
                src:send(w:block(spot) .. " at " .. p.x .. "," .. p.z)
            end)
            """.trimIndent(),
        )
        services.commands.run("build")
        assertEquals(listOf("minecraft:gold_block at 7.0,9.0"), services.commands.replies)
    }

    @Test
    fun `a plain table with x y and z is a position too`() {
        plugin(
            """
            cmd("go", function(src)
                players.by_name("Casey"):teleport({ world = "world", x = 1, y = 2, z = 3 })
            end)
            """.trimIndent(),
        )
        services.commands.run("go")
        assertEquals(1.0, casey.x)
        assertEquals(3.0, casey.z)
    }

    @Test
    fun `spawning takes a position as readily as coordinates`() {
        plugin(
            """
            cmd("moo", function(src)
                local w = worlds.default()
                local cow = w:spawn("minecraft:cow", pos("world", 2, 64, 5))
                src:send(cow.pos.x .. "," .. cow.pos.z)
            end)
            """.trimIndent(),
        )
        services.commands.run("moo")
        assertEquals(listOf("2.0,5.0"), services.commands.replies)
    }

    @Test
    fun `an event that happened somewhere hands over the place`() {
        plugin(
            """
            local seen
            on("block.break", function(e) seen = e.pos.chunk_x .. "," .. e.pos.chunk_z end)
            cmd("where", function(src) src:send(seen) end)
            """.trimIndent(),
        )
        val broken = host.events.event(PluginEventTypes.BLOCK_BREAK)
        broken.set(0, "Casey")
        broken.set(1, casey.uuid)
        broken.set(2, "world")
        broken.set(3, 40)
        broken.set(4, 64)
        broken.set(5, -17)
        broken.set(6, "minecraft:stone")
        host.events.fire(broken)
        services.commands.run("where")
        assertEquals(listOf("2,-2"), services.commands.replies)
    }

    @Test
    fun `a title a sound and a particle are one call each`() {
        plugin(
            """
            cmd("show", function(src)
                local p = players.by_name("Casey")
                p:title("<gold>Round 1", "<gray>fight", { stay = 40 })
                p:sound("minecraft:entity.player.levelup", 1, 1.2)
                p:particle("minecraft:flame", p.pos:offset(0, 1, 0), 8, 0.25)
            end)
            """.trimIndent(),
        )
        services.commands.run("show")
        assertEquals(listOf("<gold>Round 1|<gray>fight|10/40/20"), casey.titles)
        assertEquals(listOf("minecraft:entity.player.levelup@1.0/1.2"), casey.sounds)
        assertEquals(listOf("minecraft:flame at 0.0,65.0,0.0 x8"), casey.particles)
    }

    @Test
    fun `a title on its own leaves the rest alone`() {
        plugin("""cmd("go", function(src) players.by_name("Casey"):title("@only") end)""")
        services.commands.run("go")
        assertEquals(listOf("only||10/70/20"), casey.titles)
    }

    @Test
    fun `a gamerule is the server's own and not a copy`() {
        plugin(
            """
            cmd("rules", function(src)
                local w = worlds.by_name("world")
                src:send(w:rule("doDaylightCycle"))
                src:send(tostring(w:rule("doDaylightCycle", false)))
                src:send(w:rule("doDaylightCycle"))
                src:send(tostring(w:rule("notARule")))
                src:send(tostring(#w.rules))
            end)
            """.trimIndent(),
        )
        services.commands.run("rules")
        assertEquals(listOf("true", "true", "false", "nil", "2"), services.commands.replies)
        assertEquals(
            "false",
            services.worlds.list
                .first()
                .rules["doDaylightCycle"],
        )
    }

    @Test
    fun `a rule this server does not have answers no rather than pretending`() {
        plugin("""cmd("set", function(src) src:send(tostring(worlds.default():rule("nope", true))) end)""")
        services.commands.run("set")
        assertEquals(listOf("false"), services.commands.replies)
    }

    @Test
    fun `an attribute is read and written by its registry name`() {
        plugin(
            """
            cmd("buff", function(src)
                local p = players.by_name("Casey")
                src:send(tostring(p:attribute("max_health")))
                src:send(tostring(p:attribute("max_health", 40)))
                src:send(tostring(p:attribute("minecraft:max_health")))
                src:send(tostring(p:attribute("not_an_attribute", 1)))
                src:send(tostring(#p.attributes))
            end)
            """.trimIndent(),
        )
        services.commands.run("buff")
        assertEquals(listOf("20.0", "true", "40.0", "false", "3"), services.commands.replies)
        assertEquals(40.0, casey.attributeBases["minecraft:max_health"])
    }

    @Test
    fun `a plugin can make a world of its own`() {
        plugin(
            """
            cmd("build", function(src)
                local made = worlds.create{ name = "arena", dimension = "overworld", type = "flat", seed = 7 }
                src:send(made.name .. "/" .. #worlds.all())
                src:send(tostring(worlds.create{ name = "arena" }))
                src:send(tostring(worlds.create{ name = "other", dimension = "moon" }))
            end)
            """.trimIndent(),
        )
        services.commands.run("build")
        // Made once, refused the second time by name, and refused for a dimension that is not one.
        assertEquals(listOf("arena/2", "nil", "nil"), services.commands.replies)
        assertNotNull(services.worlds.byName("arena"))
    }

    @Test
    fun `a hologram is floating text somebody can be shown or not`() {
        val robin = TestPlayer("Robin").also { services.players.list += it }
        plugin(
            """
            local sign
            cmd("put", function(src)
                local w = worlds.by_name("world")
                sign = w:hologram("<gold>Shop", pos("world", 1, 70, 2))
                src:send(sign.text .. "/" .. sign.type .. "/" .. sign.y .. "/" .. tostring(sign.audience))
            end)
            cmd("mine", function(src)
                sign.text = "<red>Closed"
                sign:show_only_to({ players.by_name("Casey") })
                src:send(sign.text .. "/" .. #sign.audience .. "/" .. sign.audience[1])
            end)
            cmd("all", function(src)
                sign:show_to_all()
                src:send(tostring(sign.audience))
            end)
            """.trimIndent(),
        )
        services.commands.run("put")
        assertEquals(listOf("<gold>Shop/minecraft:text_display/70.0/nil"), services.commands.replies)
        services.reset()
        services.commands.run("mine")
        assertEquals(listOf("<red>Closed/1/${casey.uuid}"), services.commands.replies)
        services.reset()
        services.commands.run("all")
        assertEquals(listOf("nil"), services.commands.replies)
        assertEquals("Robin", robin.name)
    }

    @Test
    fun `an npc is a player standing there and it answers a click`() {
        plugin(
            """
            local bob
            cmd("hire", function(src)
                bob = worlds.default():npc{
                    name = "Bob",
                    pos = pos("world", 5, 64, 5),
                    skin = { texture = "abc", signature = "sig" },
                    on_click = function(p) p:send("hello " .. p.name) end,
                }
                bob:look(90, 0)
                src:send(bob.name .. "/" .. bob.x .. "/" .. tostring(bob.alive))
            end)
            cmd("fire", function(src)
                bob:remove()
                src:send(tostring(bob.alive))
            end)
            """.trimIndent(),
        )
        services.commands.run("hire")
        assertEquals(listOf("Bob/5.0/true"), services.commands.replies)
        val bob =
            services.worlds.list
                .first()
                .npcs
                .single()
        assertEquals("abc", bob.skinTexture)
        assertEquals(90f, bob.yaw)

        bob.click(casey)
        assertEquals(listOf("hello Casey"), casey.received)

        services.reset()
        services.commands.run("fire")
        assertEquals(listOf("false"), services.commands.replies)
    }

    @Test
    fun `an npc without a position says so instead of standing at zero`() {
        plugin("""cmd("bad", function(src) worlds.default():npc{ name = "Nowhere" } end)""")
        services.commands.run("bad")
        assertEquals(1, host.statuses.single().errors)
    }

    private companion object {
        var counter = 0

        fun nextId(): Int = ++counter

        fun deleteTree(path: String) {
            if (FileIO.directoryExists(path)) {
                for (name in FileIO.list(path)) deleteTree("$path/$name")
            }
            FileIO.delete(path)
        }
    }
}
