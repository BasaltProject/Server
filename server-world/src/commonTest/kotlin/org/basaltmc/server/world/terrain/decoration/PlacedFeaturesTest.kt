package org.basaltmc.server.world.terrain.decoration

import org.basaltmc.wire.common.Identifier
import kotlin.test.Test
import kotlin.test.assertNotNull
import kotlin.test.assertNull
import kotlin.test.assertTrue

/** What `/place feature` can run by id, and that the registry answers by the vanilla names. */
class PlacedFeaturesTest {
    @Test
    fun theRegistryAnswersByVanillaPlacedFeatureName() {
        for (name in listOf("oak", "ore_diamond", "spring_water", "patch_grass_forest", "twisting_vines")) {
            assertNotNull(PlacedFeatures[Identifier.vanilla(name)], name)
        }
    }

    @Test
    fun theFiveThatNeedMoreThanAChunkAreDeclaredAsSuch() {
        // They answer through the contextual registry instead, because a chunk alone cannot run
        // the End's spikes, a chorus plant, a fossil or the freeze pass.
        for (name in listOf("end_spike", "chorus_plant", "fossil_lower", "fossil_upper", "freeze_top_layer")) {
            assertNull(PlacedFeatures[Identifier.vanilla(name)], name)
            assertNotNull(PlacedFeatures.contextual(Identifier.vanilla(name)), name)
        }
    }

    @Test
    fun anIdNobodyExposesAnswersNull() {
        assertNull(PlacedFeatures[Identifier.vanilla("not_a_feature")])
    }

    @Test
    fun everyFamilyIsRepresented() {
        val ids = PlacedFeatures.ids.map { it.path }
        assertTrue(ids.count { it.startsWith("ore_") } >= 30, "ores: $ids")
        assertTrue(ids.count { it.startsWith("patch_") } >= 15, "patches")
        // The 262 vanilla placed features, plus a handful this build names for itself.
        assertTrue(ids.size >= 262, "total ${ids.size}")
    }
}
