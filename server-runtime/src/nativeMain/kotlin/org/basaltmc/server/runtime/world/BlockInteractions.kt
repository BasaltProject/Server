package org.basaltmc.server.runtime.world

import org.basaltmc.server.registry.BlockStateDefinitions
import org.basaltmc.wire.common.Identifier

/** A block that changed by hand, and the sound it made. */
public class BlockToggle(
    public val changes: List<IntArray>,
    public val sound: String,
)

/** A block whose property moved one notch on, and the sound it made if any. */
public class BlockCycle(
    public val stateId: Int,
    public val sound: String?,
    public val pitch: Float = 1f,
    public val volume: Float = 1f,
)

/** A block that changed on its own timer, and the sound it made. */
public class BlockRelease(
    public val stateId: Int,
    public val sound: String,
)

/**
 * Opening a door, flipping a lever, pushing a button or a trapdoor: what a hand changes on its own.
 *
 * parity: `DoorBlock`, `TrapDoorBlock`, `FenceGateBlock`, `LeverBlock` and `ButtonBlock`'s
 * `useWithoutItem`, which cycle one property and play a sound. What the signal does afterwards is
 * the redstone code's, not this one's.
 */
public object BlockInteractions {
    /** parity: `BlockSetType.canOpenByHand`, which is false for exactly one family. */
    private const val IRON = "iron"

    /** The button families whose set uses the stone sounds and the shorter press. */
    private val STONE_BUTTONS = listOf("stone", "polished_blackstone", "copper", "gold", "iron")
    private const val STONE_PRESS_TICKS = 20
    private const val WOOD_PRESS_TICKS = 30

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
        // parity: `ButtonBlock.useWithoutItem` does nothing to a button already down. Its own
        // scheduled tick is what lets it back up.
        if (kind == Toggle.BUTTON && !open) return null
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

    /**
     * The right click that moves a repeater's delay on a notch or flips a comparator's mode.
     *
     * parity: `RepeaterBlock.useWithoutItem`, which cycles the delay in silence, and
     * `ComparatorBlock.useWithoutItem`, which cycles the mode and clicks.
     */
    public fun cycle(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
    ): BlockCycle? {
        val stateId = world.state.getBlockState(x, y, z) ?: return null
        val block = BlockStateDefinitions.blockOf(stateId) ?: return null
        val properties = BlockStateDefinitions.propertiesOf(stateId)
        return when (block.path) {
            "repeater" -> {
                val delay = properties["delay"]?.toIntOrNull() ?: 1
                val next = if (delay >= MAX_DELAY) 1 else delay + 1
                val turned =
                    BlockStateDefinitions.stateIdOf(block, mapOf("delay" to next.toString()), fromStateId = stateId)
                        ?: return null
                if (!world.state.setBlockState(x, y, z, turned)) return null
                BlockCycle(turned, sound = null)
            }
            "comparator" -> {
                val subtracting = properties["mode"] == "subtract"
                val mode = if (subtracting) "compare" else "subtract"
                val turned =
                    BlockStateDefinitions.stateIdOf(block, mapOf("mode" to mode), fromStateId = stateId) ?: return null
                if (!world.state.setBlockState(x, y, z, turned)) return null
                // parity: the pitch says which way it went, and it is quieter than a lever.
                val pitch = if (subtracting) COMPARE_PITCH else SUBTRACT_PITCH
                BlockCycle(turned, COMPARATOR_CLICK, pitch = pitch, volume = COMPARATOR_VOLUME)
            }
            else -> null
        }
    }

    /** Whether a hand moves this block on a notch rather than flipping it. */
    public fun isCycleable(stateId: Int): Boolean = BlockStateDefinitions.blockOf(stateId)?.path in CYCLEABLE

    private val CYCLEABLE = setOf("repeater", "comparator")

