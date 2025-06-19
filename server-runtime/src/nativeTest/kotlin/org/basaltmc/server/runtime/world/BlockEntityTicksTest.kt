package org.basaltmc.server.runtime.world

import org.basaltmc.server.registry.BlockStateDefinitions
import org.basaltmc.server.runtime.config.WorldDeclaration
import org.basaltmc.server.runtime.config.WorldDimension
import org.basaltmc.server.runtime.config.WorldSettings
import org.basaltmc.server.runtime.config.WorldType
import org.basaltmc.server.world.ChunkBlockEntity
import org.basaltmc.server.world.crafting.Ingredient
import org.basaltmc.server.world.crafting.Recipe
import org.basaltmc.server.world.loot.ItemStack
import org.basaltmc.wire.common.Identifier
import org.basaltmc.wire.common.logging.LogLevel
import org.basaltmc.wire.common.logging.Logger
import org.basaltmc.wire.nbt.NbtCompound
import kotlin.test.AfterTest
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertTrue

/** parity: `ServerLevel.tickBlockEntities`, over the chunks a player is near. */
class BlockEntityTicksTest {
    private val world =
        LoadedWorld(
            WorldDeclaration("entities", WorldDimension.OVERWORLD, WorldType.VOID, seed = 0L, isDefault = true),
            WorldSettings.VANILLA,
            WorldMetadata(0L, 0, 64, 0),
            "build/test-data/block-entities",
            Logger("BlockEntityTicksTest").apply { minLevel = LogLevel.ERROR },
            1,
        )

    private val ironOre = Identifier.vanilla("iron_ore")

    private val smeltIron =
        Recipe.Cooking(
            group = "",
            result = ItemStack(Identifier.vanilla("iron_ingot"), 1),
            cooker = Recipe.Cooker.SMELTING,
            ingredient = Ingredient(setOf(ironOre)),
            experience = 0.7f,
            cookingTime = 200,
        )

    init {
        world.state.getOrGenerateChunk(0, 0)
    }

    @AfterTest
    fun stopGenerating() {
        world.stopGenerating(drainTimeoutMillis = 100)
    }

    private fun recipes(
        cooker: Recipe.Cooker,
        stack: ItemStack,
    ): Recipe.Cooking? = if (cooker == Recipe.Cooker.SMELTING && stack.item == ironOre) smeltIron else null

    /** A furnace with ore and coal lights up, and the block itself says so. */
    @Test
    fun aFurnaceInALoadedChunkSmelts() {
        val unlit =
            checkNotNull(
                BlockStateDefinitions.stateIdOf(
                    Identifier.vanilla("furnace"),
                    mapOf("facing" to "north", "lit" to "false"),
                ),
            )
        world.state.setBlockState(4, 70, 4, unlit)
        val data = NbtCompound()
        val state = Furnaces.State()
        state.items[0] = ItemStack(ironOre, 4)
        state.items[1] = ItemStack(Identifier.vanilla("coal"), 1)
        state.cookingTotalTime = 200
        Furnaces.write(state, data)
        val chunk = checkNotNull(world.state.chunkIfLoaded(0, 0))
        chunk.setBlockEntity(ChunkBlockEntity(4, 70, 4, Identifier.vanilla("furnace"), data))
        chunk.markClean()

        val first = BlockEntityTicks.tick(world, listOf(intArrayOf(0, 0)), ::recipes)

        assertEquals(1, first.size, "the block itself changed, because the fire is now lit")
        assertEquals(
            "true",
            BlockStateDefinitions.propertiesOf(world.state.getBlockState(4, 70, 4) ?: 0)["lit"],
        )
        assertTrue(chunk.isDirty, "a furnace counting down has to be saved")

        repeat(200) { BlockEntityTicks.tick(world, listOf(intArrayOf(0, 0)), ::recipes) }

        val cooked = Furnaces.read(checkNotNull(chunk.blockEntityAt(4, 70, 4)).data)
        assertEquals(1, cooked.items[2]?.count, "one ingot came out")
        assertEquals(3, cooked.items[0]?.count)
    }

    /** A furnace nobody is near does not tick at all, which is what keeps an empty world still. */
    @Test
    fun aChunkThatIsNotSimulatedStandsStill() {
        val data = NbtCompound()
        val state = Furnaces.State()
        state.items[0] = ItemStack(ironOre, 1)
        state.items[1] = ItemStack(Identifier.vanilla("coal"), 1)
        state.cookingTotalTime = 200
        Furnaces.write(state, data)
        val chunk = checkNotNull(world.state.chunkIfLoaded(0, 0))
        chunk.setBlockEntity(ChunkBlockEntity(8, 70, 8, Identifier.vanilla("furnace"), data))

        repeat(50) { BlockEntityTicks.tick(world, emptyList(), ::recipes) }

        val still = Furnaces.read(checkNotNull(chunk.blockEntityAt(8, 70, 8)).data)
        assertEquals(0, still.cookingTimer)
        assertEquals(1, still.items[1]?.count, "the coal was not touched")
    }
}
