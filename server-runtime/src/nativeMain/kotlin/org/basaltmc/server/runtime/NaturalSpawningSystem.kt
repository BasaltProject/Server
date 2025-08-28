package org.basaltmc.server.runtime

import org.basaltmc.server.plugins.PluginEventBus
import org.basaltmc.server.plugins.PluginEventTypes
import org.basaltmc.server.random.LegacyRandom
import org.basaltmc.server.registry.EntityTypeShapes
import org.basaltmc.server.registry.EntityTypes
import org.basaltmc.server.runtime.entity.Aabb
import org.basaltmc.server.runtime.entity.EntityCollision
import org.basaltmc.server.runtime.entity.EntityNbt
import org.basaltmc.server.runtime.entity.MobCategory
import org.basaltmc.server.runtime.entity.MobPersistence
import org.basaltmc.server.runtime.entity.Mobs
import org.basaltmc.server.runtime.entity.ServerEntity
import org.basaltmc.server.runtime.player.PlayerRegistry
import org.basaltmc.server.runtime.player.PlayerSession
import org.basaltmc.server.runtime.plugins.emit
import org.basaltmc.server.runtime.sched.Region
import org.basaltmc.server.runtime.world.BlockPlacement
import org.basaltmc.server.runtime.world.CatSpawnRequest
import org.basaltmc.server.runtime.world.GameRuleRegistry
import org.basaltmc.server.runtime.world.LoadedWorld
import org.basaltmc.server.runtime.world.LocalMobCaps
import org.basaltmc.server.runtime.world.MobCapPlayer
import org.basaltmc.server.runtime.world.MobLocalDifficulty
import org.basaltmc.server.runtime.world.MobSpawnEnvironment
import org.basaltmc.server.runtime.world.MobSpawnForest
import org.basaltmc.server.runtime.world.MobSpawnGroupData
import org.basaltmc.server.runtime.world.MobSpawnReason
import org.basaltmc.server.runtime.world.NaturalSpawner
import org.basaltmc.server.runtime.world.PatrolSpawnRequest
import org.basaltmc.server.runtime.world.RealBiomeSpawns
import org.basaltmc.server.runtime.world.SkeletonHorseTrap
import org.basaltmc.server.runtime.world.SpawnEquipmentFinalizer
import org.basaltmc.server.runtime.world.SpawnPotential
import org.basaltmc.server.runtime.world.SpawnRequest
import org.basaltmc.server.runtime.world.TraderLlamaSpawnRequest
import org.basaltmc.server.runtime.world.VillageSiegeSpawnRequest
import org.basaltmc.server.runtime.world.WanderingTraderSpawnRequest
import org.basaltmc.server.runtime.world.relocateMain
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

    /** The EVENT horse made by `ServerLevel.tickThunder`; it deliberately skips `finalizeSpawn`. */
    internal fun spawnSkeletonTrap(
        world: LoadedWorld,
        x: Int,
        y: Int,
        z: Int,
    ): ServerEntity? =
        spawnMob(
            world,
            Identifier.vanilla("skeleton_horse"),
            x.toDouble(),
            y.toDouble(),
            z.toDouble(),
            reason = "event",
        ) { horse ->
            horse.skeletonTrap = true
            horse.skeletonTrapTime = 0
            horse.age = 0
        }

    /** The TRIGGERED rider forest created when a living player comes within ten blocks. */
    internal fun activateSkeletonTrap(
        world: LoadedWorld,
        horse: ServerEntity,
        random: LegacyRandom,
    ): Boolean {
        if (horse.type.path != "skeleton_horse" || !horse.skeletonTrap || horse.removed) return false
        val forest =
            SkeletonHorseTrap.activate(
                world,
                horse,
                levelRandom = random,
                horseRandom = LegacyRandom(Random.nextLong()),
                entityRandomFactory = { LegacyRandom(Random.nextLong()) },
                equipment = equipmentFinalizer,
            )
        materializeForest(
            world,
            forest,
            eventReason = "triggered",
            checkMain = true,
        ) { entity, _ ->
            entity.hurtCooldown = SkeletonHorseTrap.ACTIVATION_INVULNERABLE_TICKS
        }
        return true
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
        val random = LegacyRandom(Random.nextLong())
        val worldPlayers = playerRegistry.snapshot().filter { it.worldName == world.name }
        val watchers =
            worldPlayers.filter { it.gamemode != Gamemode.SPECTATOR }
        if (watchers.isNotEmpty()) tickNaturalSpawning(world, watchers, random)
        world.phantomSpawner.tick(
            world,
            spawnEnemies = world.difficulty != "peaceful" && world.gameRules[GameRuleRegistry.SPAWN_MONSTERS],
            players = worldPlayers,
            random = random,
        ) { request ->
            val blockX = floor(request.x).toInt()
            val blockY = floor(request.y).toInt()
            val blockZ = floor(request.z).toInt()
            materializeNaturalForest(world, request, blockX, blockY, blockZ, random)
        }
        world.patrolSpawner.tick(
            world,
            spawnEnemies = world.difficulty != "peaceful" && world.gameRules[GameRuleRegistry.SPAWN_MONSTERS],
            players = worldPlayers,
            random = random,
        ) { request ->
            materializePatrolMember(world, request, random) != null
        }
        world.catSpawner.tick(world, worldPlayers, random) { request ->
            materializeCat(world, request, random)
        }
        world.villageSiegeSpawner.tick(
            world,
            spawnEnemies = world.difficulty != "peaceful" && world.gameRules[GameRuleRegistry.SPAWN_MONSTERS],
            players = worldPlayers,
            random = random,
        ) { request ->
            materializeVillageSiegeZombie(world, request, random)
        }
        world.wanderingTraderSpawner.tick(
            world = world,
            players = worldPlayers,
            levelRandom = random,
            spawnTrader = { request -> materializeWanderingTrader(world, request, random)?.entityId },
            spawnLlama = { request, traderId -> materializeTraderLlama(world, request, traderId, random) },
        )
    }

    private fun tickNaturalSpawning(
        world: LoadedWorld,
        watchers: List<PlayerSession>,
        random: LegacyRandom,
    ) {
        val localCaps = LocalMobCaps(watchers.map { MobCapPlayer(it.entityId, it.x, it.z) })
        val chunks = chunkSimulation.simulatedChunks(world).filter { localCaps.isSpawnCandidate(it[0], it[1]) }
        if (chunks.isEmpty()) return

        // parity: NaturalSpawner.createState excludes both PersistenceRequired and each mob's
        // requiresCustomPersistence override before global caps, local caps and spawn costs.
        val alive = world.state.entities().filter { !it.removed && !MobPersistence.keeps(it) }
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
    ): ServerEntity? =
        materializeSpawnForest(
            world,
            request,
            blockX,
            blockY,
            blockZ,
            random,
            MobSpawnReason.NATURAL,
            "natural",
        )

    private fun materializePatrolMember(
        world: LoadedWorld,
        patrol: PatrolSpawnRequest,
        random: LegacyRandom,
    ): ServerEntity? {
        val group =
            MobSpawnGroupData().also {
                it.patrolLeader = patrol.leader
            }
        val request =
            SpawnRequest(
                Identifier.vanilla("pillager"),
                patrol.x,
                patrol.y,
                patrol.z,
                yaw = 0f,
                group = group,
                biome = patrol.biome,
            )
        return materializeSpawnForest(
            world,
            request,
            floor(patrol.x).toInt(),
            floor(patrol.y).toInt(),
            floor(patrol.z).toInt(),
            random,
            MobSpawnReason.PATROL,
            "patrol",
        )
    }

    private fun materializeCat(
        world: LoadedWorld,
        cat: CatSpawnRequest,
        random: LegacyRandom,
    ): ServerEntity? {
        val group = MobSpawnGroupData().also { it.catSpawnContext = cat.context }
        val request =
            SpawnRequest(
                Identifier.vanilla("cat"),
                cat.x,
                cat.y,
                cat.z,
                yaw = 0f,
                group = group,
                biome = cat.biome,
            )
        return materializeSpawnForest(
            world,
            request,
            floor(cat.x).toInt(),
            floor(cat.y).toInt(),
            floor(cat.z).toInt(),
            random,
            MobSpawnReason.NATURAL,
            "natural",
        )?.also { it.persistent = cat.persistent }
    }

    /** VillageSiege finalizes a zero-position Zombie and only then moves that zombie, not its vehicle. */
    private fun materializeVillageSiegeZombie(
        world: LoadedWorld,
        siege: VillageSiegeSpawnRequest,
        random: LegacyRandom,
    ): ServerEntity? {
        val zombie = Identifier.vanilla("zombie")
        if (!spawnAllowed(world, zombie, siege.x, siege.y, siege.z, "event")) return null
        val inhabitedTime = world.state.chunkIfLoaded(0, 0)?.inhabitedTime ?: 0L
        val specialMultiplier =
            MobLocalDifficulty.specialMultiplier(
                world.difficulty,
                world.gameTime,
                inhabitedTime,
                NaturalSpawner.surfaceSlimeChance(world.clock.totalTicks) * 2f,
            )
        val forest =
            writeGeneratedForest(
                NbtCompound(),
                zombie,
                groupIndex = 0,
                random = random,
                specialMultiplier = specialMultiplier,
                group = MobSpawnGroupData(),
                biome = NaturalSpawner.biomeAt(world, 0, 0, 0),
                blockX = 0,
                blockY = 0,
                blockZ = 0,
                gameTime = world.gameTime,
                difficulty = world.difficulty,
                equipmentFinalizer = equipmentFinalizer,
                entityX = 0.0,
                entityY = 0.0,
                entityZ = 0.0,
                entityYaw = 0f,
                entityRandom = LegacyRandom(Random.nextLong()),
                reason = MobSpawnReason.EVENT,
                entityRandomFactory = { LegacyRandom(Random.nextLong()) },
                environment = spawnEnvironment(world),
            )
        val yaw = random.nextFloat() * FULL_ROTATION
        return materializeForest(world, forest.relocateMain(siege.x, siege.y, siege.z, yaw))
    }

    private fun materializeWanderingTrader(
        world: LoadedWorld,
        request: WanderingTraderSpawnRequest,
        random: LegacyRandom,
    ): ServerEntity? {
        val trader =
            materializeEventMob(
                world,
                Identifier.vanilla("wandering_trader"),
                request.x,
                request.y,
                request.z,
                random,
            ) ?: return null
        trader.despawnDelay = WANDERING_TRADER_DESPAWN_DELAY
        trader.wanderTarget = intArrayOf(request.referenceX, request.referenceY, request.referenceZ)
        trader.homePosition = intArrayOf(request.referenceX, request.referenceY, request.referenceZ)
        trader.homeRadius = WANDERING_TRADER_HOME_RADIUS
        return trader
    }

    private fun materializeTraderLlama(
        world: LoadedWorld,
        request: TraderLlamaSpawnRequest,
        traderId: Int,
        random: LegacyRandom,
    ) {
        val trader = world.state.entity(traderId)?.takeIf { !it.removed } ?: return
        val llama =
            materializeEventMob(
                world,
                Identifier.vanilla("trader_llama"),
                request.x,
                request.y,
                request.z,
                random,
            ) ?: return
        llama.leashHolderId = trader.entityId
        llama.leashHolderUuid = trader.uuid.toString()
    }

    /** EntityType.spawn: level yaw first, then EVENT finalization at the already-positioned mob. */
    private fun materializeEventMob(
        world: LoadedWorld,
        type: Identifier,
        x: Double,
        y: Double,
        z: Double,
        random: LegacyRandom,
    ): ServerEntity? {
        if (!spawnAllowed(world, type, x, y, z, "event")) return null
        val blockX = floor(x).toInt()
        val blockY = floor(y).toInt()
        val blockZ = floor(z).toInt()
        val inhabitedTime = world.state.chunkIfLoaded(blockX shr 4, blockZ shr 4)?.inhabitedTime ?: 0L
        val specialMultiplier =
            MobLocalDifficulty.specialMultiplier(
                world.difficulty,
                world.gameTime,
                inhabitedTime,
                NaturalSpawner.surfaceSlimeChance(world.clock.totalTicks) * 2f,
            )
        val yaw = random.nextFloat() * FULL_ROTATION
        val forest =
            writeGeneratedForest(
                NbtCompound(),
                type,
                groupIndex = 0,
                random = random,
                specialMultiplier = specialMultiplier,
                group = MobSpawnGroupData(),
                biome = NaturalSpawner.biomeAt(world, blockX, blockY, blockZ),
                blockX = blockX,
                blockY = blockY,
                blockZ = blockZ,
                gameTime = world.gameTime,
                difficulty = world.difficulty,
                equipmentFinalizer = equipmentFinalizer,
                entityX = x,
                entityY = y,
                entityZ = z,
                entityYaw = yaw,
                entityRandom = LegacyRandom(Random.nextLong()),
                reason = MobSpawnReason.EVENT,
                entityRandomFactory = { LegacyRandom(Random.nextLong()) },
                environment = spawnEnvironment(world),
            )
        return materializeForest(world, forest)
    }

    private fun materializeSpawnForest(
        world: LoadedWorld,
        request: SpawnRequest,
        blockX: Int,
        blockY: Int,
        blockZ: Int,
        random: LegacyRandom,
        reason: MobSpawnReason,
        eventReason: String,
    ): ServerEntity? {
        if (!spawnAllowed(world, request.type, request.x, request.y, request.z, eventReason)) return null
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
            MobLocalDifficulty.specialMultiplier(
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
                reason = reason,
                entityRandomFactory = { LegacyRandom(Random.nextLong()) },
                environment = spawnEnvironment(world),
            )
        return materializeForest(world, forest)
    }

    private fun materializeForest(
        world: LoadedWorld,
        forest: MobSpawnForest,
        eventReason: String = "jockey",
        checkMain: Boolean = false,
        initialize: (ServerEntity, ChunkGeneratedEntity) -> Unit = { _, _ -> },
    ): ServerEntity? {
        var main: ServerEntity? = null

        fun materialize(
            descriptor: ChunkGeneratedEntity,
            vehicle: ServerEntity?,
        ) {
            if ((descriptor !== forest.main || checkMain) &&
                !spawnAllowed(world, descriptor.type, descriptor.x, descriptor.y, descriptor.z, eventReason)
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
            val externalVehicleId =
                forest.externalVehicleLinks.firstOrNull { it.passenger === descriptor }?.vehicleId
            val resolvedVehicle = vehicle ?: externalVehicleId?.let(world.state::entity)
            if (resolvedVehicle != null) {
                entity.vehicleId = resolvedVehicle.entityId
                resolvedVehicle.passengerIds += entity.entityId
                if (resolvedVehicle.type.path == "chicken") {
                    val vehicleData = resolvedVehicle.sourceNbt ?: NbtCompound().also { resolvedVehicle.sourceNbt = it }
                    vehicleData.putBoolean("IsChickenJockey", true)
                }
            }
            initialize(entity, descriptor)
            world.state.spawnEntity(entity)
            if (descriptor === forest.main) main = entity
            descriptor.passengers.forEach { passenger -> materialize(passenger, entity) }
        }

        forest.roots.forEach { root -> materialize(root, null) }
        return main
    }

    private fun spawnEnvironment(world: LoadedWorld): MobSpawnEnvironment =
        MobSpawnEnvironment(
            nearbyUnriddenChicken = { zombie ->
                val scale = if (zombie.data.getBoolean("IsBaby")) 0.5 else 1.0
                val source =
                    Aabb.of(
                        zombie.x,
                        zombie.y,
                        zombie.z,
                        EntityTypeShapes.widthOf(zombie.type) * scale,
                        EntityTypeShapes.heightOf(zombie.type) * scale,
                    )
                val search =
                    Aabb(
                        source.minX - CHICKEN_SEARCH_HORIZONTAL,
                        source.minY - CHICKEN_SEARCH_VERTICAL,
                        source.minZ - CHICKEN_SEARCH_HORIZONTAL,
                        source.maxX + CHICKEN_SEARCH_HORIZONTAL,
                        source.maxY + CHICKEN_SEARCH_VERTICAL,
                        source.maxZ + CHICKEN_SEARCH_HORIZONTAL,
                    )
                world.state
                    .entities()
                    .firstOrNull { candidate ->
                        !candidate.removed &&
                            candidate.health > 0.0 &&
                            candidate.type.path == "chicken" &&
                            candidate.vehicleId == null &&
                            candidate.passengerIds.isEmpty() &&
                            entityBox(candidate).intersects(search)
                    }?.entityId
            },
            canSpawnEntity = { type, x, y, z ->
                val box = Aabb.of(x, y, z, EntityTypeShapes.widthOf(type), EntityTypeShapes.heightOf(type))
                !EntityCollision.collides(world.state, box) &&
                    world.state.entities().none { candidate ->
                        !candidate.removed && entityBox(candidate).intersects(box)
                    } &&
                    playerRegistry.snapshot().none { player ->
                        player.worldName == world.name &&
                            player.gamemode != Gamemode.SPECTATOR &&
                            playerBox(player.x, player.y, player.z, player.sneaking).intersects(box)
                    }
            },
        )

    private fun entityBox(entity: ServerEntity): Aabb =
        Aabb.of(entity.x, entity.y, entity.z, entity.width, entity.height)

    private fun playerBox(
        x: Double,
        y: Double,
        z: Double,
        sneaking: Boolean,
    ): Aabb {
        val packed = BlockPlacement.playerBox(x, y, z, sneaking)
        return Aabb(packed[0], packed[1], packed[2], packed[3], packed[4], packed[5])
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

    private companion object {
        const val CHICKEN_SEARCH_HORIZONTAL = 5.0
        const val CHICKEN_SEARCH_VERTICAL = 3.0
        const val FULL_ROTATION = 360f
        const val WANDERING_TRADER_DESPAWN_DELAY = 48_000
        const val WANDERING_TRADER_HOME_RADIUS = 16
    }
}
