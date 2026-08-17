package org.basaltmc.server.registry

import org.basaltmc.wire.common.Identifier
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertNull
import kotlin.test.assertTrue

class BlockStateDefinitionsTest {
    private fun block(name: String) = Identifier.vanilla(name)

    @Test
    fun everyStateIdBelongsToExactlyOneBlock() {
        for (stateId in 0 until Blocks.stateCount) {
            assertTrue(BlockStateDefinitions.blockOf(stateId) != null, "state $stateId belongs to nothing")
        }
    }

    @Test
    fun aStateIdAndItsPropertiesConvertBothWays() {
        for (stateId in 0 until Blocks.stateCount) {
            val owner = checkNotNull(BlockStateDefinitions.blockOf(stateId))
            val properties = BlockStateDefinitions.propertiesOf(stateId)
            assertEquals(stateId, BlockStateDefinitions.stateIdOf(owner, properties), "state $stateId")
        }
    }

    @Test
    fun theDefaultStateMatchesTheRegistry() {
        val furnace = checkNotNull(Blocks.REGISTRY.idOf(block("furnace")))
        assertEquals(
            mapOf("facing" to "north", "lit" to "false"),
            BlockStateDefinitions.propertiesOf(furnace),
        )
    }

    /** The chests are the blocks whose report order is not the order their ids run in. */
    @Test
    fun aChestsPropertiesRunInTheOrderItsIdsDo() {
        val chest = checkNotNull(Blocks.REGISTRY.idOf(block("chest")))
        val facingWest = checkNotNull(BlockStateDefinitions.stateIdOf(block("chest"), mapOf("facing" to "west")))
        assertEquals("west", BlockStateDefinitions.propertiesOf(facingWest)["facing"])
        assertEquals(
            BlockStateDefinitions.propertiesOf(chest)["type"],
            BlockStateDefinitions.propertiesOf(facingWest)["type"],
        )
    }

    @Test
    fun oneChangedPropertyKeepsTheRest() {
        val slab = block("stone_slab")
        val waterlogged = checkNotNull(BlockStateDefinitions.stateIdOf(slab, mapOf("waterlogged" to "true")))
        val top = checkNotNull(BlockStateDefinitions.stateIdOf(slab, mapOf("type" to "top"), fromStateId = waterlogged))
        assertEquals(mapOf("type" to "top", "waterlogged" to "true"), BlockStateDefinitions.propertiesOf(top))
    }

    @Test
    fun anUnknownPropertyIsRefused() {
        assertNull(BlockStateDefinitions.stateIdOf(block("stone_slab"), mapOf("facing" to "north")))
        assertNull(BlockStateDefinitions.stateIdOf(block("stone_slab"), mapOf("type" to "sideways")))
    }
}
