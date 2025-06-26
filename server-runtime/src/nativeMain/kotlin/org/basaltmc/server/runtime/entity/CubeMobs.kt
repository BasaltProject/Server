package org.basaltmc.server.runtime.entity

import org.basaltmc.server.registry.EntityAttributes
import org.basaltmc.wire.common.Identifier

/**
 * The mobs whose size decides what they are.
 *
 * parity: `AbstractCubeMob.setSize`: a cube's health is the square of its size, its speed is
 * `0.2 + 0.1 * size` and its damage is the size itself, so a small slime is not a big one with a
 * different model. Without this a slime read the registry's own default of 0.7 and crossed a field
 * in seconds.
 */
public object CubeMobs {
    private val CUBES = setOf("slime", "magma_cube", "sulfur_cube")

    /** parity: the size `/summon` gives one, which is the smallest. */
    public const val DEFAULT_SIZE: Int = 1

    public fun isCube(type: Identifier): Boolean = type.path in CUBES

    /** Gives a cube the attributes its size implies. Anything else is left alone. */
    public fun applySize(
        entity: ServerEntity,
        size: Int = entity.cubeSize,
    ) {
        if (!isCube(entity.type)) return
        val actual = size.coerceIn(MIN_SIZE, MAX_SIZE)
        entity.cubeSize = actual
        entity.attributeValues[EntityAttributes.MOVEMENT_SPEED] = BASE_SPEED + SPEED_PER_SIZE * actual
        entity.attributeValues[EntityAttributes.MAX_HEALTH] = (actual * actual).toDouble()
        entity.attributeValues[EntityAttributes.ATTACK_DAMAGE] = actual.toDouble()
        entity.health = (actual * actual).toDouble()
    }

    private const val MIN_SIZE = 1
    private const val MAX_SIZE = 127
    private const val BASE_SPEED = 0.2
    private const val SPEED_PER_SIZE = 0.1
}
