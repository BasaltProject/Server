package org.basaltmc.server.world.terrain.decoration

import org.basaltmc.server.random.WorldgenRandom
import org.basaltmc.server.world.Chunk
import org.basaltmc.wire.common.Identifier

/**
 * Placed features that can be run at one position on demand, by id, which is what `/place feature`
 * needs: the decoration pipeline invokes a feature per step over a whole chunk, never "this one,
 * here".
 *
 * A feature joins the list once its body takes a position. Everything else is refused by name, so
 * the registry reports [ids].
 */
public object PlacedFeatures {
    /** One feature at chunk-local [localX]/[localZ] and world [y]. */
    public fun interface Placement {
        public fun place(
            chunk: Chunk,
            random: WorldgenRandom,
            localX: Int,
            y: Int,
            localZ: Int,
        )
    }

    private val byId: Map<Identifier, Placement> =
        mapOf(
            Identifier.vanilla("forest_rock") to
                Placement { chunk, random, x, y, z -> ForestRockDecoration.placeAt(chunk, random, x, y, z) },
            Identifier.vanilla("desert_well") to
                Placement { chunk, random, x, y, z -> DesertWellDecoration.placeAt(chunk, random, x, y, z) },
        )

    public val ids: Set<Identifier> get() = byId.keys

    public operator fun get(id: Identifier): Placement? = byId[id]
}
