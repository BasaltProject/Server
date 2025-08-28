package org.basaltmc.server.runtime

import org.basaltmc.server.plugins.PluginEventBus
import org.basaltmc.server.plugins.PluginEventTypes
import org.basaltmc.server.random.LegacyRandom
import org.basaltmc.server.runtime.command.CommandEnvironment
import org.basaltmc.server.runtime.config.WorldDimension
import org.basaltmc.server.runtime.config.WorldsConfig
import org.basaltmc.server.runtime.player.PlayerRegistry
import org.basaltmc.server.runtime.player.PlayerSession
import org.basaltmc.server.runtime.plugins.emit
import org.basaltmc.server.runtime.world.Beds
import org.basaltmc.server.runtime.world.DamageTypes
import org.basaltmc.server.runtime.world.EndPortals
import org.basaltmc.server.runtime.world.Fluids
import org.basaltmc.server.runtime.world.GameRuleRegistry
import org.basaltmc.server.runtime.world.Lightning
import org.basaltmc.server.runtime.world.LoadedWorld
import org.basaltmc.server.runtime.world.NetherPortals
import org.basaltmc.server.runtime.world.SkeletonHorseTrap
import org.basaltmc.server.runtime.world.Submersion
import org.basaltmc.server.runtime.world.Vibrations
import org.basaltmc.server.world.portal.PortalAxis
import org.basaltmc.server.world.portal.PortalShape
import org.basaltmc.wire.common.logging.Logger
import org.basaltmc.wire.protocol.play.Gamemode
import kotlin.math.floor
import kotlin.random.Random

/**
 * Everything the world itself owes the environment around a player: sleep, the border, drowning
 * and burning, the clock, weather, lightning and the two kinds of portal.
 */
