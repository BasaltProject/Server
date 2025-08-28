package org.basaltmc.server.runtime.world

import org.basaltmc.server.random.BasaltRandom
import org.basaltmc.server.registry.BlockShapes
import org.basaltmc.server.registry.BlockStateDefinitions
import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.registry.EntityTypeShapes
import org.basaltmc.server.registry.hasFluid
import org.basaltmc.server.registry.isLeavesState
import org.basaltmc.server.runtime.entity.Aabb
import org.basaltmc.server.runtime.entity.EntityCollision
import org.basaltmc.server.runtime.entity.ServerEntity
import org.basaltmc.server.runtime.entity.ai.VillagerSchedule
import org.basaltmc.wire.common.Identifier
import kotlin.math.floor

/** Villager social agreement and `SpawnUtil.LEGACY_IRON_GOLEM` placement from Minecraft 26.2. */
internal object VillageGolemSpawner {
    private const val RECENT_SLEEP_TICKS = 24_000L
    private const val GOLEM_MEMORY_TICKS = 599
    private const val GOLEM_SENSOR_INTERVAL = 200
    private const val GOLEM_SENSOR_RANGE_SQUARED = 16.0 * 16.0
    private const val PANIC_INTERVAL = 100L
    private const val GOSSIP_INTERVAL = 1_200L
    private const val VILLAGER_SEARCH = 10.0
    private const val PANIC_AGREEMENT = 3
    private const val GOSSIP_AGREEMENT = 5
    private const val SPAWN_ATTEMPTS = 10
    private const val SPAWN_RANGE_XZ = 8
    private const val SPAWN_RANGE_Y = 6
    private const val THREAT_RANGE_SQUARED = 8.0 * 8.0

    private val IRON_GOLEM = Identifier.vanilla("iron_golem")

    fun tick(
        world: LoadedWorld,
        villager: ServerEntity,
        living: List<ServerEntity>,
        random: BasaltRandom,
        spawn: (IntArray) -> Boolean,
    ) {
        if (villager.golemDetectedTicks > 0) villager.golemDetectedTicks--
        if ((villager.tickCount + villager.entityId) % GOLEM_SENSOR_INTERVAL == 0 && nearbyGolem(villager, living)) {
            villager.golemDetectedTicks = GOLEM_MEMORY_TICKS
        }
        if (!wantsGolem(villager, world.gameTime)) return

        val panicking = villager.panicTicks > 0 || nearbyThreat(villager, living)
        val gossipReady =
            villager.lastGossipTime == Long.MIN_VALUE ||
                world.gameTime >= villager.lastGossipTime + GOSSIP_INTERVAL
        val agreement =
            when {
                panicking && world.gameTime % PANIC_INTERVAL == 0L -> PANIC_AGREEMENT
                VillagerSchedule.activityAt(world.clock.totalTicks) == VillagerSchedule.Activity.MEET &&
                    gossipReady -> {
                    villager.lastGossipTime = world.gameTime
                    GOSSIP_AGREEMENT
                }
                else -> return
            }
        val nearby = nearbyVillagers(villager, living)
        val agreeing =
            nearby
                .asSequence()
                .filter { wantsGolem(it, world.gameTime) }
                .take(5)
                .count()
        if (agreeing < agreement) return
        val position = findSpawnPosition(world, villager, random) ?: return
        if (!spawn(position)) return
        nearby.forEach { it.golemDetectedTicks = GOLEM_MEMORY_TICKS }
    }

    internal fun wantsGolem(
        villager: ServerEntity,
        gameTime: Long,
    ): Boolean =
        villager.lastSleptGameTime != Long.MIN_VALUE &&
            gameTime - villager.lastSleptGameTime < RECENT_SLEEP_TICKS &&
            villager.golemDetectedTicks <= 0

