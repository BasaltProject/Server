package org.basaltmc.server.runtime.world

import org.basaltmc.server.registry.BlockEntityTypes
import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.registry.ItemBlocks
import org.basaltmc.server.registry.Items
import org.basaltmc.wire.common.Identifier
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertNotNull
import kotlin.test.assertNull

/** parity: `BlockItem.getBlock` and `Block.asItem`, which are not the same name for a dozen items. */
class ItemBlocksTest {
    /** The ones that made this necessary: none of them can be placed by their own name. */
    @Test
    fun anItemPlacesItsOwnBlock() {
        assertEquals(Identifier.vanilla("redstone_wire"), ItemBlocks.of(Identifier.vanilla("redstone")))
        assertEquals(Identifier.vanilla("tripwire"), ItemBlocks.of(Identifier.vanilla("string")))
        assertEquals(Identifier.vanilla("wheat"), ItemBlocks.of(Identifier.vanilla("wheat_seeds")))
        assertEquals(Identifier.vanilla("cocoa"), ItemBlocks.of(Identifier.vanilla("cocoa_beans")))
    }

    /** And the way back, which is what middle-clicking one of them needs. */
    @Test
    fun aBlockIsPickedUpAsItsOwnItem() {
        assertEquals(Identifier.vanilla("redstone"), ItemBlocks.itemOf(Identifier.vanilla("redstone_wire")))
        assertEquals(Identifier.vanilla("wheat_seeds"), ItemBlocks.itemOf(Identifier.vanilla("wheat")))
        // Everything else keeps its name, which is nearly every block there is.
        assertEquals(Identifier.vanilla("stone"), ItemBlocks.itemOf(Identifier.vanilla("stone")))
    }

    /** Every block a table names has to exist, and its item with it. */
    @Test
    fun bothSidesOfTheTableAreReal() {
        for (item in listOf("redstone", "string", "wheat_seeds", "carrot", "potato", "glow_berries")) {
            val block = ItemBlocks.of(Identifier.vanilla(item))
            assertNotNull(Blocks.REGISTRY.idOf(block), block.toString())
            assertNotNull(Items.REGISTRY.byIdentifier(Identifier.vanilla(item)), item)
        }
    }

    /**
     * parity: `BlockEntityType.isValid`. All eight copper chests share one type, and naming it after
     * the block left it without a protocol id, so the chest never reached the client.
     */
    @Test
    fun aBlockEntityIsNamedAfterItsTypeAndNotItsBlock() {
        assertEquals(Identifier.vanilla("chest"), BlockEntityTypes.ofBlock(Identifier.vanilla("waxed_copper_chest")))
        assertEquals(Identifier.vanilla("chest"), BlockEntityTypes.ofBlock(Identifier.vanilla("copper_chest")))
        assertEquals(
            Identifier.vanilla("shulker_box"),
            BlockEntityTypes.ofBlock(Identifier.vanilla("black_shulker_box")),
        )
        assertEquals(Identifier.vanilla("bell"), BlockEntityTypes.ofBlock(Identifier.vanilla("bell")))
        assertNull(BlockEntityTypes.ofBlock(Identifier.vanilla("stone")), "plain stone owns none")
    }

    /** And every type a block owns has a protocol id, or the chunk cannot carry it. */
    @Test
    fun everyOwnedTypeHasAProtocolId() {
        for (block in listOf("waxed_copper_chest", "bell", "chest", "black_shulker_box", "decorated_pot")) {
            val type = assertNotNull(BlockEntityTypes.ofBlock(Identifier.vanilla(block)), block)
            assertNotNull(BlockEntityTypes.idOf(type), type.toString())
        }
    }
}