public class EnvironmentTickSystem(
    private val playerRegistry: PlayerRegistry,
    private val pluginEvents: PluginEventBus,
    private val logger: Logger,
    private val worldsConfig: WorldsConfig,
    private val worlds: () -> Map<String, LoadedWorld>,
    private val chunkSimulation: ChunkSimulationView,
    private val broadcaster: BlockChangeBroadcaster,
    private val environment: CommandEnvironment,
    private val spawning: NaturalSpawningSystem,
) {
    /**
     * Skips the night when enough of the world's players have held a bed for five seconds.
     *
     * parity: `ServerLevel.tickTime`'s sleep path, `players_sleeping_percentage` and all: the clock
     * jumps to the next morning, the rain stops and everyone gets up. Nobody is woken by daylight
     * alone, because that is what getting up at dawn already does.
     */
    public fun tickSleep(world: LoadedWorld) {
        val players = playerRegistry.snapshot().filter { it.worldName == world.name }
        val awake = players.filter { it.gamemode != Gamemode.SPECTATOR }
        if (awake.isEmpty()) return
        val sleeping = awake.filter { it.sleepingBed != null }
        if (sleeping.isEmpty()) return
        val percentage = world.gameRules[GameRuleRegistry.PLAYERS_SLEEPING_PERCENTAGE]
        val needed = maxOf(1, (awake.size * percentage + 99) / 100)
        if (sleeping.size < needed) return
        if (sleeping.any { it.sleepTicks < Beds.SLEEP_TICKS_BEFORE_MORNING }) return

        world.clock.setTotalTicks(Beds.nextMorning(world.clock.totalTicks))
        world.weather.raining = false
        world.weather.thundering = false
        for (player in players) {
            player.connection.wakeUp()
            player.connection.sendTime(world)
        }
    }

    /**
     * Hurts whoever has walked outside the border. parity: `ServerPlayer.tick`, which checks every
     * ten ticks rather than every one, so the damage arrives in visible steps.
     */
    public fun tickBorderDamage(world: LoadedWorld) {
        if (world.gameTime % BORDER_DAMAGE_INTERVAL_TICKS != 0L) return
        val border = world.border
        if (border.damagePerBlock <= 0.0) return
        for (player in playerRegistry.snapshot()) {
            if (player.worldName != world.name) continue
            if (player.gamemode == Gamemode.CREATIVE || player.gamemode == Gamemode.SPECTATOR) continue
            val damage = border.damageAt(player.x, player.z)
            if (damage <= 0f) continue
            player.connection.hurt(damage, "outOfWorldBorder", DamageTypes.OUTSIDE_BORDER)
        }
    }

    /**
     * Moves whoever has stood in a portal long enough, and counts down the wait for everyone else.
     *
     * parity: `Entity.handlePortal`. The delay is the two game rules (nothing in creative, four
     * seconds otherwise), and the 300 tick cooldown afterwards is what stops the portal on the
     * other side from sending them straight back.
     */
    public fun tickPortals(world: LoadedWorld) {
        val creativeDelay = world.gameRules[GameRuleRegistry.PLAYERS_NETHER_PORTAL_CREATIVE_DELAY]
        val defaultDelay = world.gameRules[GameRuleRegistry.PLAYERS_NETHER_PORTAL_DEFAULT_DELAY]
        for (player in playerRegistry.snapshot()) {
            if (player.worldName != world.name) continue
            if (player.portalCooldown > 0) {
                player.portalCooldown--
                player.portalTicks = 0
                continue
            }
            val state =
                world.state.getBlockState(
                    floor(player.x).toInt(),
                    floor(player.y).toInt(),
                    floor(player.z).toInt(),
                )
            if (state != null && EndPortals.isEndPortal(state)) {
                // parity: `Portal.getPortalTransitionTime` is zero for an end portal, so it takes
                // whoever stepped in on the tick they stepped in.
                travelThroughEndPortal(world, player)
                continue
            }
            val axis = state?.let(PortalShape::axisOf)
            if (axis == null) {
                player.portalTicks = 0
                continue
            }
            player.portalTicks++
            val delay = if (player.gamemode == Gamemode.CREATIVE) creativeDelay else defaultDelay
            if (player.portalTicks < delay) continue
            travelThroughPortal(world, player, axis)
        }
    }

    /**
     * Drowning, burning, and the two numbers the HUD draws them from.
     *
     * parity: `LivingEntity.baseTick` and `Entity.lavaHurt`. The client renders the water overlay
     * from its own copy of the blocks, but the bubbles and the flames come from here: it draws them
     * from the air supply and the shared flags the server sends, and neither moves on its own.
     */
    public fun tickSubmersion(world: LoadedWorld) {
        for (player in playerRegistry.snapshot()) {
            if (player.worldName != world.name) continue
            if (player.dead) continue
            val eyeY = player.y + Fluids.eyeHeight(player.sneaking)
            val feet =
                world.state.getBlockState(
                    floor(player.x).toInt(),
                    floor(player.y).toInt(),
                    floor(player.z).toInt(),
                )
            val eyes = world.state.getBlockState(floor(player.x).toInt(), floor(eyeY).toInt(), floor(player.z).toInt())
            val air = player.airSupply
            val fire = player.fireTicks

            val drowns = !player.invulnerable && eyes != null && Fluids.of(eyes) == Submersion.WATER
            if (drowns) {
                player.airSupply = Fluids.airAfterTick(player.airSupply, eyesInWater = true)
                if (player.airSupply <= Fluids.DROWNING_AIR) {
                    player.airSupply = 0
                    player.connection.hurt(Fluids.DROWN_DAMAGE, "drown", DamageTypes.DROWN)
                }
            } else {
                player.airSupply = Fluids.airAfterTick(player.airSupply, eyesInWater = false)
            }

            val inLava =
                !player.invulnerable &&
                    (
                        (feet != null && Fluids.of(feet) == Submersion.LAVA) ||
                            (eyes != null && Fluids.of(eyes) == Submersion.LAVA)
                    )
            if (inLava) {
                player.fireTicks = Fluids.LAVA_FIRE_TICKS
                if (world.gameTime % LAVA_DAMAGE_INTERVAL_TICKS == 0L) {
                    player.connection.hurt(Fluids.LAVA_DAMAGE, "lava", DamageTypes.LAVA)
                }
            } else if (player.fireTicks > 0) {
                player.fireTicks--
                // parity: `Entity.baseTick` burns for one heart every second, and lava's own damage
                // replaces it while standing in it.
                if (player.fireTicks % FIRE_DAMAGE_INTERVAL_TICKS == 0) {
                    player.connection.hurt(Fluids.FIRE_DAMAGE, "onFire", DamageTypes.ON_FIRE)
                }
            }

            if (player.airSupply != air || (player.fireTicks > 0) != (fire > 0)) {
                player.connection.syncSubmersion()
            }
        }
    }

    /** The other side of a Nether portal: this world's own Nether, or the overworld it belongs to. */
    private fun netherCounterpart(world: LoadedWorld): LoadedWorld? {
        val wanted =
            if (world.declaration.dimension == WorldDimension.NETHER) {
                WorldDimension.OVERWORLD
            } else {
                WorldDimension.NETHER
            }
        return counterpart(world, wanted)
    }

    /**
     * The world a portal out of [world] leads to, or null with a line in the log saying why.
     *
     * The pairing comes from `worlds.toml`: a Nether or an End names the Overworld it belongs to,
     * so a server can have several sets of dimensions and a portal still knows which one it is in.
     * A dimension nobody declared leaves the portal leading nowhere, and saying nothing about it is
     * the one answer that leaves the operator guessing. Warned once per world and dimension, not
     * once per tick: a player standing in a portal asks this twenty times a second.
     */
    private fun counterpart(
        world: LoadedWorld,
        dimension: WorldDimension,
    ): LoadedWorld? {
        val declaration = worldsConfig.portalTarget(world.declaration, dimension)
        val target = declaration?.let { worlds()[it.name] }
        if (target == null && warnedMissingDimensions.add(world.name to dimension)) {
            val overworld = worldsConfig.overworldOf(world.declaration).name
            logger.warn {
                "A portal in '${world.name}' leads to the ${dimension.configName} of '$overworld', " +
                    "and worlds.toml declares none, so it leads nowhere. Declare one with " +
                    "linked_to = \"$overworld\" to travel through it."
            }
        }
        return target
    }

    /** Which missing dimensions have already been reported, per world. See [counterpart]. */
    private val warnedMissingDimensions = mutableSetOf<Pair<String, WorldDimension>>()

    private fun travelThroughPortal(
        world: LoadedWorld,
        player: PlayerSession,
        axis: PortalAxis,
    ) {
        val target = netherCounterpart(world) ?: return
        // parity: `Level.isAllowedToEnterPortal`, the game rule that keeps a server out of the Nether.
        if (target.declaration.dimension == WorldDimension.NETHER &&
            !world.gameRules[GameRuleRegistry.ALLOW_ENTERING_NETHER_USING_PORTALS]
        ) {
            return
        }
        val exit = NetherPortals.exitFor(world, target, player.x, player.y, player.z, axis) ?: return
        val travelling =
            pluginEvents.emit(PluginEventTypes.PLAYER_PORTAL) { event ->
                event.set(0, player.name)
                event.set(1, player.uuid.toString())
                event.set(2, world.name)
                event.set(3, target.name)
                event.set(4, exit.x)
                event.set(5, exit.y)
                event.set(6, exit.z)
                event.set(7, "nether")
            }
        if (travelling?.cancelled == true) return
        player.portalTicks = 0
        player.portalCooldown = NetherPortals.TRAVEL_COOLDOWN_TICKS
        player.connection.switchWorld(target, exit.x, exit.y, exit.z)
    }

    /** The other side of an end portal: the End, or the world its own spawn is in. */
    private fun endCounterpart(world: LoadedWorld): LoadedWorld? {
        val wanted =
            if (world.declaration.dimension == WorldDimension.END) WorldDimension.OVERWORLD else WorldDimension.END
        return counterpart(world, wanted)
    }

    private fun travelThroughEndPortal(
        world: LoadedWorld,
        player: PlayerSession,
    ) {
        val target = endCounterpart(world) ?: return
        val exit = EndPortals.exitFor(world, target)
        player.portalTicks = 0
        player.portalCooldown = NetherPortals.TRAVEL_COOLDOWN_TICKS
        player.connection.switchWorld(target, exit.x, exit.y, exit.z)
    }

    /**
     * Advances one world's clock and weather, and tells its players about it once a second.
     *
     * parity: vanilla resyncs the clock every 20 ticks rather than every tick, because the client
     * runs its own between updates; sending 20 packets a second per player would be pure noise.
     */
    public fun tickTime(world: LoadedWorld) {
        val wasRaining = world.weather.raining
        val wasThundering = world.weather.thundering
        world.tickTime()
        if (world.weather.raining != wasRaining || world.weather.thundering != wasThundering) {
            val change =
                pluginEvents.emit(PluginEventTypes.WORLD_WEATHER_CHANGE) { event ->
                    event.set(0, world.name)
                    event.set(1, world.weather.raining)
                    event.set(2, world.weather.thundering)
                    event.set(3, wasRaining)
                    event.set(4, wasThundering)
                }
            if (change?.cancelled == true) {
                world.weather.raining = wasRaining
                world.weather.thundering = wasThundering
            }
            environment.syncWeather(world)
        }
        if (world.gameTime % TIME_SYNC_INTERVAL_TICKS == 0L) environment.syncTime(world)
        if (world.weather.thundering) strikeLightning(world)
    }

    /**
     * The storm's own share of a tick: one roll per simulated chunk, and a strike where it lands.
     *
     * parity: `ServerLevel.tickChunk`, whose thunder is a one in a hundred thousand per chunk. A
     * lightning rod within 128 blocks takes the strike, which is the whole point of putting one up.
     */
    private fun strikeLightning(world: LoadedWorld) {
        if (!world.weather.raining) return
        val chunks = chunkSimulation.simulatedChunks(world)
        if (chunks.isEmpty()) return
        val random = LegacyRandom(Random.nextLong())
        val watchers = playerRegistry.snapshot().filter { it.worldName == world.name }
        for (chunk in chunks) {
            if (!Lightning.shouldStrike(random)) continue
            val x = (chunk[0] shl 4) + random.nextInt(16)
            val z = (chunk[1] shl 4) + random.nextInt(16)
            val target = Lightning.targetFor(world, x, z) ?: continue
            val struck =
                pluginEvents.emit(PluginEventTypes.WORLD_LIGHTNING) { event ->
                    event.set(0, world.name)
                    event.set(1, target[0])
                    event.set(2, target[1])
                    event.set(3, target[2])
                }
            if (struck?.cancelled == true) continue
            val isTrap = SkeletonHorseTrap.shouldCreateFromThunder(world, target[0], target[1], target[2], random)
            if (isTrap) spawning.spawnSkeletonTrap(world, target[0], target[1], target[2])
            val changes = mutableListOf<org.basaltmc.server.runtime.world.BlockChange>()
            // parity: `LightningBolt.tick`, where fire is only lit on normal and hard.
            val lightFires = !isTrap && (world.difficulty == "normal" || world.difficulty == "hard")
            Lightning.strike(world, target[0], target[1], target[2], lightFires, changes)
            // parity: `GameEvent.LIGHTNING_STRIKE`, which a sculk sensor hears as fourteen.
            Vibrations.emit(
                world,
                "lightning_strike",
                target[0].toDouble(),
                target[1].toDouble(),
                target[2].toDouble(),
                changes,
            )
            for (change in changes) broadcaster.announceRedstone(world, watchers, change)
        }
    }

    private companion object {
        /** parity: `Entity.lavaHurt`, which hurts every ten ticks, and fire, which does every twenty. */
        const val LAVA_DAMAGE_INTERVAL_TICKS = 10L

        const val FIRE_DAMAGE_INTERVAL_TICKS = 20

        // parity: vanilla's own clock resync interval, once a second.
        const val TIME_SYNC_INTERVAL_TICKS = 20L

        // parity: `ServerPlayer.tick` checks the border every ten ticks, not every one.
        const val BORDER_DAMAGE_INTERVAL_TICKS = 10L
    }
}
