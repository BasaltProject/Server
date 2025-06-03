package org.basaltmc.server.entity.ecs

import org.basaltmc.server.entity.component.Position
import org.basaltmc.server.entity.component.Velocity

/**
 * Advances every entity that has both a [Position] and a [Velocity] by one tick. Iterates the
 * smaller store ([velocities], since every entity here has at most as much velocity data as
 * position data) and looks up the other by entity index, the standard sparse-set join: no
 * per-entity allocation, no temporary collections.
 */
public fun applyVelocity(
    positions: ComponentStore<Position>,
    velocities: ComponentStore<Velocity>,
) {
    velocities.forEach { entityIndex, velocity ->
        val position = positions.get(entityIndex) ?: return@forEach
        positions.set(
            entityIndex,
            Position(position.x + velocity.dx, position.y + velocity.dy, position.z + velocity.dz),
        )
    }
}
