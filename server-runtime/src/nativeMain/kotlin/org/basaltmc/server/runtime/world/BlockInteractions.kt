package org.basaltmc.server.runtime.world

import org.basaltmc.server.registry.BlockStateDefinitions
import org.basaltmc.wire.common.Identifier

/** A block that changed by hand, and the sound it made. */
public class BlockToggle(
    public val changes: List<IntArray>,
    public val sound: String,
)

/**
 * Opening a door, flipping a lever, pushing a trapdoor: the blocks a hand can change on its own.
 *
 * parity: `DoorBlock`, `TrapDoorBlock`, `FenceGateBlock` and `LeverBlock`'s `useWithoutItem`, which
 * cycle one property and play a sound. What is not here is what happens afterwards: nothing carries
 * the redstone signal a lever makes, because there is no redstone yet, and a button stays pressed
 * because unpressing it needs a scheduled block tick and only fluids have those.
 */
public object BlockInteractions {
    /** parity: `BlockSetType.canOpenByHand`, which is false for exactly one family. */
    private const val IRON = "iron"

    /** parity: the sound sets, which are per wood rather than per block. */
    private val SOUND_SETS =
        listOf(
            "bamboo" to "bamboo_wood",
            "cherry" to "cherry_wood",
            "crimson" to "nether_wood",
            "warped" to "nether_wood",
            "copper" to "copper",
            "iron" to "iron",
        )

    /** Whether a hand does anything at all to this block. */
    public fun isToggleable(stateId: Int): Boolean = toggleOf(stateId) != null

    /**
     * Flips [stateId] and whatever else goes with it, or null when nothing happens.
     *
     * Both halves of a door move together, which is why this reads the world rather than answering
     * from the state alone.
     */
    public fun toggle(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
    ): BlockToggle? {
        val stateId = world.state.getBlockState(x, y, z) ?: return null
        val kind = toggleOf(stateId) ?: return null
        val block = BlockStateDefinitions.blockOf(stateId) ?: return null
        val open = BlockStateDefinitions.propertiesOf(stateId)[kind.property] != "true"
        val changes = mutableListOf<IntArray>()

        fun flip(
            atX: Int,
            atY: Int,
            atZ: Int,
        ) {
            val at = world.state.getBlockState(atX, atY, atZ) ?: return
            if (BlockStateDefinitions.blockOf(at) != block) return
            val flipped =
                BlockStateDefinitions.stateIdOf(block, mapOf(kind.property to open.toString()), fromStateId = at)
                    ?: return
            if (!world.state.setBlockState(atX, atY, atZ, flipped)) return
            changes += intArrayOf(atX, atY, atZ, flipped)
        }

        flip(x, y, z)
        if (kind == Toggle.DOOR) {
            // The other half, whichever way round this one is.
            val half = BlockStateDefinitions.propertiesOf(stateId)["half"]
            flip(x, if (half == "upper") y - 1 else y + 1, z)
        }
        if (changes.isEmpty()) return null
        return BlockToggle(changes, soundFor(block, kind, open))
    }

    private enum class Toggle(
        val property: String,
    ) {
        DOOR("open"),
        TRAPDOOR("open"),
        FENCE_GATE("open"),
        LEVER("powered"),
    }

    private fun toggleOf(stateId: Int): Toggle? {
        val path = BlockStateDefinitions.blockOf(stateId)?.path ?: return null
        // parity: an iron door or trapdoor only moves for redstone, never for a hand.
        return when {
            path == "lever" -> Toggle.LEVER
            path.startsWith(IRON) -> null
            path.endsWith("_door") -> Toggle.DOOR
            path.endsWith("_trapdoor") -> Toggle.TRAPDOOR
            path.endsWith("_fence_gate") -> Toggle.FENCE_GATE
            else -> null
        }
    }

    private fun soundFor(
        block: Identifier,
        kind: Toggle,
        open: Boolean,
    ): String {
        if (kind == Toggle.LEVER) return "minecraft:block.lever.click"
        val action = if (open) "open" else "close"
        val set = SOUND_SETS.firstOrNull { block.path.startsWith(it.first) }?.second
        return when (kind) {
            // parity: a plain fence gate has no wood in its sound name, unlike the other two.
            Toggle.FENCE_GATE ->
                if (set == null) "minecraft:block.fence_gate.$action" else "minecraft:block.${set}_fence_gate.$action"
            Toggle.TRAPDOOR -> "minecraft:block.${set ?: "wooden"}_trapdoor.$action"
            else -> "minecraft:block.${set ?: "wooden"}_door.$action"
        }
    }
}
