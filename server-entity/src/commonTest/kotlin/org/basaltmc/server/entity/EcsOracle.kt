package org.basaltmc.server.entity

import org.basaltmc.server.entity.component.Position
import org.basaltmc.server.entity.component.Velocity
import org.basaltmc.server.entity.ecs.ComponentStore
import org.basaltmc.server.entity.ecs.EntityHandle
import org.basaltmc.server.entity.ecs.EntityId
import org.basaltmc.server.entity.ecs.World
import org.basaltmc.server.entity.ecs.applyVelocity
import kotlin.test.Test

/** The frozen Kotlin as the oracle for the entity id lifecycle and the sparse-set store. */
class EcsOracle {
    /** Ids as `index#generation`, which is what recycling a slot is observable through. */
    private fun describe(ids: List<EntityId>): String = ids.joinToString(",") { "${it.index}#${it.generation}" }

    @Test
    fun dump() {
        val world = World()
        val first = List(5) { world.create() }
        println("ORACLE created ids=${describe(first)}")

        world.destroy(first[1])
        world.destroy(first[3])
        // Destroying twice is a no-op, not a second free slot.
        world.destroy(first[1])
        val recycled = List(3) { world.create() }
        println("ORACLE recycled ids=${describe(recycled)}")
        println(
            "ORACLE alive live=${first.count { world.isAlive(it) }}" +
                " stale=${first.filterNot { world.isAlive(it) }.let { describe(it) }}" +
                " unknown=${world.isAlive(EntityId.of(99, 0))}" +
                " wrongGeneration=${world.isAlive(EntityId.of(first[1].index, 0))}",
        )

        // An index past 16 bits, which only the full low word of the packed id can hold.
        val wide = EntityId.of(70_000, 3)
        println("ORACLE wide id=${describe(listOf(wide))} raw=${wide.raw}")

        val handle = EntityHandle(world, recycled[0])
        val before = handle.isAlive
        handle.destroy()
        println("ORACLE handle before=$before after=${handle.isAlive} id=${handle.id}")

        val store = ComponentStore<String>()
        for (index in 0 until 6) store.set(index, "v$index")
        // Overwriting keeps the slot, so nothing moves.
        store.set(3, "V3")
        store.remove(0)
        store.remove(4)
        store.remove(4)
        store.set(9, "v9")
        val seen = StringBuilder()
        store.forEach { entityIndex, value -> seen.append("$entityIndex=$value;") }
        println(
            "ORACLE store size=${store.size} order=$seen has0=${store.has(0)} has3=${store.has(3)}" +
                " get3=${store.get(3)} get0=${store.get(0)} get9=${store.get(9)} far=${store.has(1000)}" +
                " has5=${store.has(5)} get5=${store.get(5)}",
        )

        val positions = ComponentStore<Position>()
        val velocities = ComponentStore<Velocity>()
        positions.set(0, Position(0.0, 64.0, 0.0))
        positions.set(1, Position(10.0, 70.0, -5.0))
        // A position with no velocity stays put, and a velocity with no position is skipped.
        positions.set(2, Position(1.0, 1.0, 1.0))
        velocities.set(1, Velocity(0.5, -0.1, 0.25))
        velocities.set(0, Velocity(-1.0, 0.0, 2.0))
        velocities.set(7, Velocity(100.0, 100.0, 100.0))
        repeat(3) { applyVelocity(positions, velocities) }
        val moved = StringBuilder()
        positions.forEach { entityIndex, position ->
            moved.append("$entityIndex=${position.x}/${position.y}/${position.z};")
        }
        println("ORACLE moved $moved")
    }
}