    /** parity: `RepeaterBlock.DELAY`, one to four, and `ComparatorBlock`'s own click. */
    private const val MAX_DELAY = 4
    private const val COMPARATOR_CLICK = "block.comparator.click"
    private const val COMPARATOR_VOLUME = 0.3f
    private const val SUBTRACT_PITCH = 0.55f
    private const val COMPARE_PITCH = 0.5f

    private enum class Toggle(
        val property: String,
    ) {
        DOOR("open"),
        TRAPDOOR("open"),
        FENCE_GATE("open"),
        LEVER("powered"),
        BUTTON("powered"),
    }

    /**
     * How long a pressed button stays down. parity: the `ButtonBlock` constructors, twenty ticks
     * for every stone-like set and thirty for every wooden one.
     */
    public fun pressedTicks(stateId: Int): Int? {
        if (toggleOf(stateId) != Toggle.BUTTON) return null
        val path = BlockStateDefinitions.blockOf(stateId)?.path ?: return null
        return if (STONE_BUTTONS.any { path.startsWith(it) }) STONE_PRESS_TICKS else WOOD_PRESS_TICKS
    }

    /**
     * parity: `BlockSetType`'s button sounds, which do not follow its other sounds: a copper, gold
     * or iron button clicks like stone, and only the four wood families have their own.
     */
    private fun buttonSoundSet(block: Identifier): String =
        when {
            STONE_BUTTONS.any { block.path.startsWith(it) } -> "stone"
            block.path.startsWith("bamboo") -> "bamboo_wood"
            block.path.startsWith("cherry") -> "cherry_wood"
            block.path.startsWith("crimson") || block.path.startsWith("warped") -> "nether_wood"
            else -> "wooden"
        }

    private fun toggleOf(stateId: Int): Toggle? {
        val path = BlockStateDefinitions.blockOf(stateId)?.path ?: return null
        // parity: an iron door or trapdoor only moves for redstone, never for a hand.
        return when {
            path == "lever" -> Toggle.LEVER
            path.endsWith("_button") -> Toggle.BUTTON
            path.startsWith(IRON) -> null
            path.endsWith("_door") -> Toggle.DOOR
            path.endsWith("_trapdoor") -> Toggle.TRAPDOOR
            path.endsWith("_fence_gate") -> Toggle.FENCE_GATE
            else -> null
        }
    }

    /** What a button's own scheduled tick does: back up, with the other half of its click. */
    public fun release(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
    ): BlockRelease? {
        val stateId = world.state.getBlockState(x, y, z) ?: return null
        if (toggleOf(stateId) != Toggle.BUTTON) return null
        if (BlockStateDefinitions.propertiesOf(stateId)["powered"] != "true") return null
        val block = BlockStateDefinitions.blockOf(stateId) ?: return null
        val released = BlockStateDefinitions.stateIdOf(block, mapOf("powered" to "false"), stateId) ?: return null
        if (!world.state.setBlockState(x, y, z, released)) return null
        return BlockRelease(released, soundFor(block, Toggle.BUTTON, open = false))
    }

    /**
     * The same sound, for the blocks redstone opens rather than a hand: a door, a trapdoor or a
     * fence gate. Anything else answers null, because nothing else makes a noise on a signal.
     */
    public fun redstoneSound(
        stateId: Int,
        open: Boolean,
    ): String? {
        val block = BlockStateDefinitions.blockOf(stateId) ?: return null
        val kind =
            when {
                block.path.endsWith("_door") -> Toggle.DOOR
                block.path.endsWith("_trapdoor") -> Toggle.TRAPDOOR
                block.path.endsWith("_fence_gate") -> Toggle.FENCE_GATE
                else -> return null
            }
        return soundFor(block, kind, open)
    }

    private fun soundFor(
        block: Identifier,
        kind: Toggle,
        open: Boolean,
    ): String {
        if (kind == Toggle.LEVER) return "minecraft:block.lever.click"
        if (kind ==
            Toggle.BUTTON
        ) {
            return "minecraft:block.${buttonSoundSet(block)}_button.click_${if (open) "on" else "off"}"
        }
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
