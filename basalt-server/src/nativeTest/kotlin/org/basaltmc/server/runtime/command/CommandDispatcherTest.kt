package org.basaltmc.server.runtime.command

import org.basaltmc.server.runtime.config.GroupsConfig
import org.basaltmc.server.runtime.config.PermissionGroup
import org.basaltmc.server.runtime.config.PermissionRule
import org.basaltmc.server.runtime.player.PlayerRegistry
import org.basaltmc.server.runtime.player.PlayerSession
import org.basaltmc.wire.protocol.play.CommandNodeData
import org.basaltmc.wire.protocol.play.Gamemode
import org.basaltmc.wire.text.Component
import org.basaltmc.wire.text.PlainTextSerializer
import kotlin.test.Test
import kotlin.test.assertContains
import kotlin.test.assertEquals
import kotlin.test.assertFailsWith
import kotlin.test.assertTrue

/**
 * A console-shaped source that keeps what it was told, so tests can assert on the feedback, and
 * holds a fixed permission set, so each test states exactly what it may do.
 */
private class RecordingSource(
    private val permissions: Set<String> = setOf("*"),
) : CommandSource {
    val messages = mutableListOf<String>()

    override val permissionLevel: Int = PermissionLevel.OWNERS

    override fun hasPermission(node: String?): Boolean =
        node != null &&
            permissions.any { it == "*" || it == node || (it.endsWith("*") && node.startsWith(it.dropLast(1))) }

    override val name: String = "Tester"
    override val displayName: Component = Component.text("Tester")
    override val position: Triple<Double, Double, Double> = Triple(100.0, 64.0, -50.0)
    override val player: PlayerSession? = null

    override fun sendSystemMessage(message: Component) {
        messages += PlainTextSerializer.serialize(message)
    }
}

/** Two groups, matching the shape the generated groups.toml ships with. */
internal fun testGroups(
    defaultPermissions: List<String> = listOf("basalt.command.help", "basalt.command.me"),
    operatorPermissions: List<String> = listOf("*"),
): GroupsConfig {
    val default =
        PermissionGroup(
            "default",
            defaultPermissions.map(PermissionRule::parse),
            inherits = null,
            isDefault = true,
            isOperator = false,
        )
    val operator =
        PermissionGroup(
            "operator",
            operatorPermissions.map(PermissionRule::parse),
            inherits = null,
            isDefault = false,
            isOperator = true,
        )
    return GroupsConfig(mapOf("default" to default, "operator" to operator), default, operator)
}

private fun testEnvironment(): CommandEnvironment =
    CommandEnvironment(
        players = PlayerRegistry(),
        groupsProvider = { testGroups() },
        worldSeed = 12345L,
        maxPlayers = 20,
    )

class CommandDispatcherTest {
    @Test
    fun runsALiteralCommandAndItsArguments() {
        val dispatcher = CommandDispatcher(testEnvironment())
        var seen: Int? = null
        dispatcher.register("count") {
            argument("amount", IntegerArgument(min = 0, max = 10)) {
                executes { context ->
                    seen = context.get("amount")
                    1
                }
            }
        }

        assertEquals(1, dispatcher.execute(RecordingSource(), "count 7"))
        assertEquals(7, seen)
    }

    @Test
    fun backtracksOutOfABranchThatCannotConsumeTheWholeLine() {
        // The shape /teleport <destination> versus /teleport <targets> <location>: the first branch
        // parses the same first word and only fails on what follows it.
        val dispatcher = CommandDispatcher(testEnvironment())
        val taken = mutableListOf<String>()
        dispatcher.register("teleport") {
            argument("destination", WordArgument) {
                executes {
                    taken += "destination"
                    1
                }
            }
            argument("targets", WordArgument) {
                argument("location", Vec3Argument) {
                    executes {
                        taken += "targets+location"
                        1
                    }
                }
            }
        }

        dispatcher.execute(RecordingSource(), "teleport Steve")
        dispatcher.execute(RecordingSource(), "teleport Steve 1 2 3")

        assertEquals(listOf("destination", "targets+location"), taken)
    }

