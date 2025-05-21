package org.basaltmc.entity.ecs

/**
 * A lightweight object-oriented view over one entity: the shape a future BSMod binding (or any
 * other object-style consumer) would see, sitting on top of the struct-of-arrays storage
 * everything else here uses internally (the "fachada orientada a objetos" this project's own
 * roadmap calls for over the ECS prototype). Owns no state itself beyond a `(World, EntityId)`
 * pair, so creating one is free and there is nothing to keep in sync.
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
