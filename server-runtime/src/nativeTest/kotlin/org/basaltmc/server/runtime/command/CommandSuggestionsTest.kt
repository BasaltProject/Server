package org.basaltmc.server.runtime.command

import org.basaltmc.server.runtime.player.PlayerSession
import org.basaltmc.wire.common.Identifier
import org.basaltmc.wire.protocol.play.CommandNodeData
import org.basaltmc.wire.text.Component
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertTrue

private class SuggestingSource(
    private val permissions: Set<String> = setOf("*"),
) : CommandSource {
    override val permissionLevel: Int = PermissionLevel.OWNERS
    override val name: String = "Tester"
    override val displayName: Component = Component.text("Tester")
    override val position: Triple<Double, Double, Double> = Triple(0.0, 64.0, 0.0)
    override val player: PlayerSession? = null

    override fun hasPermission(node: String?): Boolean = node != null && permissions.any { it == "*" || it == node }

    override fun sendSystemMessage(message: Component) = Unit
}

private val STRUCTURES =
    listOf("village_plains", "village_desert", "desert_pyramid").map(Identifier::vanilla)

private fun structureDispatcher(): CommandDispatcher {
    val dispatcher = CommandDispatcher(testEnvironment())
    dispatcher.register("place") {
        literal("structure") {
            argument("id", ResourceLocationArgument) {
                suggestsFromServer { context -> context.matchingIds(STRUCTURES) }
                executes { 1 }
            }
        }
        literal("feature") {
            argument("id", ResourceLocationArgument) { executes { 1 } }
        }
    }
    return dispatcher
}

class CommandSuggestionsTest {
    @Test
    fun offersEveryValueWhenTheArgumentIsStillEmpty() {
        val result = structureDispatcher().suggest(SuggestingSource(), "/place structure ")

        assertEquals(STRUCTURES.map { it.toString() }.sorted(), result.suggestions.map { it.text })
        assertEquals("/place structure ".length, result.start)
        assertEquals(0, result.length)
    }

    @Test
    fun replacesOnlyTheWordBeingTyped() {
        val input = "/place structure minecraft:vill"
        val result = structureDispatcher().suggest(SuggestingSource(), input)

        assertEquals(input.indexOf("minecraft:vill"), result.start)
        assertEquals("minecraft:vill".length, result.length)
        assertEquals(
            listOf("minecraft:village_desert", "minecraft:village_plains"),
            result.suggestions.map { it.text },
        )
    }

    /** parity: `SharedSuggestionProvider.matchesSubStr`, which also matches after `.`, `_` and `/`. */
    @Test
    fun matchesAfterASeparatorAndWithoutTheNamespace() {
        val result = structureDispatcher().suggest(SuggestingSource(), "/place structure plains")

        assertEquals(listOf("minecraft:village_plains"), result.suggestions.map { it.text })
    }

    @Test
    fun offersTheLiteralChildrenOfTheNodeBeingTyped() {
        val result = structureDispatcher().suggest(SuggestingSource(), "/place stru")

        assertEquals(listOf("structure"), result.suggestions.map { it.text })
        assertEquals("/place ".length, result.start)
    }

    @Test
    fun answersNothingForAnArgumentWithNoValuesOfItsOwn() {
        assertTrue(structureDispatcher().suggest(SuggestingSource(), "/place feature ").suggestions.isEmpty())
    }

    @Test
    fun completesFromWhatEarlierArgumentsParsedTo() {
        val dispatcher = CommandDispatcher(testEnvironment())
        val criteria = mapOf("plains" to listOf("visit", "trade"), "desert" to listOf("dig"))
        dispatcher.register("advancement") {
            argument("advancement", WordArgument) {
                argument("criterion", WordArgument) {
                    suggests { context ->
                        context.matching(criteria[context.argument<String>("advancement")].orEmpty())
                    }
                    executes { 1 }
                }
            }
        }

        val result = dispatcher.suggest(SuggestingSource(), "/advancement plains ")

        assertEquals(listOf("trade", "visit"), result.suggestions.map { it.text })
    }

    @Test
    fun saysNothingAboutACommandTheSourceMayNotUse() {
        val dispatcher = CommandDispatcher(testEnvironment())
        dispatcher.register("secret") {
            permission("basalt.command.secret")
            argument("id", ResourceLocationArgument) {
                suggests { context -> context.matchingIds(STRUCTURES) }
                executes { 1 }
            }
        }

        val result = dispatcher.suggest(SuggestingSource(permissions = emptySet()), "/secret ")

        assertTrue(result.suggestions.isEmpty())
    }

    /** The client only asks about a node the tree flagged, so the flag has to travel with it. */
    @Test
    fun marksTheNodesTheServerAnswersFor() {
        val nodes = structureDispatcher().treeFor(SuggestingSource()).nodes
        val arguments = nodes.filter { it.kind == CommandNodeData.Kind.ARGUMENT }

        assertEquals(1, arguments.count { it.suggestionId == SuggestionProviders.ASK_SERVER })
        assertEquals(1, arguments.count { it.suggestionId == null })
    }

    /**
     * An argument whose parser names a registry carries no provider: the client completes from its
     * own copy when it has one and asks for the rest by itself. parity: what vanilla sends.
     */
    @Test
    fun leavesARegistryArgumentToItsOwnParser() {
        val dispatcher = CommandDispatcher(testEnvironment())
        dispatcher.register("locate") {
            argument("biome", RegistryArgument.orTag("minecraft:worldgen/biome")) {
                suggests { context -> context.matchingIds(STRUCTURES) }
                executes { 1 }
            }
        }
        val node =
            dispatcher
                .treeFor(SuggestingSource())
                .nodes
                .single { it.kind == CommandNodeData.Kind.ARGUMENT }

        assertEquals(null, node.suggestionId)
        // Still answered: the client asks for a registry it has not got a copy of.
        assertEquals(3, dispatcher.suggest(SuggestingSource(), "/locate ").suggestions.size)
    }
}
