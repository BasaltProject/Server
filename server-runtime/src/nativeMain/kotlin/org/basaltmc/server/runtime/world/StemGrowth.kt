package org.basaltmc.server.runtime.world

import org.basaltmc.server.random.LegacyRandom
import org.basaltmc.server.registry.BlockStateDefinitions
import org.basaltmc.server.registry.Blocks
import org.basaltmc.wire.common.Identifier

/** parity: `StemBlock.randomTick`, pumpkin and melon stems. */
public object StemGrowth {
    /**
     * parity: `StemBlock.randomTick`: the same growth speed a crop has, and at full age a fruit on
     * one of the four sides with the stem turned to face it.
     */
    public fun stem(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
        stateId: Int,
        speed: Float,
        random: LegacyRandom,
        changes: MutableList<BlockChange>,
    ) {
        if (WorldLight.rawBrightness(world, x, y, z) < GROWTH_LIGHT) return
        if (random.nextInt((25f / speed).toInt() + 1) != 0) return
        val block = BlockStateDefinitions.blockOf(stateId) ?: return
        val age = BlockStateDefinitions.propertiesOf(stateId)["age"]?.toIntOrNull() ?: return
        if (age < STEM_MAX_AGE) {
            val older = BlockStateDefinitions.stateIdOf(block, mapOf("age" to (age + 1).toString()), stateId) ?: return
            set(world, x, y, z, older, changes)
            return
        }
        val facing = HORIZONTAL_NAMES[random.nextInt(HORIZONTAL_NAMES.size)]
        val fruitX = x + offsetX(facing)
        val fruitZ = z + offsetZ(facing)
        if (world.state.getBlockState(fruitX, y, fruitZ) != Blocks.AIR.stateId) return
        val ground = world.state.getBlockState(fruitX, y - 1, fruitZ) ?: return
        val groundPath = BlockStateDefinitions.blockOf(ground)?.path
        if (groundPath !in FRUIT_GROUND) return
        val fruit = FRUIT_OF[block.path] ?: return
        val attached = ATTACHED_OF[block.path] ?: return
        val fruitState = Blocks.REGISTRY.idOf(Identifier.vanilla(fruit)) ?: return
        val attachedState =
            BlockStateDefinitions.stateIdOf(Identifier.vanilla(attached), mapOf("facing" to facing)) ?: return
        set(world, fruitX, y, fruitZ, fruitState, changes)
        set(world, x, y, z, attachedState, changes)
    }

    /** Which stem grows what, and what it turns into once it has. */
    private val FRUIT_OF = mapOf("pumpkin_stem" to "pumpkin", "melon_stem" to "melon")
    private val ATTACHED_OF =
        mapOf("pumpkin_stem" to "attached_pumpkin_stem", "melon_stem" to "attached_melon_stem")

    /** parity: `StemBlock.fruitSupportBlocks`, `#minecraft:dirt` plus farmland. */
    private val FRUIT_GROUND =
        setOf(
            "dirt",
            "grass_block",
            "podzol",
            "coarse_dirt",
            "mycelium",
            "rooted_dirt",
            "moss_block",
            "mud",
            "farmland",
        )

    private const val STEM_MAX_AGE = 7
}
