package org.basaltmc.server.runtime.world

import org.basaltmc.server.registry.BlockShapes
import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.registry.EntityTypeShapes
import org.basaltmc.server.registry.hasFluid
import org.basaltmc.server.runtime.entity.Aabb
import org.basaltmc.server.runtime.entity.Mobs
import org.basaltmc.server.runtime.entity.ServerEntity
import org.basaltmc.wire.common.Identifier
import kotlin.math.ceil
import kotlin.math.floor

/** Full spawn-AABB block, entity and liquid checks shared by both natural spawn paths. */
internal object NaturalSpawnObstruction {
    fun allows(
        world: LoadedWorld,
        type: Identifier,
        x: Double,
        y: Double,
        z: Double,
    ): Boolean {
        val spawnBox = spawnBox(type, x, y, z)
        val entityBox = entityBox(type, x, y, z)
        val blockAt = { blockX: Int, blockY: Int, blockZ: Int ->
            world.state.getBlockState(blockX, blockY, blockZ)
        }
        if (!blocksAllow(spawnBox, blockAt)) return false
        if (!allowsLiquids(type) && containsAnyLiquid(entityBox, blockAt)) return false
        return !world.state.hasNaturalSpawnEntityObstruction(spawnBox, entityBox)
    }

    fun blocksAllow(
        type: Identifier,
        x: Double,
        y: Double,
        z: Double,
        blockAt: (Int, Int, Int) -> Int?,
    ): Boolean {
        if (!blocksAllow(spawnBox(type, x, y, z), blockAt)) return false
        return allowsLiquids(type) || !containsAnyLiquid(entityBox(type, x, y, z), blockAt)
    }

    internal fun allowsLiquids(type: Identifier): Boolean = type.path == "strider" || type.path in WATER_TYPES

    internal fun entityBlocksSpawn(
        entity: ServerEntity,
        spawnBox: Aabb,
        entityBox: Aabb,
    ): Boolean {
        if (entity.removed) return false
        val otherBox = Aabb.of(entity.x, entity.y, entity.z, entity.width, entity.height)
        return (canBeCollidedWith(entity.type) && otherBox.intersects(spawnBox)) ||
            (blocksBuilding(entity.type) && otherBox.intersects(entityBox))
    }

    private fun spawnBox(
        type: Identifier,
        x: Double,
        y: Double,
        z: Double,
    ): Aabb = Aabb.of(x, y, z, EntityTypeShapes.spawnWidthOf(type), EntityTypeShapes.spawnHeightOf(type))

    private fun entityBox(
        type: Identifier,
        x: Double,
        y: Double,
        z: Double,
    ): Aabb = Aabb.of(x, y, z, EntityTypeShapes.widthOf(type), EntityTypeShapes.heightOf(type))

    private inline fun blocksAllow(
        box: Aabb,
        blockAt: (Int, Int, Int) -> Int?,
    ): Boolean =
        !anyBlock(box) { x, y, z ->
            val state = blockAt(x, y, z) ?: return@anyBlock false
            BlockShapes.collisionBoxes(state).any { shape ->
                Aabb(
                    x + shape[0] / 16.0,
                    y + shape[1] / 16.0,
                    z + shape[2] / 16.0,
                    x + shape[3] / 16.0,
                    y + shape[4] / 16.0,
                    z + shape[5] / 16.0,
                ).intersects(box)
            }
        }

    private inline fun containsAnyLiquid(
        box: Aabb,
        blockAt: (Int, Int, Int) -> Int?,
    ): Boolean = anyBlock(box) { x, y, z -> blockAt(x, y, z)?.let { Blocks.hasFluid(it) } == true }

    private inline fun anyBlock(
        box: Aabb,
        predicate: (Int, Int, Int) -> Boolean,
    ): Boolean {
        val maxX = ceil(box.maxX).toInt()
        val maxY = ceil(box.maxY).toInt()
        val maxZ = ceil(box.maxZ).toInt()
        for (x in floor(box.minX).toInt() until maxX) {
            for (y in floor(box.minY).toInt() until maxY) {
                for (z in floor(box.minZ).toInt() until maxZ) {
                    if (predicate(x, y, z)) return true
                }
            }
        }
        return false
    }

    private val WATER_TYPES =
        setOf(
            "axolotl",
            "cod",
            "dolphin",
            "drowned",
            "elder_guardian",
            "glow_squid",
            "guardian",
            "nautilus",
            "pufferfish",
            "salmon",
            "squid",
            "tadpole",
            "tropical_fish",
            "zombie_nautilus",
        )

    /** parity: the three vanilla entity families whose base collision shape participates in `noCollision(null, box)`. */
    private fun canBeCollidedWith(type: Identifier): Boolean =
        type.path == "shulker" ||
            type.path == "happy_ghast" ||
            type.path.endsWith("_boat") ||
            type.path.endsWith("_raft")

    /** parity: living entities and the non-living constructors that set `Entity.blocksBuilding`. */
    private fun blocksBuilding(type: Identifier): Boolean =
        Mobs.isMob(type) ||
            type.path == "armor_stand" ||
            type.path == "end_crystal" ||
            type.path == "falling_block" ||
            type.path == "tnt" ||
            type.path == "minecart" ||
            type.path.endsWith("_minecart") ||
            type.path.endsWith("_boat") ||
            type.path.endsWith("_raft")
}
