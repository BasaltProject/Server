package org.basaltmc.server.command

import org.basaltmc.common.placeholder.PlaceholderRegistry
import org.basaltmc.common.placeholder.PlaceholderValue
import org.basaltmc.server.config.CommandOverride
import org.basaltmc.server.config.CommandsConfig
import org.basaltmc.server.config.CustomCommand
import org.basaltmc.server.player.PlayerRegistry
import org.basaltmc.server.player.PlayerSession
import org.basaltmc.text.Component
import org.basaltmc.text.PlainTextSerializer
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertNotNull
import kotlin.test.assertNull
import kotlin.test.assertTrue

/** Keeps the components themselves, not their text: whether a message is still a key is the point. */
private class ComponentSource(
    private val permissions: Set<String> = setOf("*"),
) : CommandSource {
    val messages = mutableListOf<Component>()

    override val permissionLevel: Int = PermissionLevel.OWNERS
    override val name: String = "Tester"
    override val displayName: Component = Component.text("Tester")
    override val position: Triple<Double, Double, Double> = Triple(0.0, 64.0, 0.0)
    override val player: PlayerSession? = null

    override fun hasPermission(node: String?): Boolean = node != null && permissions.any { it == "*" || it == node }

    override fun sendSystemMessage(message: Component) {
        messages += message
    }
}

/** A command that always succeeds with a vanilla-shaped translatable, so overrides have something to hit. */
private class ProbeCommand : Command(name = "probe", permissionLevel = PermissionLevel.ALL) {
    override fun build(root: CommandNodeBuilder) {
        root.executes { context ->
            context.success(Component.translatable("commands.probe.success", Component.text("42")), false)
            1
        }
    }
}

private fun environmentWith(
    commands: CommandsConfig,
    placeholders: PlaceholderRegistry = PlaceholderRegistry(),
): CommandEnvironment =
    CommandEnvironment(
        players = PlayerRegistry(),
        groups = testGroups(operatorPermissions = listOf("*")),
        worldSeed = 1L,
        maxPlayers = 20,
        placeholders = placeholders,
        commands = commands,
    )

private fun configOf(
    overrides: Map<String, CommandOverride> = emptyMap(),
    custom: List<CustomCommand> = emptyList(),
    messages: Map<String, String> = emptyMap(),
) = CommandsConfig(overrides, custom, messages)

class ConfiguredCommandTest {
    @Test
    fun aCommandSwitchedOffIsNotRegisteredAtAll() {
        val dispatcher =
            CommandDispatcher(environmentWith(configOf(mapOf("probe" to CommandOverride(enabled = false)))))

        assertNull(dispatcher.register(ProbeCommand()))
        // Unknown, not refused: a command that answers "you may not" tells everyone it exists.
        val source = ComponentSource()
        assertEquals(0, dispatcher.execute(source, "probe"))
    }

    @Test
    fun aCommandCanBePointedAtAnotherPermissionAndGivenMoreNames() {
        val override = CommandOverride(permission = "basalt.command.moderator", aliases = listOf("pb"))
        val dispatcher = CommandDispatcher(environmentWith(configOf(mapOf("probe" to override))))

        val node = assertNotNull(dispatcher.register(ProbeCommand()))
        assertEquals("basalt.command.moderator", node.permission)

        // The alias redirects onto the same node, so it carries the same permission.
        val moderator = ComponentSource(permissions = setOf("basalt.command.moderator"))
        assertEquals(1, dispatcher.execute(moderator, "pb"))
        // And the permission it used to ask for no longer opens it.
        assertEquals(0, dispatcher.execute(ComponentSource(permissions = setOf("basalt.command.probe")), "probe"))
    }

    @Test
    fun aCustomCommandAnswersWithItsOwnLines() {
        val placeholders =
            PlaceholderRegistry().apply {
                register("basalt_server") { params, _ ->
                    if (params == "online") PlaceholderValue.of(3) else PlaceholderValue.Unknown
                }
            }
        val custom =
            CustomCommand(
                name = "discord",
                permission = "basalt.command.discord",
                message = listOf("<blue>Chat with us</blue>", "%basalt_server_online% online"),
            )
        val dispatcher = CommandDispatcher(environmentWith(configOf(custom = listOf(custom)), placeholders))

        val source = ComponentSource(permissions = setOf("basalt.command.discord"))
        assertEquals(1, dispatcher.execute(source, "discord"))
        assertEquals(listOf("Chat with us", "3 online"), source.messages.map(PlainTextSerializer::serialize))
    }

    @Test
    fun aCustomCommandIsClosedUntilItsPermissionIsGranted() {
        val custom = CustomCommand("rules", "basalt.command.rules", listOf("be nice"))
        val dispatcher = CommandDispatcher(environmentWith(configOf(custom = listOf(custom))))

        assertEquals(0, dispatcher.execute(ComponentSource(permissions = emptySet()), "rules"))
        assertEquals(1, dispatcher.execute(ComponentSource(permissions = setOf("basalt.command.rules")), "rules"))
    }

    @Test
    fun anOverriddenMessageReplacesTheKeyAndKeepsItsArguments() {
        val messages = mapOf("commands.probe.success" to "<green>done: %1%</green>")
        val dispatcher = CommandDispatcher(environmentWith(configOf(messages = messages)))
        dispatcher.register(ProbeCommand())

        val source = ComponentSource()
        assertEquals(1, dispatcher.execute(source, "probe"))
        assertEquals("done: 42", PlainTextSerializer.serialize(source.messages.single()))
    }

    @Test
    fun aMessageNobodyOverrodeIsLeftAsAKeyForTheClientToTranslate() {
        val dispatcher = CommandDispatcher(environmentWith(configOf()))
        dispatcher.register(ProbeCommand())

        val source = ComponentSource()
        dispatcher.execute(source, "probe")
        // Still a Translatable: this is the whole reason overriding one costs the client's language.
        assertTrue(source.messages.single() is Component.Translatable)
    }
}
