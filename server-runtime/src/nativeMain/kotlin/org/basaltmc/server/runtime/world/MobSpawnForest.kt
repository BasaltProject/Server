package org.basaltmc.server.runtime.world

import org.basaltmc.server.random.BasaltRandom
import org.basaltmc.server.world.ChunkGeneratedEntity
import org.basaltmc.wire.common.Identifier
import org.basaltmc.wire.nbt.NbtCompound

/** Mutable vehicle/passenger topology assembled while concrete mob finalizers recurse. */
internal class MobSpawnForestBuilder {
    private val nodes = mutableListOf<MobSpawnNode>()

    fun create(
        type: Identifier,
        x: Double,
        y: Double,
        z: Double,
        yaw: Float,
        pitch: Float = 0f,
        data: NbtCompound = NbtCompound(),
    ): MobSpawnNode = MobSpawnNode(type, x, y, z, yaw, pitch, data).also(nodes::add)

    fun import(descriptor: ChunkGeneratedEntity): MobSpawnNode {
        val node =
            create(
                descriptor.type,
                descriptor.x,
                descriptor.y,
                descriptor.z,
                descriptor.yaw,
                descriptor.pitch,
                descriptor.data,
            )
        descriptor.passengers.forEach { passenger -> mount(import(passenger), node, force = true) }
        return node
    }

    /** parity: `Entity.startRiding`, including forced reparenting after recursive finalization. */
    fun mount(
        passenger: MobSpawnNode,
        vehicle: MobSpawnNode,
        force: Boolean,
    ): Boolean {
        if (passenger === vehicle || vehicle.hasVehicle(passenger)) return false
        val previous = passenger.vehicle
        if (previous === vehicle) return true
        if (previous != null && !force) return false
        previous?.passengers?.remove(passenger)
        passenger.vehicle = vehicle
        vehicle.passengers += passenger
        return true
    }

    fun freeze(main: MobSpawnNode): MobSpawnForest {
        val descriptors = mutableMapOf<MobSpawnNode, ChunkGeneratedEntity>()

        fun freezeNode(node: MobSpawnNode): ChunkGeneratedEntity {
            val descriptor =
                ChunkGeneratedEntity(
                    type = node.type,
                    x = node.x,
                    y = node.y,
                    z = node.z,
                    yaw = node.yaw,
                    pitch = node.pitch,
                    data = node.data,
                    passengers = node.passengers.map(::freezeNode),
                )
            descriptors[node] = descriptor
            return descriptor
        }

        val roots = nodes.filter { it.vehicle == null }.map(::freezeNode)
        return MobSpawnForest(checkNotNull(descriptors[main]), roots)
    }
}

internal class MobSpawnNode(
    val type: Identifier,
    val x: Double,
    val y: Double,
    val z: Double,
    val yaw: Float,
    val pitch: Float,
    val data: NbtCompound,
) {
    var vehicle: MobSpawnNode? = null
    val passengers: MutableList<MobSpawnNode> = mutableListOf()

    fun hasVehicle(candidate: MobSpawnNode): Boolean {
        var current = vehicle
        while (current != null) {
            if (current === candidate) return true
            current = current.vehicle
        }
        return false
    }
}

internal data class MobSpawnForest(
    val main: ChunkGeneratedEntity,
    val roots: List<ChunkGeneratedEntity>,
)

internal enum class MobSpawnReason {
    NATURAL,
    CHUNK_GENERATION,
    STRUCTURE,
    JOCKEY,
}

/** Builds the complete generated forest while preserving which descriptor is the requested mob. */
internal fun writeGeneratedForest(
    data: NbtCompound,
    type: Identifier,
    groupIndex: Int,
    random: BasaltRandom,
    specialMultiplier: Float = 0f,
    group: MobSpawnGroupData = MobSpawnGroupData(),
    biome: Identifier? = null,
    blockX: Int = 0,
    blockY: Int = 0,
    blockZ: Int = 0,
    gameTime: Long = 0L,
    difficulty: String = "easy",
    equipmentFinalizer: SpawnEquipmentFinalizer? = null,
    entityX: Double = blockX + 0.5,
    entityY: Double = blockY.toDouble(),
    entityZ: Double = blockZ + 0.5,
    entityYaw: Float = 0f,
    entityRandom: BasaltRandom = random,
    reason: MobSpawnReason = MobSpawnReason.NATURAL,
    entityRandomFactory: () -> BasaltRandom = { entityRandom },
): MobSpawnForest {
    val builder = MobSpawnForestBuilder()
    val main = builder.create(type, entityX, entityY, entityZ, entityYaw, data = data)
    MobSpawnFinalization.writeGeneratedNode(
        builder,
        main,
        groupIndex,
        random,
        specialMultiplier,
        group,
        biome,
        blockX,
        blockY,
        blockZ,
        gameTime,
        difficulty,
        equipmentFinalizer ?: spawnEquipmentWithoutAssets(),
        reason,
        entityRandom,
        entityRandomFactory,
    )
    return builder.freeze(main)
}
