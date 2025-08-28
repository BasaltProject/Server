package org.basaltmc.server.runtime.world

import kotlin.math.floor

/** Village-section queries shared by custom spawners until the runtime owns a full POI claim map. */
internal object VillageProximity {
    // parity: PoiManager.sectionsToVillage. With no POI-claim table yet, a live villager's section
    // stands in for an occupied POI; the distance is still SectionTracker's Chebyshev.
    fun isCloseToVillage(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
        sectionDistance: Int,
    ): Boolean {
        val sectionX = x shr SECTION_BITS
        val sectionY = y shr SECTION_BITS
        val sectionZ = z shr SECTION_BITS
        return world.state.entities().any { entity ->
            !entity.removed &&
                entity.type.path == "villager" &&
                maxOf(
                    kotlin.math.abs((floor(entity.x).toInt() shr SECTION_BITS) - sectionX),
                    kotlin.math.abs((floor(entity.y).toInt() shr SECTION_BITS) - sectionY),
                    kotlin.math.abs((floor(entity.z).toInt() shr SECTION_BITS) - sectionZ),
                ) <= sectionDistance
        }
    }

    // One live villager stands in for one claimed home until claim tickets exist. The radius is
    // PoiManager.getCountInRange's spherical one, not the section metric above.
    fun occupiedHomesNear(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
        radius: Int,
    ): Int {
        val radiusSquared = radius.toLong() * radius
        return world.state.entities().count { entity ->
            if (entity.removed || entity.type.path != "villager") return@count false
            val dx = floor(entity.x).toInt() - x
            val dy = floor(entity.y).toInt() - y
            val dz = floor(entity.z).toInt() - z
            dx.toLong() * dx + dy.toLong() * dy + dz.toLong() * dz <= radiusSquared
        }
    }

    private const val SECTION_BITS = 4
}
