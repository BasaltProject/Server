package org.basaltmc.server.runtime.command

import org.basaltmc.server.registry.ItemTools
import org.basaltmc.server.runtime.command.commands.EffectCommand
import org.basaltmc.server.runtime.player.PlayerRegistry
import org.basaltmc.server.runtime.player.PlayerSession
import org.basaltmc.wire.common.Identifier
import org.basaltmc.wire.text.Component
import org.basaltmc.wire.text.PlainTextSerializer
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertTrue

private class EffectConsole : CommandSource {
    val messages: MutableList<String> = mutableListOf()

    override val name: String = "Server"
    override val displayName: Component = Component.text("Server")
    override val permissionLevel: Int = PermissionLevel.OWNERS
    override val position: Triple<Double, Double, Double> = Triple(0.0, 64.0, 0.0)
    override val player: PlayerSession? = null

    override fun hasPermission(node: String?): Boolean = true

    override fun sendSystemMessage(message: Component) {
        messages += PlainTextSerializer.serialize(message)
    }
}

class EffectCommandTest {
    private val dispatcher =
        CommandDispatcher(
            CommandEnvironment(
                players = PlayerRegistry(),
                groupsProvider = { testGroups() },
                worldSeed = 0L,
                maxPlayers = 20,
            ),
        ).also { it.register(EffectCommand()) }

    private val source = EffectConsole()

    /** Nobody is connected in this fixture, so the command reports rather than throwing. */
    @Test
    fun givingAnEffectToNobodyStillParses() {
        assertEquals(0, dispatcher.execute(source, "effect give @a minecraft:haste 30 2"))
        assertTrue(source.messages.none { "Unknown" in it })
    }

    @Test
    fun anEffectNobodyDefinesIsRefused() {
        assertEquals(0, dispatcher.execute(source, "effect give @a minecraft:nonsense"))
    }

    /** parity: `MobEffectUtil.getDigSpeedAmplification`, twenty per cent per level. */
    @Test
    fun hasteSpeedsUpMiningAndFatigueSlowsIt() {
        val pickaxe = Identifier.vanilla("diamond_pickaxe")
        val stone = Identifier.vanilla("stone")
        val plain =
            ItemTools.destroyProgressPerTick(
                pickaxe,
                stone,
                1.5f,
                requiresCorrectTool = true,
                efficiencyLevel = 0,
            ) { _, _ -> true }
        val hasted =
            ItemTools.destroyProgressPerTick(
                pickaxe,
                stone,
                1.5f,
                requiresCorrectTool = true,
                efficiencyLevel = 0,
                hasteAmplifier = 0,
            ) { _, _ -> true }
        val tired =
            ItemTools.destroyProgressPerTick(
                pickaxe,
                stone,
                1.5f,
                requiresCorrectTool = true,
                efficiencyLevel = 0,
                miningFatigueAmplifier = 0,
            ) { _, _ -> true }

        assertEquals(plain * 1.2f, hasted, 1e-6f)
        assertEquals(plain * 0.3f, tired, 1e-6f)
    }

    /** parity: under water it is a fifth, and off the ground a fifth again. */
    @Test
    fun waterAndAirSlowMiningDown() {
        val pickaxe = Identifier.vanilla("diamond_pickaxe")
        val stone = Identifier.vanilla("stone")
        val plain =
            ItemTools.destroyProgressPerTick(
                pickaxe,
                stone,
                1.5f,
                requiresCorrectTool = true,
                efficiencyLevel = 0,
            ) { _, _ -> true }
        val submerged =
            ItemTools.destroyProgressPerTick(
                pickaxe,
                stone,
                1.5f,
                requiresCorrectTool = true,
                efficiencyLevel = 0,
                underwater = true,
            ) { _, _ -> true }
        val airborne =
            ItemTools.destroyProgressPerTick(
                pickaxe,
                stone,
                1.5f,
                requiresCorrectTool = true,
                efficiencyLevel = 0,
                inAir = true,
            ) { _, _ -> true }

        assertEquals(plain * 0.2f, submerged, 1e-6f)
        assertEquals(plain / 5f, airborne, 1e-6f)
    }
}
