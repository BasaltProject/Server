package org.basaltmc.server.runtime.world

import org.basaltmc.server.registry.BlockStateDefinitions

/**
 * Primitives shared by more than one block-family growth object.
 *
 * Kept apart from any single family so none of them has to depend on another for a horizontal
 * offset or a light threshold.
 */
internal const val GROWTH_LIGHT: Int = 9

internal val HORIZONTAL_NAMES: List<String> = listOf("north", "south", "west", "east")
internal val FACE_NAMES: List<String> = listOf("down", "up", "north", "south", "west", "east")

internal fun set(
    world: LoadedWorld,
    x: Int,
    y: Int,
    z: Int,
    stateId: Int,
    changes: MutableList<BlockChange>,
) {
    if (!world.state.setBlockState(x, y, z, stateId)) return
    changes += BlockChange(x, y, z, stateId)
}

internal fun offsetX(facing: String): Int =
    if (facing == "west") {
        -1
    } else if (facing == "east") {
        1
    } else {
        0
    }

internal fun offsetY(facing: String): Int =
    if (facing == "down") {
        -1
    } else if (facing == "up") {
        1
    } else {
        0
    }

internal fun offsetZ(facing: String): Int =
    if (facing == "north") {
        -1
    } else if (facing == "south") {
        1
    } else {
        0
    }

/** Advances an age property by one, shared by every simple age-based crop. */
internal fun grow(
    world: LoadedWorld,
    x: Int,
    y: Int,
    z: Int,
    stateId: Int,
    maxAge: Int,
    changes: MutableList<BlockChange>,
) {
    val block = BlockStateDefinitions.blockOf(stateId) ?: return
    val age = BlockStateDefinitions.propertiesOf(stateId)["age"]?.toIntOrNull() ?: return
    if (age >= maxAge) return
    val older = BlockStateDefinitions.stateIdOf(block, mapOf("age" to (age + 1).toString()), stateId) ?: return
    set(world, x, y, z, older, changes)
}
