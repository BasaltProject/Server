package org.basaltmc.server.runtime

import org.basaltmc.server.plugins.PluginEventBus
import org.basaltmc.server.plugins.PluginEventTypes
import org.basaltmc.server.random.LegacyRandom
import org.basaltmc.server.registry.EntityTypes
import org.basaltmc.server.runtime.entity.EntityNbt
import org.basaltmc.server.runtime.entity.MobCategory
import org.basaltmc.server.runtime.entity.MobPersistence
import org.basaltmc.server.runtime.entity.Mobs
import org.basaltmc.server.runtime.entity.ServerEntity
import org.basaltmc.server.runtime.player.PlayerRegistry
import org.basaltmc.server.runtime.plugins.emit
import org.basaltmc.server.runtime.sched.Region
import org.basaltmc.server.runtime.world.GameRuleRegistry
import org.basaltmc.server.runtime.world.LoadedWorld
import org.basaltmc.server.runtime.world.LocalMobCaps
import org.basaltmc.server.runtime.world.MobCapPlayer
import org.basaltmc.server.runtime.world.MobSpawnFinalization
import org.basaltmc.server.runtime.world.MobSpawnForest
import org.basaltmc.server.runtime.world.NaturalSpawner
import org.basaltmc.server.runtime.world.RealBiomeSpawns
import org.basaltmc.server.runtime.world.SpawnEquipmentFinalizer
import org.basaltmc.server.runtime.world.SpawnPotential
import org.basaltmc.server.runtime.world.SpawnRequest
import org.basaltmc.server.runtime.world.writeGeneratedForest
import org.basaltmc.server.world.ChunkGeneratedEntity
import org.basaltmc.wire.common.Identifier
import org.basaltmc.wire.nbt.NbtCompound
import org.basaltmc.wire.protocol.play.Gamemode
import kotlin.math.floor
import kotlin.random.Random
import kotlin.uuid.Uuid

/**
 * What fills the world around the players with the mobs their biomes offer, and how a plugin or
 * another system asks for one mob in particular.
 */
