package org.basaltmc.server.registry

import org.basaltmc.wire.common.Identifier
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertTrue

/** parity: `EntityType`'s own builder values, taken from the real registry. */
class EntityTypeShapesTest {
    @Test
    fun everyVanillaTypeHasABox() {
        assertEquals(EntityTypes.all.size, EntityTypeShapes.all.size, "one shape per registered type")
        // parity: the handful of types with no box at all: markers, displays and the bolt itself.
        val boxless = EntityTypeShapes.all.filter { it.width == 0f || it.height == 0f }.map { it.entity }
        assertEquals(
            listOf("block_display", "interaction", "item_display", "lightning_bolt", "marker", "text_display"),
            boxless,
        )
    }

    @Test
    fun theBoxesAreVanillasOwn() {
        assertEquals(0.6, EntityTypeShapes.widthOf(Identifier.vanilla("player")), 1e-6)
        assertEquals(1.8, EntityTypeShapes.heightOf(Identifier.vanilla("player")), 1e-6)
        assertEquals(0.25, EntityTypeShapes.widthOf(Identifier.vanilla("item")), 1e-6)
        assertEquals(0.98, EntityTypeShapes.widthOf(Identifier.vanilla("tnt")), 1e-6)
        assertEquals(0.98, EntityTypeShapes.heightOf(Identifier.vanilla("tnt")), 1e-6)
    }

    /** parity: `MobCategory`, which is what a spawn cap counts. */
    @Test
    fun eachTypeKnowsWhatItCountsAs() {
        assertEquals("monster", EntityTypeShapes.categoryOf(Identifier.vanilla("zombie")))
        assertEquals("creature", EntityTypeShapes.categoryOf(Identifier.vanilla("cow")))
        assertEquals("water_creature", EntityTypeShapes.categoryOf(Identifier.vanilla("squid")))
        assertEquals("misc", EntityTypeShapes.categoryOf(Identifier.vanilla("item")))
    }

    @Test
    fun whatBurnsAndWhatDoesNot() {
        assertTrue(EntityTypeShapes.isFireImmune(Identifier.vanilla("blaze")))
        assertTrue(!EntityTypeShapes.isFireImmune(Identifier.vanilla("cow")))
    }
}
