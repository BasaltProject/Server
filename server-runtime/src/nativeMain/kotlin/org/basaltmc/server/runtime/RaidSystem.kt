package org.basaltmc.server.runtime

import org.basaltmc.server.plugins.PluginEventBus
import org.basaltmc.server.plugins.PluginEventTypes
import org.basaltmc.server.registry.BlockShapes
import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.runtime.entity.Raid
import org.basaltmc.server.runtime.entity.RaidManager
import org.basaltmc.server.runtime.entity.RaidStatus
import org.basaltmc.server.runtime.entity.RaidWaves
import org.basaltmc.server.runtime.entity.ai.VillageBlocks
import org.basaltmc.server.runtime.player.PlayerRegistry
import org.basaltmc.server.runtime.player.PlayerSession
import org.basaltmc.server.runtime.plugins.emit
import org.basaltmc.server.runtime.world.ActiveEffect
import org.basaltmc.server.runtime.world.BlockEffect
import org.basaltmc.server.runtime.world.LoadedWorld
import org.basaltmc.server.runtime.world.MobEffects
import org.basaltmc.wire.common.Identifier
import org.basaltmc.wire.common.logging.Logger
import org.basaltmc.wire.protocol.play.BossBarColor
import org.basaltmc.wire.protocol.play.BossBarOverlay
import org.basaltmc.wire.protocol.play.BossEventPacket
import org.basaltmc.wire.protocol.play.Gamemode
import org.basaltmc.wire.protocol.play.SoundSource
import org.basaltmc.wire.text.Component
import kotlin.math.PI
import kotlin.math.cos
import kotlin.math.floor
import kotlin.math.sin
import kotlin.random.Random

/**
 * Raids: their waves, the boss bar each one gets, and the bad omen that starts one.
 *
 * parity: `Raids` (one per world, kept in memory rather than saved data here) and `Raid` itself.
 */