    @Test
    fun reportsAnUnknownCommandWithBrigadiersOwnWordingAndTheContextLine() {
        val dispatcher = CommandDispatcher(testEnvironment())
        val source = RecordingSource()

        assertEquals(0, dispatcher.execute(source, "frobnicate"))

        assertEquals(2, source.messages.size)
        assertEquals("Unknown command", source.messages[0])
        // Serialized plainly, so the translation key is what shows: the client renders it as `<--[HERE]`.
        assertContains(source.messages[1], "command.context.here")
    }

    @Test
    fun aCommandThatThrowsIsReportedAndDoesNotEscape() {
        val dispatcher = CommandDispatcher(testEnvironment())
        dispatcher.register("boom") { executes { error("deliberate") } }
        val source = RecordingSource()

        assertEquals(0, dispatcher.execute(source, "boom"))

        assertEquals(listOf("command.failed"), source.messages)
    }

    @Test
    fun aCommandAboveTheSourcesLevelIsNotJustRefusedButUnknown() {
        val dispatcher = CommandDispatcher(testEnvironment())
        dispatcher.register("stop") {
            permission("basalt.command.stop")
            executes { 1 }
        }
        val source = RecordingSource(permissions = emptySet())

        assertEquals(0, dispatcher.execute(source, "stop"))

        assertEquals("Unknown command", source.messages.first())
    }

    @Test
    fun theTreeSentToAClientIsFilteredByPermission() {
        val dispatcher = CommandDispatcher(testEnvironment())
        dispatcher.register("me") {
            permission("basalt.command.me")
            executes { 1 }
        }
        dispatcher.register("stop") {
            permission("basalt.command.stop")
            executes { 1 }
        }

        val everyone = dispatcher.treeFor(RecordingSource(permissions = setOf("basalt.command.me")))
        val operator = dispatcher.treeFor(RecordingSource(permissions = setOf("*")))

        assertEquals(listOf("me"), everyone.nodes.mapNotNull { it.name }.filter { it.isNotEmpty() })
        assertTrue(operator.nodes.any { it.name == "stop" })
        assertEquals(CommandNodeData.Kind.ROOT, everyone.nodes[everyone.rootIndex].kind)
    }

    @Test
    fun anAliasRedirectsOntoItsTargetInTheTreeAndWhenRun() {
        val dispatcher = CommandDispatcher(testEnvironment())
        var ran = 0
        val target =
            dispatcher.register("teleport") {
                argument("amount", IntegerArgument()) {
                    executes {
                        ran++
                        1
                    }
                }
            }
        dispatcher.registerAlias("tp", target)

        dispatcher.execute(RecordingSource(), "tp 3")
        assertEquals(1, ran)

        val tree = dispatcher.treeFor(RecordingSource())
        val alias = tree.nodes.single { it.name == "tp" }
        assertEquals(tree.nodes.indexOfFirst { it.name == "teleport" }, alias.redirect)
    }

    @Test
    fun integerBoundsAreEnforcedWithBrigadiersOwnMessage() {
        val dispatcher = CommandDispatcher(testEnvironment())
        dispatcher.register("level") {
            argument("amount", IntegerArgument(min = 0, max = 5)) { executes { 1 } }
        }
        val source = RecordingSource()

        dispatcher.execute(source, "level 9")

        assertEquals("Integer must not be more than 5, found 9", source.messages.first())
    }