public class NaturalSpawningSystem(
    private val playerRegistry: PlayerRegistry,
    private val pluginEvents: PluginEventBus,
    private val biomeSpawns: RealBiomeSpawns,
    private val chunkSimulation: ChunkSimulationView,
    private val equipmentFinalizer: SpawnEquipmentFinalizer,
) {
    /** One mob of this type where it was asked for, or null when the type is not one this build has. */
    public fun spawnMob(
        world: LoadedWorld,
        type: Identifier,
        x: Double,
        y: Double,
        z: Double,
        yaw: Float = 0f,
        /** Why it is being made, which is the one thing a plugin always wants to filter on. */
        reason: String = "natural",
        /** Common/type-specific birth state, applied before the entity becomes visible to the world. */
        initialize: ((ServerEntity) -> Unit)? = null,
    ): ServerEntity? {
        val protocolType = EntityTypes[type]?.protocolId ?: return null
        if (!spawnAllowed(world, type, x, y, z, reason)) return null
        val entity =
            ServerEntity(
                entityId = playerRegistry.allocateEntityId(),
                uuid = Uuid.random(),
                type = type,
                protocolType = protocolType,
                x = x,
                y = y,
                z = z,
                yaw = yaw,
            )
        initialize?.invoke(entity)
        world.state.spawnEntity(entity)
        return entity
    }

    /**
     * Fills the world around the players with the mobs their biomes offer.
     *
     * parity: `NaturalSpawner.spawnForChunk`, once per simulated chunk per tick, under a cap per
     * category that scales with how much of the world is being simulated. `doMobSpawning` turns the
     * whole thing off, as it does in vanilla.
     */
    public fun tickSpawning(
        world: LoadedWorld,
        region: Region,
    ) {
        // Global and local caps are one world-wide snapshot. Running this independently in every
        // region made the first distant player consume the cap of every other one.
        if (!region.isPrimary) return
        if (!world.gameRules[GameRuleRegistry.SPAWN_MOBS]) return
        val watchers =
            playerRegistry.snapshot().filter {
                it.worldName == world.name && it.gamemode != Gamemode.SPECTATOR
            }
        if (watchers.isEmpty()) return
        val localCaps = LocalMobCaps(watchers.map { MobCapPlayer(it.entityId, it.x, it.z) })
        val chunks = chunkSimulation.simulatedChunks(world).filter { localCaps.isSpawnCandidate(it[0], it[1]) }
        if (chunks.isEmpty()) return

        // parity: NaturalSpawner.createState excludes both PersistenceRequired and each mob's
        // requiresCustomPersistence override before global caps, local caps and spawn costs.
        val alive = world.state.entities().filter { !it.removed && !MobPersistence.keeps(it) }
        val random = LegacyRandom(Random.nextLong())
        val categories = NaturalSpawner.categoriesFor(world.difficulty, world.gameTime)
        val cappedCategories = MobCategory.entries.filter { it != MobCategory.MISC }
        val caps = cappedCategories.associateWith { NaturalSpawner.capFor(it, chunks.size) }
        val counts =
            cappedCategories.associateWithTo(mutableMapOf()) { category ->
                alive.count { Mobs.categoryOf(it.type) == category.categoryName }
            }
        val spawnPotential = SpawnPotential()
        for (entity in alive) {
            val blockX = floor(entity.x).toInt()
            val blockY = floor(entity.y).toInt()
            val blockZ = floor(entity.z).toInt()
            val cost =
                NaturalSpawner.biomeAt(world, blockX, blockY, blockZ)?.let { biome ->
                    biomeSpawns.costFor(biome, entity.type)
                }
            if (cost != null) spawnPotential.add(blockX, blockY, blockZ, cost.charge)
            val category = MobCategory.byName(Mobs.categoryOf(entity.type)) ?: continue
            localCaps.add(category, blockX shr 4, blockZ shr 4)
        }
        val shuffled = chunks.toMutableList()
        for (index in shuffled.lastIndex downTo 1) {
            val swap = random.nextInt(index + 1)
            val held = shuffled[index]
            shuffled[index] = shuffled[swap]
            shuffled[swap] = held
        }
        // parity: ServerChunkCache shuffles once, then gives every spawning chunk every category.
        for (chunk in shuffled) {
            for (category in categories) {
                val cap = caps.getValue(category)
                if (counts.getValue(category) >= cap) continue
                if (!localCaps.canSpawn(category, chunk[0], chunk[1])) continue
                NaturalSpawner.spawnForChunk(
                    world,
                    chunk[0],
                    chunk[1],
                    category,
                    entriesFor = { biome -> biomeSpawns.listFor(biome, category.categoryName) },
                    playerDistanceSquared = { x, y, z ->
                        watchers.minOfOrNull { player ->
                            val dx = player.x - x
                            val dy = player.y - y
                            val dz = player.z - z
                            dx * dx + dy * dy + dz * dz
                        }
                    },
                    random = random,
                    requestAccepted = { request ->
                        val blockX = floor(request.x).toInt()
                        val blockY = floor(request.y).toInt()
                        val blockZ = floor(request.z).toInt()
                        val cost =
                            NaturalSpawner.biomeAt(world, blockX, blockY, blockZ)?.let { biome ->
                                biomeSpawns.costFor(biome, request.type)
                            }
                        if (cost != null && !spawnPotential.allows(blockX, blockY, blockZ, cost)) {
                            false
                        } else {
                            val spawned = materializeNaturalForest(world, request, blockX, blockY, blockZ, random)
                            if (spawned == null) {
                                false
                            } else {
                                if (cost != null) spawnPotential.add(blockX, blockY, blockZ, cost.charge)
                                val actualCategory = MobCategory.byName(Mobs.categoryOf(request.type)) ?: category
                                counts[actualCategory] = counts.getValue(actualCategory) + 1
                                localCaps.add(actualCategory, blockX shr 4, blockZ shr 4)
                                true
                            }
                        }
                    },
                )
            }
        }
    }

    private fun materializeNaturalForest(
        world: LoadedWorld,
        request: SpawnRequest,
        blockX: Int,
        blockY: Int,
        blockZ: Int,
        random: LegacyRandom,
    ): ServerEntity? {
        if (!spawnAllowed(world, request.type, request.x, request.y, request.z, "natural")) return null
        val inhabitedTime =
            world.state
                .chunkIfLoaded(blockX shr 4, blockZ shr 4)
                ?.inhabitedTime ?: 0L
        val moonBrightness =
            if (world.declaration.dimension.hasClock) {
                NaturalSpawner.surfaceSlimeChance(world.clock.totalTicks) * 2f
            } else {
                0f
            }
        val specialMultiplier =
            MobSpawnFinalization.specialMultiplier(
                world.difficulty,
                world.gameTime,
                inhabitedTime,
                moonBrightness,
            )
        val forest =
            writeGeneratedForest(
                NbtCompound(),
                request.type,
                request.groupIndex,
                random,
                specialMultiplier,
                group = request.group,
                biome = request.biome,
                blockX = blockX,
                blockY = blockY,
                blockZ = blockZ,
                gameTime = world.gameTime,
                difficulty = world.difficulty,
                equipmentFinalizer = equipmentFinalizer,
                entityX = request.x,
                entityY = request.y,
                entityZ = request.z,
                entityYaw = request.yaw,
                entityRandom = LegacyRandom(Random.nextLong()),
            )
        return materializeForest(world, forest)
    }

    private fun materializeForest(
        world: LoadedWorld,
        forest: MobSpawnForest,
    ): ServerEntity? {
        var main: ServerEntity? = null

        fun materialize(
            descriptor: ChunkGeneratedEntity,
            vehicle: ServerEntity?,
        ) {
            if (descriptor !== forest.main &&
                !spawnAllowed(world, descriptor.type, descriptor.x, descriptor.y, descriptor.z, "jockey")
            ) {
                return
            }
            val protocolType = EntityTypes[descriptor.type]?.protocolId ?: return
            val entity =
                EntityNbt.decodeGenerated(
                    descriptor,
                    playerRegistry.allocateEntityId(),
                    protocolType,
                ) ?: return
            if (vehicle != null) {
                entity.vehicleId = vehicle.entityId
                vehicle.passengerIds += entity.entityId
            }
            world.state.spawnEntity(entity)
            if (descriptor === forest.main) main = entity
            descriptor.passengers.forEach { passenger -> materialize(passenger, entity) }
        }

        forest.roots.forEach { root -> materialize(root, null) }
        return main
    }

    private fun spawnAllowed(
        world: LoadedWorld,
        type: Identifier,
        x: Double,
        y: Double,
        z: Double,
        reason: String,
    ): Boolean {
        val event =
            pluginEvents.emit(PluginEventTypes.ENTITY_SPAWN) { spawning ->
                spawning.set(0, type.toString())
                spawning.set(1, world.name)
                spawning.set(2, x)
                spawning.set(3, y)
                spawning.set(4, z)
                spawning.set(5, reason)
            }
        return event?.cancelled != true
    }
}