public class RaidSystem(
    private val playerRegistry: PlayerRegistry,
    private val pluginEvents: PluginEventBus,
    private val logger: Logger,
    private val broadcaster: BlockChangeBroadcaster,
    private val spawning: NaturalSpawningSystem,
) {
    /** parity: `Raids`, one per world, which this build keeps in memory rather than saved data. */
    public val raidManagers: MutableMap<String, RaidManager> = mutableMapOf()

    /** One raid manager per world, made the first time anything asks for one. */
    private fun raidsIn(world: LoadedWorld): RaidManager = raidManagers.getOrPut(world.name) { RaidManager() }

    /**
     * Every raid in one world.
     *
     * parity: `Raids.tick` over `Raid.tick`: a wave every time the last one is dead, fifteen seconds
     * apart, and the raid ends when its waves do rather than when the village stops being one.
     */
    public fun tickRaids(world: LoadedWorld) {
        val manager = raidManagers[world.name] ?: return
        if (manager.all.isEmpty()) return
        val players = playerRegistry.snapshot().filter { it.worldName == world.name }
        val finished = mutableListOf<Raid>()
        for (raid in manager.all.toList()) {
            tickRaid(world, players, raid)
            if (raid.status == RaidStatus.ONGOING || raid.celebrationTicks > 0) continue
            hideRaidBar(players, raid)
            pluginEvents.emit(PluginEventTypes.RAID_FINISH) { event ->
                event.set(0, world.name)
                event.set(1, raid.centerX)
                event.set(2, raid.centerY)
                event.set(3, raid.centerZ)
                event.set(4, raid.groupsSpawned)
                event.set(5, raid.status == RaidStatus.VICTORY)
            }
            finished.add(raid)
        }
        manager.forget(finished)
    }

    private fun tickRaid(
        world: LoadedWorld,
        players: List<PlayerSession>,
        raid: Raid,
    ) {
        if (raid.status != RaidStatus.ONGOING) {
            raid.celebrationTicks--
            return
        }
        raid.ticksActive++
        if (raid.ticksActive >= Raid.TIMEOUT_TICKS) {
            raid.status = RaidStatus.STOPPED
            raid.celebrationTicks = 0
            return
        }
        var health = 0.0
        raid.raiders.retainAll { id ->
            val raider = world.state.entity(id)
            if (raider == null || raider.removed || raider.health <= 0.0) return@retainAll false
            health += raider.health
            true
        }
        // parity: `updatePlayers`, which is who the bar belongs to and who ends up a hero.
        for (player in players) {
            if (raid.distanceSquaredTo(player.x, player.y, player.z) >= Raid.VALID_RADIUS_SQUARED) continue
            raid.heroes.add(player.entityId)
        }
        if (raid.raiders.isEmpty()) {
            if (!raid.hasMoreWaves) {
                winRaid(players, raid)
                return
            }
            if (raid.cooldown > 0) {
                raid.cooldown--
            } else {
                spawnRaidWave(world, players, raid)
                raid.cooldown = Raid.PRE_RAID_TICKS
            }
        }
        updateRaidBar(players, raid, health)
    }

    /** parity: `spawnGroup`, whose counts are `RaiderType`'s own arrays. */
    private fun spawnRaidWave(
        world: LoadedWorld,
        players: List<PlayerSession>,
        raid: Raid,
    ) {
        val wave = raid.groupsSpawned + 1
        val position = raidSpawnPosition(world, raid)
        if (position == null) {
            logger.debug { "Raid wave $wave found nowhere to spawn around ${raid.centerX}, ${raid.centerZ}" }
            return
        }
        var total = 0.0
        for (type in RaidWaves.types) {
            repeat(RaidWaves.countFor(type, wave)) {
                val raider =
                    spawning.spawnMob(
                        world,
                        Identifier.vanilla(type),
                        position[0] + 0.5,
                        position[1].toDouble(),
                        position[2] + 0.5,
                    ) ?: return@repeat
                raider.persistent = true
                raider.raidCenter = intArrayOf(raid.centerX, raid.centerY, raid.centerZ)
                raid.raiders.add(raider.entityId)
                total += raider.health
            }
        }
        raid.groupsSpawned = wave
        raid.totalHealth = total
        // parity: `playSound`, the horn everyone within sixty-four blocks hears.
        broadcaster.sendEffects(
            players.filter { raid.distanceSquaredTo(it.x, it.y, it.z) < RAID_HORN_RANGE_SQUARED },
            position[0],
            position[1],
            position[2],
            Blocks.AIR.stateId,
            listOf(BlockEffect.Sound(SOUND_RAID_HORN, SoundSource.NEUTRAL, RAID_HORN_VOLUME, 1f)),
        )
    }

    /** parity: `findRandomSpawnPos`, a ring around the village with ground under it. */
    private fun raidSpawnPosition(
        world: LoadedWorld,
        raid: Raid,
    ): IntArray? {
        repeat(RAID_SPAWN_ATTEMPTS) {
            val angle = Random.nextDouble() * 2.0 * PI
            val distance = RAID_SPAWN_MIN + Random.nextDouble() * (RAID_SPAWN_MAX - RAID_SPAWN_MIN)
            val x = raid.centerX + (cos(angle) * distance).toInt()
            val z = raid.centerZ + (sin(angle) * distance).toInt()
            val ground = groundBelow(world, x, raid.centerY + RAID_SPAWN_LIFT, z) ?: return@repeat
            return intArrayOf(x, ground, z)
        }
        return null
    }

    /** The first solid block under a column, which is where anything spawned outdoors stands. */
    private fun groundBelow(
        world: LoadedWorld,
        x: Int,
        fromY: Int,
        z: Int,
    ): Int? {
        var y = fromY
        var dropped = 0
        while (dropped < RAID_SPAWN_DROP) {
            val below = world.state.getBlockState(x, y - 1, z) ?: return null
            val here = world.state.getBlockState(x, y, z) ?: return null
            if (!BlockShapes.isEmptyShape(below) && BlockShapes.isEmptyShape(here)) return y
            y--
            dropped++
        }
        return null
    }

    /** parity: the victory, which is the hero effect for everybody who was there for it. */
    private fun winRaid(
        players: List<PlayerSession>,
        raid: Raid,
    ) {
        raid.status = RaidStatus.VICTORY
        raid.celebrationTicks = Raid.CELEBRATION_TICKS
        for (player in players) {
            if (player.entityId !in raid.heroes) continue
            player.connection.addEffect(MobEffects.HERO_OF_THE_VILLAGE, Raid.HERO_DURATION_TICKS, raid.omenLevel - 1)
        }
    }

    private fun updateRaidBar(
        players: List<PlayerSession>,
        raid: Raid,
        health: Double,
    ) {
        if (raid.groupsSpawned == 0) return
        val progress =
            if (raid.totalHealth <= 0.0) 0f else (health / raid.totalHealth).toFloat().coerceIn(0f, 1f)
        for (player in players) {
            val inRaid = raid.distanceSquaredTo(player.x, player.y, player.z) < Raid.VALID_RADIUS_SQUARED
            if (!inRaid) {
                if (raid.watchers.remove(player.entityId)) sendRaidBar(player, raid, BossEventPacket.Operation.Remove)
                continue
            }
            if (raid.watchers.add(player.entityId)) {
                sendRaidBar(
                    player,
                    raid,
                    BossEventPacket.Operation.Add(
                        Component.translatable(RAID_BAR_KEY),
                        progress,
                        BossBarColor.RED,
                        BossBarOverlay.NOTCHED_10,
                    ),
                )
            } else {
                sendRaidBar(player, raid, BossEventPacket.Operation.UpdateProgress(progress))
            }
        }
    }

    private fun hideRaidBar(
        players: List<PlayerSession>,
        raid: Raid,
    ) {
        for (player in players) {
            if (!raid.watchers.remove(player.entityId)) continue
            sendRaidBar(player, raid, BossEventPacket.Operation.Remove)
        }
    }

    private fun sendRaidBar(
        player: PlayerSession,
        raid: Raid,
        operation: BossEventPacket.Operation,
    ) {
        player.connection.sendPlayPacket(BossEventPacket.ID) { BossEventPacket(raid.barId, operation).write(it) }
    }

    /**
     * parity: `BadOmenMobEffect`, which swaps itself for a raid omen the moment its carrier walks
     * into a village, and `RaidOmenMobEffect`, which starts the raid where that happened.
     */
    public fun tickOmens(
        world: LoadedWorld,
        player: PlayerSession,
        active: ActiveEffect,
    ) {
        if (player.gamemode == Gamemode.SPECTATOR) return
        if (active.effect == MobEffects.BAD_OMEN) {
            if (world.difficulty == "peaceful") return
            // The effect's own countdown is the clock here: it runs whether or not the day does.
            if (active.remainingTicks % VILLAGE_CHECK_INTERVAL != 0) return
            if (!isVillage(world, player.x, player.y, player.z)) return
            val raid = raidsIn(world).nearby(player.x, player.y, player.z)
            if (raid != null && raid.omenLevel >= Raid.MAX_OMEN_LEVEL) return
            player.effects.remove(MobEffects.BAD_OMEN)
            player.connection.sendEffectRemoved(MobEffects.BAD_OMEN)
            player.raidOmenPosition =
                intArrayOf(
                    floor(player.x).toInt(),
                    floor(player.y).toInt(),
                    floor(player.z).toInt(),
                )
            player.connection.addEffect(MobEffects.RAID_OMEN, RAID_OMEN_TICKS, active.amplifier)
            return
        }
        if (active.effect != MobEffects.RAID_OMEN || active.remainingTicks > 1) return
        val position = player.raidOmenPosition ?: return
        player.raidOmenPosition = null
        val raid =
            raidsIn(world).createOrExtend(position[0], position[1], position[2], active.amplifier, world.difficulty)
        if (raid != null) {
            logger.debug {
                "Raid at ${raid.centerX}, ${raid.centerY}, ${raid.centerZ} in world '${world.name}': " +
                    "${raid.totalGroups} wave(s), omen level ${raid.omenLevel}"
            }
        }
    }

    /**
     * Whether this is a village at all.
     *
     * parity: `ServerLevel.isVillage`, which counts the village points of interest around a spot.
     * There is no point of interest map here, so the beds are counted directly: a place people
     * sleep in is what a raid is aimed at either way.
     */
    private fun isVillage(
        world: LoadedWorld,
        x: Double,
        y: Double,
        z: Double,
    ): Boolean {
        val originX = floor(x).toInt()
        val originY = floor(y).toInt()
        val originZ = floor(z).toInt()
        var beds = 0
        for (dy in -VILLAGE_HEIGHT..VILLAGE_HEIGHT) {
            for (dx in -VILLAGE_RADIUS..VILLAGE_RADIUS) {
                for (dz in -VILLAGE_RADIUS..VILLAGE_RADIUS) {
                    val state = world.state.getBlockState(originX + dx, originY + dy, originZ + dz) ?: continue
                    val block = Blocks.byStateId(state)?.block?.path ?: continue
                    if (block !in VillageBlocks.BEDS) continue
                    beds++
                    if (beds >= VILLAGE_BEDS) return true
                }
            }
        }
        return false
    }

    private companion object {
        const val SOUND_RAID_HORN = "event.raid.horn"

        /** parity: the horn is heard sixty-four blocks away, at sixty-four blocks of volume. */
        const val RAID_HORN_RANGE_SQUARED = 4096.0
        const val RAID_HORN_VOLUME = 64f
        const val RAID_BAR_KEY = "event.minecraft.raid"

        /** parity: `NUM_SPAWN_ATTEMPTS` and the ring `findRandomSpawnPos` looks along. */
        const val RAID_SPAWN_ATTEMPTS = 20
        const val RAID_SPAWN_MIN = 24.0
        const val RAID_SPAWN_MAX = 32.0
        const val RAID_SPAWN_LIFT = 12
        const val RAID_SPAWN_DROP = 24

        /** parity: `BadOmenMobEffect`, whose raid omen lasts thirty seconds. */
        const val RAID_OMEN_TICKS = 600

        /** How often a bad omen looks for a village, and what counts as one. */
        const val VILLAGE_CHECK_INTERVAL = 20
        const val VILLAGE_RADIUS = 16
        const val VILLAGE_HEIGHT = 4
        const val VILLAGE_BEDS = 3
    }
}
