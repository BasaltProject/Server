package org.basaltmc.server.registry

import org.basaltmc.wire.common.Identifier

/** One entity type's box, spawn category, tracking and the two flags a spawn rule reads. */
public class EntityTypeShape(
    public val entity: String,
    public val width: Float,
    public val height: Float,
    public val category: String,
    public val trackingRange: Int,
    public val updateInterval: Int,
    public val fireImmune: Boolean,
    public val farFromPlayer: Boolean,
)

/**
 * How big each entity type is, and what it counts as when the world decides what to spawn.
 *
 * parity: `EntityType`'s own builder values. The box is what collides with the world, the category
 * is what a spawn cap counts, and the tracking range is how far a client is told about it.
 */
public object EntityTypeShapes {
    private val byEntity: Map<Identifier, EntityTypeShape> =
        ALL_ENTITY_TYPE_SHAPES.associateBy { Identifier.vanilla(it.entity) }

    public operator fun get(type: Identifier): EntityTypeShape? = byEntity[type]

    public fun widthOf(type: Identifier): Double = (byEntity[type]?.width ?: DEFAULT_SIZE).toDouble()

    public fun heightOf(type: Identifier): Double = (byEntity[type]?.height ?: DEFAULT_SIZE).toDouble()

    /** Width of `EntityType.getSpawnAABB`, including the cube mobs' builder-only safety scale. */
    public fun spawnWidthOf(type: Identifier): Double = widthOf(type) * spawnDimensionsScale(type)

    /** Height of `EntityType.getSpawnAABB`, including the cube mobs' builder-only safety scale. */
    public fun spawnHeightOf(type: Identifier): Double = heightOf(type) * spawnDimensionsScale(type)

    /** parity: `MobCategory`, which is what a spawn cap counts and what despawn rules read. */
    public fun categoryOf(type: Identifier): String = byEntity[type]?.category ?: "misc"

    public fun isFireImmune(type: Identifier): Boolean = byEntity[type]?.fireImmune ?: false

    /** parity: the builder `immuneTo` block tags consulted by `EntityType.isBlockDangerous`. */
    public fun isImmuneTo(
        type: Identifier,
        block: String,
    ): Boolean =
        when (type.path) {
            "fox" -> block == "sweet_berry_bush"
            "polar_bear", "snow_golem", "stray" -> block == "powder_snow"
            "wither", "wither_skeleton" -> block == "wither_rose"
            else -> false
        }

    /** parity: `EntityType.canSpawnFarFromPlayer`, true by default for unknown/custom MISC types. */
    public fun canSpawnFarFromPlayer(type: Identifier): Boolean = byEntity[type]?.farFromPlayer ?: true

    /** Every type there is, for the tests that walk the whole registry. */
    public val all: List<EntityTypeShape> get() = ALL_ENTITY_TYPE_SHAPES

    /** What an unknown type is assumed to fill, which is the size of a slime ball of nothing. */
    private const val DEFAULT_SIZE = 1.0f

    private fun spawnDimensionsScale(type: Identifier): Double =
        when (type.path) {
            "magma_cube", "slime" -> 4.0
            "sulfur_cube" -> 2.0
            else -> 1.0
        }
}
