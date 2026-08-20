package org.basaltmc.server.entity.ecs

/**
 * A lightweight object view over one entity: the shape a plugin binding sees, sitting on top of the
 * struct-of-arrays storage everything here uses internally. Owns no state beyond a
 * `(World, EntityId)` pair, so creating one is free and there is nothing to keep in sync.
 */
public class EntityHandle(
    public val world: World,
    public val id: EntityId,
) {
    public val isAlive: Boolean get() = world.isAlive(id)

    public fun destroy() {
        world.destroy(id)
    }
}