    internal fun findSpawnPosition(
        world: LoadedWorld,
        villager: ServerEntity,
        random: BasaltRandom,
    ): IntArray? {
        val startX = floor(villager.x).toInt()
        val startY = floor(villager.y).toInt()
        val startZ = floor(villager.z).toInt()
        repeat(SPAWN_ATTEMPTS) attempt@{
            val x = startX + random.nextIntBetweenInclusive(-SPAWN_RANGE_XZ, SPAWN_RANGE_XZ)
            val z = startZ + random.nextIntBetweenInclusive(-SPAWN_RANGE_XZ, SPAWN_RANGE_XZ)
            if (world.border.distanceOutside(x + 0.5, z + 0.5) > 0.0) return@attempt
            var aboveY = startY + SPAWN_RANGE_Y
            var above = world.state.getBlockState(x, aboveY, z) ?: return@attempt
            repeat(SPAWN_RANGE_Y * 2 + 1) {
                val supportY = aboveY - 1
                val support = world.state.getBlockState(x, supportY, z) ?: return@attempt
                if (legacySupport(support, above)) {
                    val spawnY = supportY + 1
                    val box =
                        Aabb.of(
                            x + 0.5,
                            spawnY.toDouble(),
                            z + 0.5,
                            EntityTypeShapes.widthOf(IRON_GOLEM),
                            EntityTypeShapes.heightOf(IRON_GOLEM),
                        )
                    if (!EntityCollision.collides(world.state, box) && !containsFluid(world, box)) {
                        return intArrayOf(x, spawnY, z)
                    }
                    return@attempt
                }
                aboveY = supportY
                above = support
            }
        }
        return null
    }

    private fun legacySupport(
        supportState: Int,
        aboveState: Int,
    ): Boolean {
        val support = BlockStateDefinitions.blockOf(supportState)?.path ?: return false
        if (
            support in LEGACY_REJECTED_SUPPORTS ||
            support.endsWith("_stained_glass") ||
            support.endsWith("_stained_glass_pane") ||
            Blocks.isLeavesState(supportState)
        ) {
            return false
        }
        val clearAbove = Blocks.isAir(aboveState) || Blocks.hasFluid(aboveState)
        return clearAbove && (BlockShapes.isSolidState(supportState) || support == "powder_snow")
    }

    /** `Mob.checkSpawnObstruction` rejects any fluid touched by the completed golem AABB. */
    private fun containsFluid(
        world: LoadedWorld,
        box: Aabb,
    ): Boolean {
        val maxX = floor(box.maxX - 1.0e-7).toInt()
        val maxY = floor(box.maxY - 1.0e-7).toInt()
        val maxZ = floor(box.maxZ - 1.0e-7).toInt()
        for (x in floor(box.minX).toInt()..maxX) {
            for (y in floor(box.minY).toInt()..maxY) {
                for (z in floor(box.minZ).toInt()..maxZ) {
                    val state = world.state.getBlockState(x, y, z) ?: return true
                    if (Blocks.hasFluid(state)) return true
                }
            }
        }
        return false
    }

    private fun nearbyVillagers(
        source: ServerEntity,
        living: List<ServerEntity>,
    ): List<ServerEntity> {
        val sourceBox = Aabb.of(source.x, source.y, source.z, source.width, source.height)
        val search =
            Aabb(
                sourceBox.minX - VILLAGER_SEARCH,
                sourceBox.minY - VILLAGER_SEARCH,
                sourceBox.minZ - VILLAGER_SEARCH,
                sourceBox.maxX + VILLAGER_SEARCH,
                sourceBox.maxY + VILLAGER_SEARCH,
                sourceBox.maxZ + VILLAGER_SEARCH,
            )
        return living.filter { candidate ->
            !candidate.removed &&
                candidate.type.path == "villager" &&
                Aabb.of(candidate.x, candidate.y, candidate.z, candidate.width, candidate.height).intersects(search)
        }
    }

    private fun nearbyGolem(
        villager: ServerEntity,
        living: List<ServerEntity>,
    ): Boolean =
        living.any { candidate ->
            !candidate.removed &&
                candidate.type == IRON_GOLEM &&
                distanceSquared(villager, candidate) <= GOLEM_SENSOR_RANGE_SQUARED
        }

    private fun nearbyThreat(
        villager: ServerEntity,
        living: List<ServerEntity>,
    ): Boolean =
        living.any { candidate ->
            !candidate.removed &&
                candidate.type.path in VILLAGER_THREATS &&
                distanceSquared(villager, candidate) <= THREAT_RANGE_SQUARED
        }

    private fun distanceSquared(
        first: ServerEntity,
        second: ServerEntity,
    ): Double {
        val dx = first.x - second.x
        val dy = first.y - second.y
        val dz = first.z - second.z
        return dx * dx + dy * dy + dz * dz
    }

    private val LEGACY_REJECTED_SUPPORTS =
        setOf(
            "cobweb",
            "cactus",
            "glass_pane",
            "conduit",
            "ice",
            "tnt",
            "glowstone",
            "beacon",
            "sea_lantern",
            "frosted_ice",
            "tinted_glass",
            "glass",
        )

    private val VILLAGER_THREATS =
        setOf(
            "zombie",
            "husk",
            "drowned",
            "zombie_villager",
            "pillager",
            "vindicator",
            "evoker",
            "ravager",
            "vex",
            "illusioner",
            "zoglin",
        )
}