    @Test
    fun registeringACommandObjectCarriesItsNamePermissionAndAliases() {
        // The contract a BSMod command will be written against: declare the three, fill in build().
        val dispatcher = CommandDispatcher(testEnvironment())
        var ran = 0
        dispatcher.register(
            object : Command(name = "warp", permissionLevel = PermissionLevel.GAMEMASTERS, aliases = listOf("w")) {
                override fun build(root: CommandNodeBuilder) {
                    root.argument("amount", IntegerArgument()) {
                        executes {
                            ran++
                            1
                        }
                    }
                }
            },
        )

        assertEquals(1, dispatcher.execute(RecordingSource(), "warp 3"))
        assertEquals(1, dispatcher.execute(RecordingSource(), "w 3"))
        assertEquals(2, ran)

        // Sub-commands inherit the command's permission, so someone without it sees neither form.
        val outsider = RecordingSource(permissions = setOf("basalt.command.other"))
        assertEquals(0, dispatcher.execute(outsider, "warp 3"))
        assertEquals("Unknown command", outsider.messages.first())
        assertTrue(dispatcher.treeFor(outsider).nodes.none { it.name == "warp" || it.name == "w" })
    }

    @Test
    fun everyVanillaCommandRegistersWithoutClashing() {
        val dispatcher = CommandDispatcher(testEnvironment())
        registerVanillaCommands(dispatcher)

        val names = dispatcher.usableCommands(RecordingSource()).map { it.name }
        assertEquals(names.distinct(), names)
        assertContains(names, "gamemode")
        assertContains(names, "tp")
        assertContains(names, "xp")
    }

    @Test
    fun smartUsageMatchesBrigadiersBracketRules() {
        val dispatcher = CommandDispatcher(testEnvironment())
        val gamemode =
            dispatcher.register("gamemode") {
                argument("gamemode", GameModeArgument) {
                    executes { 1 }
                    argument("target", WordArgument) { executes { 1 } }
                }
            }
        val list =
            dispatcher.register("list") {
                executes { 1 }
                literal("uuids") { executes { 1 } }
            }

        val source = RecordingSource()
        assertEquals("gamemode <gamemode> [<target>]", smartUsage(gamemode, source))
        assertEquals("list [uuids]", smartUsage(list, source))
    }
}

class ArgumentTypeTest {
    private val environment = testEnvironment()
    private val source = RecordingSource()

    @Test
    fun absoluteIntegerCoordinatesAreCentreCorrectedOnXAndZButNotY() {
        val reader = StringReader("10 64 -3")
        val coordinates = Vec3Argument.parse(reader, source, environment)
        assertEquals(Triple(10.5, 64.0, -2.5), coordinates.resolve(Triple(0.0, 0.0, 0.0)))
    }

    @Test
    fun aCoordinateWithADecimalPointIsLeftAlone() {
        val reader = StringReader("10.0 64.0 -3.0")
        val coordinates = Vec3Argument.parse(reader, source, environment)
        assertEquals(Triple(10.0, 64.0, -3.0), coordinates.resolve(Triple(0.0, 0.0, 0.0)))
    }

    @Test
    fun tildeResolvesAgainstWhoeverRanTheCommand() {
        val reader = StringReader("~ ~5 ~-2")
        val coordinates = Vec3Argument.parse(reader, source, environment)
        assertEquals(Triple(100.0, 69.0, -52.0), coordinates.resolve(source.position))
    }

    @Test
    fun anIncompletePositionIsRejected() {
        val failure =
            assertFailsWith<CommandSyntaxException> { Vec3Argument.parse(StringReader("10 64"), source, environment) }
        assertEquals("argument.pos3d.incomplete", (failure.component as Component.Translatable).key)
    }

    @Test
    fun gameModesParseByNameAndNothingElse() {
        assertEquals(Gamemode.CREATIVE, GameModeArgument.parse(StringReader("creative"), source, environment))
        assertEquals(Gamemode.SPECTATOR, GameModeArgument.parse(StringReader("SPECTATOR"), source, environment))
        assertFailsWith<CommandSyntaxException> { GameModeArgument.parse(StringReader("god"), source, environment) }
    }

    @Test
    fun aGreedyStringTakesTheWholeRemainderIncludingSpaces() {
        val reader = StringReader("hello there world")
        assertEquals("hello there world", MessageArgument.parse(reader, source, environment))
        assertTrue(!reader.canRead())
    }
}
