package org.basaltmc.server.runtime

import org.basaltmc.server.plugins.PluginEventBus
import org.basaltmc.server.plugins.PluginEventTypes
import org.basaltmc.server.random.LegacyRandom
import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.registry.EntityTypeShapes
import org.basaltmc.server.registry.EntityTypes
import org.basaltmc.server.registry.Items
import org.basaltmc.server.runtime.entity.Attribute
import org.basaltmc.server.runtime.entity.Breeding
import org.basaltmc.server.runtime.entity.DamageSource
import org.basaltmc.server.runtime.entity.EntityCombat
import org.basaltmc.server.runtime.entity.Flyers
import org.basaltmc.server.runtime.entity.MobCombat
import org.basaltmc.server.runtime.entity.Projectiles
import org.basaltmc.server.runtime.entity.SunBurn
import org.basaltmc.server.runtime.entity.ai.LookControl
import org.basaltmc.server.runtime.entity.ai.MobActions
import org.basaltmc.server.runtime.entity.ai.MobContext
import org.basaltmc.server.runtime.entity.ai.MobGoals
import org.basaltmc.server.runtime.entity.ai.MoveControl
import org.basaltmc.server.runtime.entity.ai.Navigation
import org.basaltmc.server.runtime.entity.ai.PlayerView
import org.basaltmc.server.runtime.entity.displayName
import org.basaltmc.server.runtime.player.Combat
import org.basaltmc.server.runtime.player.PlayerRegistry
import org.basaltmc.server.runtime.player.PlayerSession
import org.basaltmc.server.runtime.plugins.emit
import org.basaltmc.server.runtime.world.Beds
import org.basaltmc.server.runtime.world.BlockEffect
import org.basaltmc.server.runtime.world.DamageTypes
import org.basaltmc.server.runtime.world.Fluids
import org.basaltmc.server.runtime.world.LoadedWorld
import org.basaltmc.server.runtime.world.RealLootTables
import org.basaltmc.server.runtime.world.Submersion
import org.basaltmc.server.runtime.world.Vibrations
import org.basaltmc.server.runtime.world.WorldLight
import org.basaltmc.server.runtime.world.WorldState
import org.basaltmc.wire.common.Identifier
import org.basaltmc.wire.protocol.play.AnimatePacket
import org.basaltmc.wire.protocol.play.EntityTeleportPacket
import org.basaltmc.wire.protocol.play.LevelParticlesPacket
import org.basaltmc.wire.protocol.play.Pose
import org.basaltmc.wire.protocol.play.SetEntityDataPacket
import org.basaltmc.wire.protocol.play.SoundSource
import kotlin.math.abs
import kotlin.math.atan2
import kotlin.math.cos
import kotlin.math.floor
import kotlin.math.sin
import kotlin.math.sqrt
import kotlin.random.Random
import kotlin.uuid.Uuid

/**
 * What a goal is allowed to make the world do (swing, hit, breed, eat, ...), plus the rest of a
 * mob's own turn: fire and sunlight, the AI step itself, breeding and the loot it drops on death.
 *
 * The goals cannot reach the server themselves, so implementing [MobActions] here is the whole of
 * their arm's length.
 */
public class MobActionSystem(
    private val playerRegistry: PlayerRegistry,
    private val pluginEvents: PluginEventBus,
    private val broadcaster: BlockChangeBroadcaster,
    private val explosions: ExplosionSystem,
    private val spawning: NaturalSpawningSystem,
    private val combat: CombatSystem,
    private val raids: RaidSystem,
    private val lootTables: RealLootTables,
    private val worlds: () -> Map<String, LoadedWorld>,
) : MobActions {
    internal fun activateSkeletonTrap(
        world: LoadedWorld,
        horse: org.basaltmc.server.runtime.entity.ServerEntity,
        random: LegacyRandom,
    ) {
        if (!spawning.activateSkeletonTrap(world, horse, random)) return
        val changes = mutableListOf<org.basaltmc.server.runtime.world.BlockChange>()
        Vibrations.emit(world, "lightning_strike", horse.x, horse.y, horse.z, changes)
        val watchers = playerRegistry.snapshot().filter { it.worldName == world.name }
        for (change in changes) broadcaster.announceRedstone(world, watchers, change)
    }

    internal fun tickVillageGolem(
        world: LoadedWorld,
        villager: org.basaltmc.server.runtime.entity.ServerEntity,
        living: List<org.basaltmc.server.runtime.entity.ServerEntity>,
        random: LegacyRandom,
    ) {
        spawning.tickVillageGolem(world, villager, living, random)
    }

    override fun swing(mob: org.basaltmc.server.runtime.entity.ServerEntity) {
        val world = worlds().values.firstOrNull { it.state.entity(mob.entityId) != null } ?: return
        for (player in playerRegistry.snapshot().filter { it.worldName == world.name }) {
            player.connection.sendPlayPacket(AnimatePacket.ID) {
                AnimatePacket(mob.entityId, AnimatePacket.SWING_MAIN_HAND).write(it)
            }
        }
    }

    override fun hurtPlayer(
        mob: org.basaltmc.server.runtime.entity.ServerEntity,
        playerId: Int,
        damage: Float,
    ) {
        val player = playerRegistry.snapshot().firstOrNull { it.entityId == playerId } ?: return
        val world = worlds()[player.worldName] ?: return
        // parity: `Player.hurtServer`, where the difficulty decides how much of it lands.
        player.connection.hurt(
            Combat.afterDifficulty(damage, world.difficulty),
            DEATH_MOB,
            DamageTypes.MOB_ATTACK,
            DamageSource(mob.entityId, mob.x, mob.z, mob.displayName()),
        )
        // parity: `OwnerHurtByTargetGoal`: a pet goes for whatever hit the person it follows.
        for (pet in world.state.entities()) {
            if (pet.owner == player.uuid.toString()) pet.ownerHurtBy = mob.entityId
        }
    }

    override fun hurtMob(
        mob: org.basaltmc.server.runtime.entity.ServerEntity,
        target: org.basaltmc.server.runtime.entity.ServerEntity,
        damage: Float,
    ) {
        val world = worlds().values.firstOrNull { it.state.entity(target.entityId) != null } ?: return
        val result =
            EntityCombat.hurt(
                target,
                damage,
                mob.x,
                mob.z,
                events = pluginEvents,
                worldName = world.name,
                cause = "mob_attack",
                attackerName = mob.type.toString(),
            )
        if (!result.landed) return
        target.hurtBy = mob.entityId
        target.panicTicks = PANIC_TICKS
        val watchers = playerRegistry.snapshot().filter { it.worldName == world.name }
        combat.announceEntityHit(watchers, target, DamageTypes.MOB_ATTACK, mob.entityId, result.died)
    }

    override fun breed(
        mob: org.basaltmc.server.runtime.entity.ServerEntity,
        partner: org.basaltmc.server.runtime.entity.ServerEntity,
    ) {
        val world = worlds().values.firstOrNull { it.state.entity(mob.entityId) != null } ?: return
        breed(world, mob, partner)
    }

    override fun eatGrass(mob: org.basaltmc.server.runtime.entity.ServerEntity) {
        val world = worlds().values.firstOrNull { it.state.entity(mob.entityId) != null } ?: return
        eatGrassUnder(world, mob)
    }

    override fun setCharging(
        mob: org.basaltmc.server.runtime.entity.ServerEntity,
        charging: Boolean,
    ) {
        if (mob.charging == charging) return
        mob.charging = charging
        val world = worlds().values.firstOrNull { it.state.entity(mob.entityId) != null } ?: return
        val index = if (mob.type.path == "blaze") BLAZE_CHARGED_INDEX else GHAST_CHARGING_INDEX
        val entry =
            if (mob.type.path == "blaze") {
                SetEntityDataPacket.Entry.Bytes(index, (if (charging) BLAZE_CHARGED_FLAG else 0).toByte())
            } else {
                SetEntityDataPacket.Entry.Booleans(index, charging)
            }
        for (player in playerRegistry.snapshot().filter { it.worldName == world.name }) {
            player.connection.sendPlayPacket(SetEntityDataPacket.ID) {
                SetEntityDataPacket(mob.entityId, listOf(entry)).write(it)
            }
        }
    }

    override fun shootFireball(
        mob: org.basaltmc.server.runtime.entity.ServerEntity,
        targetX: Double,
        targetY: Double,
        targetZ: Double,
        large: Boolean,
    ) {
        val world = worlds().values.firstOrNull { it.state.entity(mob.entityId) != null } ?: return
        val kind = if (large) Projectiles.FIREBALL else Projectiles.SMALL_FIREBALL
        val protocolType = EntityTypes[kind.type]?.protocolId ?: return
        val fromY = mob.y + mob.height * FIREBALL_EYE_SHARE
        val ball =
            Projectiles.spawn(
                world = world.state,
                kind = kind,
                entityId = playerRegistry.allocateEntityId(),
                protocolType = protocolType,
                x = mob.x,
                y = fromY,
                z = mob.z,
                dirX = targetX - mob.x,
                dirY = targetY - fromY,
                dirZ = targetZ - mob.z,
                power = FIREBALL_POWER,
                uncertainty = if (large) 0f else SMALL_FIREBALL_SPREAD,
                random = LegacyRandom(Random.nextLong()),
            )
        ball.ownerId = mob.entityId
        ball.explosionPower = if (large) GHAST_EXPLOSION_POWER else 0f
        broadcaster.sendEffects(
            playerRegistry.snapshot().filter { it.worldName == world.name },
            floor(mob.x).toInt(),
            floor(mob.y).toInt(),
            floor(mob.z).toInt(),
            Blocks.AIR.stateId,
            listOf(BlockEffect.Sound(SOUND_FIREBALL_SHOOT, SoundSource.HOSTILE, 1f, 1f)),
        )
    }

    override fun setBeamTarget(
        mob: org.basaltmc.server.runtime.entity.ServerEntity,
        targetId: Int?,
    ) {
        val world = worlds().values.firstOrNull { it.state.entity(mob.entityId) != null } ?: return
        for (player in playerRegistry.snapshot().filter { it.worldName == world.name }) {
            player.connection.sendPlayPacket(SetEntityDataPacket.ID) {
                SetEntityDataPacket(
                    mob.entityId,
                    listOf(SetEntityDataPacket.Entry.Ints(GUARDIAN_BEAM_INDEX, targetId ?: 0)),
                ).write(it)
            }
        }
    }

    override fun setSleeping(
        mob: org.basaltmc.server.runtime.entity.ServerEntity,
        sleeping: Boolean,
    ) {
        if (mob.sleeping == sleeping) return
        val world = worlds().values.firstOrNull { it.state.entity(mob.entityId) != null } ?: return
        mob.sleeping = sleeping
        if (sleeping && mob.type.path == "villager") mob.lastSleptGameTime = world.gameTime
        val pose = if (sleeping) Pose.SLEEPING else Pose.STANDING
        for (player in playerRegistry.snapshot().filter { it.worldName == world.name }) {
            player.connection.sendPlayPacket(SetEntityDataPacket.ID) {
                SetEntityDataPacket(
                    mob.entityId,
                    listOf(SetEntityDataPacket.Entry.Poses(SetEntityDataPacket.POSE_INDEX, pose)),
                ).write(it)
            }
        }
    }

    override fun setCasting(
        mob: org.basaltmc.server.runtime.entity.ServerEntity,
        casting: Boolean,
    ) {
        val world = worlds().values.firstOrNull { it.state.entity(mob.entityId) != null } ?: return
        val spell = if (casting) SPELL_CASTING else SPELL_NONE
        for (player in playerRegistry.snapshot().filter { it.worldName == world.name }) {
            player.connection.sendPlayPacket(SetEntityDataPacket.ID) {
                SetEntityDataPacket(
                    mob.entityId,
                    listOf(SetEntityDataPacket.Entry.Bytes(SPELL_INDEX, spell.toByte())),
                ).write(it)
            }
        }
    }

    override fun throwPotion(
        mob: org.basaltmc.server.runtime.entity.ServerEntity,
        targetX: Double,
        targetY: Double,
        targetZ: Double,
        potion: String,
    ) {
        val world = worlds().values.firstOrNull { it.state.entity(mob.entityId) != null } ?: return
        val protocolType = EntityTypes[Projectiles.SPLASH_POTION.type]?.protocolId ?: return
        val fromY = mob.y + mob.height * POTION_THROW_SHARE
        val dx = targetX - mob.x
        val dz = targetZ - mob.z
        val flat = sqrt(dx * dx + dz * dz)
        val bottle =
            Projectiles.spawn(
                world = world.state,
                kind = Projectiles.SPLASH_POTION,
                entityId = playerRegistry.allocateEntityId(),
                protocolType = protocolType,
                x = mob.x,
                y = fromY,
                z = mob.z,
                dirX = dx,
                // parity: `performRangedAttack`'s own arc, a fifth of the distance across.
                dirY = targetY - fromY + flat * POTION_ARC,
                dirZ = dz,
                power = POTION_POWER,
                uncertainty = POTION_SPREAD,
                random = LegacyRandom(Random.nextLong()),
            )
        bottle.ownerId = mob.entityId
        bottle.potionEffect = potion
        broadcaster.sendEffects(
            playerRegistry.snapshot().filter { it.worldName == world.name },
            floor(mob.x).toInt(),
            floor(mob.y).toInt(),
            floor(mob.z).toInt(),
            Blocks.AIR.stateId,
            listOf(BlockEffect.Sound(SOUND_WITCH_THROW, SoundSource.HOSTILE, 1f, 1f)),
        )
    }

    override fun castFangs(
        mob: org.basaltmc.server.runtime.entity.ServerEntity,
        targetX: Double,
        targetY: Double,
        targetZ: Double,
    ) {
        val world = worlds().values.firstOrNull { it.state.entity(mob.entityId) != null } ?: return
        val watchers = playerRegistry.snapshot().filter { it.worldName == world.name }
        // parity: `Evoker.EvokerAttackSpellGoal`, two lines of fangs walking towards you.
        val angle = atan2(targetZ - mob.z, targetX - mob.x)
        for (step in 0 until FANG_STEPS) {
            val reach = FANG_START + step * FANG_SPACING
            val x = mob.x + cos(angle) * reach
            val z = mob.z + sin(angle) * reach
            for (player in watchers) {
                val dx = player.x - x
                val dz = player.z - z
                if (dx * dx + dz * dz > FANG_HIT_RANGE) continue
                player.connection.hurt(
                    FANG_DAMAGE,
                    DEATH_MOB,
                    DamageTypes.MOB_ATTACK,
                    DamageSource(mob.entityId, mob.x, mob.z, mob.displayName()),
                )
            }
            for (watcher in watchers) {
                watcher.connection.sendPlayPacket(LevelParticlesPacket.ID) {
                    LevelParticlesPacket(
                        explosions.particleId("crit"),
                        x,
                        targetY + FANG_PARTICLE_LIFT,
                        z,
                        count = FANG_PARTICLES,
                    ).write(it)
                }
            }
        }
        broadcaster.sendEffects(
            watchers,
            floor(mob.x).toInt(),
            floor(mob.y).toInt(),
            floor(mob.z).toInt(),
            Blocks.AIR.stateId,
            listOf(BlockEffect.Sound(SOUND_FANGS, SoundSource.HOSTILE, 1f, 1f)),
        )
    }

    override fun summonVexes(
        mob: org.basaltmc.server.runtime.entity.ServerEntity,
        count: Int,
    ) {
        val world = worlds().values.firstOrNull { it.state.entity(mob.entityId) != null } ?: return
        repeat(count) {
            val vex =
                spawning.spawnMob(
                    world,
                    VEX_TYPE,
                    mob.x + Random.nextDouble() * 2.0 - 1.0,
                    mob.y + 1.0,
                    mob.z + Random.nextDouble() * 2.0 - 1.0,
                )
            vex?.targetId = mob.targetId
        }
    }

    override fun applyEffect(
        playerId: Int,
        effect: String,
        durationTicks: Int,
        amplifier: Int,
    ) {
        val player = playerRegistry.snapshot().firstOrNull { it.entityId == playerId } ?: return
        player.connection.addEffect(Identifier.vanilla(effect), durationTicks, amplifier)
    }

    override fun teleported(mob: org.basaltmc.server.runtime.entity.ServerEntity) {
        val world = worlds().values.firstOrNull { it.state.entity(mob.entityId) != null } ?: return
        val watchers = playerRegistry.snapshot().filter { it.worldName == world.name }
        broadcaster.sendEffects(
            watchers,
            floor(mob.x).toInt(),
            floor(mob.y).toInt(),
            floor(mob.z).toInt(),
            Blocks.AIR.stateId,
            listOf(BlockEffect.Sound(SOUND_ENDERMAN_TELEPORT, SoundSource.HOSTILE, 1f, 1f)),
        )
        // The client has to be told where it went: it is a jump, not a walk.
        for (player in watchers) {
            player.connection.sendPlayPacket(EntityTeleportPacket.ID) {
                EntityTeleportPacket(mob.entityId, mob.x, mob.y, mob.z, mob.yaw, mob.pitch, true).write(it)
            }
        }
    }

    override fun primeCreeper(mob: org.basaltmc.server.runtime.entity.ServerEntity) {
        val world = worlds().values.firstOrNull { it.state.entity(mob.entityId) != null } ?: return
        val watchers = playerRegistry.snapshot().filter { it.worldName == world.name }
        broadcaster.sendEffects(
            watchers,
            floor(mob.x).toInt(),
            floor(mob.y).toInt(),
            floor(mob.z).toInt(),
            Blocks.AIR.stateId,
            listOf(BlockEffect.Sound(SOUND_CREEPER_PRIMED, SoundSource.HOSTILE, 1f, 0.5f)),
        )
    }

    override fun explodeCreeper(mob: org.basaltmc.server.runtime.entity.ServerEntity) {
        val world = worlds().values.firstOrNull { it.state.entity(mob.entityId) != null } ?: return
        val watchers = playerRegistry.snapshot().filter { it.worldName == world.name }
        mob.removed = true
        explosions.blowUp(
            world,
            watchers,
            mob.x,
            mob.y,
            mob.z,
            CREEPER_EXPLOSION_POWER,
            "creeper",
            mob.entityId,
        )
    }

    override fun shootArrow(
        mob: org.basaltmc.server.runtime.entity.ServerEntity,
        targetX: Double,
        targetY: Double,
        targetZ: Double,
        power: Float,
    ) {
        val world = worlds().values.firstOrNull { it.state.entity(mob.entityId) != null } ?: return
        val protocolType = EntityTypes[Projectiles.ARROW.type]?.protocolId ?: return
        val fromY = mob.y + mob.height * ARROW_EYE_SHARE
        val dx = targetX - mob.x
        val dy = targetY + ARROW_AIM_SHARE - fromY
        val dz = targetZ - mob.z
        val flat = sqrt(dx * dx + dz * dz)
        val arrow =
            Projectiles.spawn(
                world = world.state,
                kind = Projectiles.ARROW,
                entityId = playerRegistry.allocateEntityId(),
                protocolType = protocolType,
                x = mob.x,
                y = fromY,
                z = mob.z,
                dirX = dx,
                // parity: the arc a skeleton adds, a fifth of the distance to the target.
                dirY = dy + flat * ARROW_ARC,
                dirZ = dz,
                power = ARROW_POWER,
                uncertainty = arrowSpread(world),
                random = LegacyRandom(Random.nextLong()),
            )
        arrow.ownerId = mob.entityId
        broadcaster.sendEffects(
            playerRegistry.snapshot().filter { it.worldName == world.name },
            floor(mob.x).toInt(),
            floor(mob.y).toInt(),
            floor(mob.z).toInt(),
            Blocks.AIR.stateId,
            listOf(BlockEffect.Sound(SOUND_SKELETON_SHOOT, SoundSource.HOSTILE, 1f, 1f)),
        )
    }

    /** parity: `14 - difficulty * 4`, the spread a skeleton's shot has. */
    private fun arrowSpread(world: LoadedWorld): Float =
        when (world.difficulty) {
            "peaceful" -> 14f
            "easy" -> 10f
            "normal" -> 6f
            else -> 2f
        }

    /**
     * parity: `EatBlockGoal`: the grass in front becomes air and the block below becomes dirt, and
     * a lamb that eats grows a little.
     */
    private fun eatGrassUnder(
        world: LoadedWorld,
        mob: org.basaltmc.server.runtime.entity.ServerEntity,
    ) {
        val watchers = playerRegistry.snapshot().filter { it.worldName == world.name }
        val x = floor(mob.x).toInt()
        val y = floor(mob.y).toInt()
        val z = floor(mob.z).toInt()
        val here = world.state.getBlockState(x, y, z)
        val shortGrass = Blocks.REGISTRY.idOf(Identifier.vanilla("short_grass"))
        if (here != null && here == shortGrass) {
            if (world.state.setBlockState(x, y, z, Blocks.AIR.stateId)) {
                broadcaster.announceBlock(
                    watchers,
                    org.basaltmc.server.runtime.world
                        .BlockChange(x, y, z, Blocks.AIR.stateId),
                )
            }
            return
        }
        val below = world.state.getBlockState(x, y - 1, z) ?: return
        if (below != Blocks.GRASS_BLOCK.stateId) return
        val dirt = Blocks.REGISTRY.idOf(Identifier.vanilla("dirt")) ?: return
        if (!world.state.setBlockState(x, y - 1, z, dirt)) return
        broadcaster.announceBlock(
            watchers,
            org.basaltmc.server.runtime.world
                .BlockChange(x, y - 1, z, dirt),
        )
    }

    /**
     * The undead burning in the sun, and what burning costs anything that is on fire.
     *
     * parity: `Mob.burnUndead` and `Entity.baseTick`: eight seconds of fire, a point of damage every
     * twenty ticks of it, and the flames the client draws off the shared flags.
     */
    internal fun tickFire(
        world: LoadedWorld,
        watchers: List<PlayerSession>,
        mob: org.basaltmc.server.runtime.entity.ServerEntity,
        random: LegacyRandom,
    ) {
        val wasBurning = mob.fireTicks > 0
        val blockX = floor(mob.x).toInt()
        val blockY = floor(mob.y).toInt()
        val blockZ = floor(mob.z).toInt()
        if (SunBurn.burns(mob.type)) {
            val day = !Beds.isNight(world.clock.totalTicks)
            val light = WorldLight.skyLight(world, blockX, blockY, blockZ)
            val openSky = WorldLight.skyLight(world, blockX, blockY + 1, blockZ) >= SKY_LIGHT_MAX
            val inWater = Fluids.of(world.state.getBlockState(blockX, blockY, blockZ) ?: 0) == Submersion.WATER
            if (SunBurn.shouldBurn(mob.type, day, light, openSky, inWater, random)) {
                val burning =
                    pluginEvents.emit(PluginEventTypes.ENTITY_COMBUST) { event ->
                        event.set(0, mob.entityId)
                        event.set(1, mob.type.toString())
                        event.set(2, world.name)
                        event.set(3, SunBurn.FIRE_TICKS)
                        event.set(4, "sunlight")
                    }
                if (burning?.cancelled != true) mob.fireTicks = SunBurn.FIRE_TICKS
            }
        }
        if (mob.fireTicks > 0) {
            mob.fireTicks--
            if (mob.fireTicks % SunBurn.DAMAGE_INTERVAL == 0) {
                val result =
                    EntityCombat.hurt(
                        mob,
                        SunBurn.FIRE_DAMAGE,
                        events = pluginEvents,
                        worldName = world.name,
                        cause = "fire",
                    )
                if (result.landed) combat.announceEntityHit(watchers, mob, DamageTypes.ON_FIRE, -1, result.died)
            }
        }
        val burning = mob.fireTicks > 0
        if (burning == wasBurning) return
        val flags = if (burning) SetEntityDataPacket.FLAG_ON_FIRE else 0
        for (player in watchers) {
            player.connection.sendPlayPacket(SetEntityDataPacket.ID) {
                SetEntityDataPacket(
                    mob.entityId,
                    listOf(SetEntityDataPacket.Entry.Bytes(SetEntityDataPacket.SHARED_FLAGS_INDEX, flags.toByte())),
                ).write(it)
            }
        }
    }

    /**
     * One mob's turn to decide and then move.
     *
     * parity: `Mob.serverAiStep`: the selectors decide on every other tick (staggered by the entity
     * id, so half the mobs decide on each), and then the navigation, the move control and the look
     * control run every tick.
     */
    internal fun tickMobAi(
        world: LoadedWorld,
        mob: org.basaltmc.server.runtime.entity.ServerEntity,
        views: List<PlayerView>,
        living: List<org.basaltmc.server.runtime.entity.ServerEntity>,
        random: LegacyRandom,
    ) {
        val selector = mob.goals ?: MobGoals.forType(mob.type)?.also { mob.goals = it } ?: return
        val context =
            MobContext(
                world = world.state,
                mob = mob,
                random = random,
                // The tick's own snapshot, taken once for the whole world: asking for it per mob
                // turned a pen of cows into a copy of the entity list per cow per tick.
                entities = living,
                players = views,
                actions = this,
                light = lightAt(world, MobCombat.blockPosition(mob)),
                night = Beds.isNight(world.clock.totalTicks),
                dayTime = world.clock.totalTicks,
                gameTime = world.gameTime,
                raidNearby = raids.raidManagers[world.name]?.nearby(mob.x, mob.y, mob.z) != null,
                speed = mob.attributeWithModifiers(Attribute.MOVEMENT_SPEED),
            )
        // parity: `Mob.serverAiStep`, which decides on every other tick and staggers the mobs by
        // their own id so they do not all think on the same one.
        if ((mob.tickCount + mob.entityId) % 2 != 0) {
            selector.tickRunning(context, all = false)
        } else {
            selector.tick(context)
        }
        // parity: `Mob.travel`'s flying branch: what flies is never walked along a path, and the
        // goals move it themselves, so all that is left here is the drift.
        if (Flyers.flies(mob.type)) {
            org.basaltmc.server.runtime.entity.FlyingMovement
                .tick(world.state, mob)
            LookControl.tick(mob)
            return
        }
        Navigation.tick(world.state, mob)
        MoveControl.tick(world.state, mob, context.speed)
        LookControl.tick(mob)
    }

    /** Whether somebody is stood close enough to a cart to shove it. parity: two boxes touching. */
    internal fun touchesCart(
        cart: org.basaltmc.server.runtime.entity.ServerEntity,
        player: PlayerSession,
    ): Boolean {
        val reach = (cart.width + EntityTypeShapes.widthOf(PLAYER_TYPE)) / 2.0
        if (abs(player.x - cart.x) > reach || abs(player.z - cart.z) > reach) return false
        return player.y <= cart.y + cart.height && player.y + EntityTypeShapes.heightOf(PLAYER_TYPE) >= cart.y
    }

    /** The light a mob is standing in, which is all the spiders need to know. */
    private fun lightAt(
        world: LoadedWorld,
        position: IntArray,
    ): Int {
        val block = WorldLight.blockLight(world, position[0], position[1], position[2])
        val sky = WorldLight.skyLight(world, position[0], position[1], position[2])
        // parity: `Entity.getLightLevelDependentMagicValue`, whose sky half is darkened by the time
        // of day. Night is taken as darkness rather than tracking the sky brightness itself.
        val daylight = if (Beds.isNight(world.clock.totalTicks)) 0 else sky
        return maxOf(block, daylight)
    }

    /** parity: `Entity.die` over `LootTable`, the drops a mob leaves where it fell. */
    internal fun dropDeathLoot(
        world: LoadedWorld,
        entity: org.basaltmc.server.runtime.entity.ServerEntity,
    ) {
        val table = lootTables.forEntity(entity.type) ?: return
        for (stack in table.generate(LegacyRandom(Random.nextLong()))) {
            if (Items.REGISTRY.byIdentifier(stack.item) == null) continue
            world.state.spawnItem(
                WorldState.DroppedItem(
                    entityId = playerRegistry.allocateEntityId(),
                    uuid = Uuid.random(),
                    item = stack.item,
                    count = stack.count,
                    x = entity.x,
                    y = entity.y + entity.height / 2,
                    z = entity.z,
                ),
            )
        }
    }

    /** parity: `Animal.spawnChildFromBreeding`: the calf appears where its parent stands. */
    internal fun breed(
        world: LoadedWorld,
        animal: org.basaltmc.server.runtime.entity.ServerEntity,
        partner: org.basaltmc.server.runtime.entity.ServerEntity,
    ) {
        val protocolType = EntityTypes[animal.type]?.protocolId ?: return
        val breeding =
            pluginEvents.emit(PluginEventTypes.ENTITY_BREED) { event ->
                event.set(0, animal.entityId)
                event.set(1, animal.type.toString())
                event.set(2, world.name)
                event.set(3, partner.entityId)
            }
        if (breeding?.cancelled == true) return
        val baby =
            org.basaltmc.server.runtime.entity.ServerEntity(
                entityId = playerRegistry.allocateEntityId(),
                uuid = Uuid.random(),
                type = animal.type,
                protocolType = protocolType,
                x = animal.x,
                y = animal.y,
                z = animal.z,
            )
        baby.age = Breeding.BABY_AGE
        world.state.spawnEntity(baby)
        Breeding.afterBreeding(animal, partner)
    }

    private companion object {
        /** parity: `EntityTypes.PLAYER`, whose box is what a mob's reach is measured against. */
        val PLAYER_TYPE: Identifier = Identifier.vanilla("player")

        /** parity: `death.attack.mob`, the message a mob with no name kills you with. */
        const val DEATH_MOB = "mob"

        /** parity: `PanicGoal`, which runs for as long as the last hit is remembered. */
        const val PANIC_TICKS = 40

        /** parity: `canSeeSky`, which here is the full fifteen of sky light above the mob. */
        const val SKY_LIGHT_MAX = 15

        /** parity: `Creeper.explosionRadius`, and the two sounds its fuse and a bow make. */
        const val CREEPER_EXPLOSION_POWER = 3f
        const val SOUND_CREEPER_PRIMED = "entity.creeper.primed"
        const val SOUND_ENDERMAN_TELEPORT = "entity.enderman.teleport"
        const val SOUND_FIREBALL_SHOOT = "entity.ghast.shoot"
        const val SOUND_WITCH_THROW = "entity.witch.throw"
        const val SOUND_FANGS = "entity.evoker.prepare_attack"

        /** parity: `SpellcasterIllager`'s own spell byte, and the pose it puts the model in. */
        const val SPELL_INDEX = 17
        const val SPELL_CASTING = 1
        const val SPELL_NONE = 0

        /** parity: `Witch.performRangedAttack`, thrown at 0.75 with eight degrees of spread. */
        const val POTION_POWER = 0.75f
        const val POTION_SPREAD = 8f
        const val POTION_ARC = 0.2
        const val POTION_THROW_SHARE = 0.5

        /** parity: `EvokerAttackSpellGoal`, sixteen fangs walking out at six damage each. */
        const val FANG_STEPS = 16
        const val FANG_START = 1.5
        const val FANG_SPACING = 1.0
        const val FANG_HIT_RANGE = 1.0
        const val FANG_DAMAGE = 6f
        const val FANG_PARTICLES = 4
        const val FANG_PARTICLE_LIFT = 0.5

        val VEX_TYPE: Identifier = Identifier.vanilla("vex")

        /** parity: `Ghast.DATA_IS_CHARGING`, `Blaze`'s own flags byte and `Guardian`'s beam target. */
        const val GHAST_CHARGING_INDEX = 16
        const val BLAZE_CHARGED_INDEX = 16
        const val BLAZE_CHARGED_FLAG = 1
        const val GUARDIAN_BEAM_INDEX = 17

        /** parity: `Ghast.getExplosionPower`, and the speed a fireball is thrown at. */
        const val GHAST_EXPLOSION_POWER = 1f
        const val FIREBALL_POWER = 0.5f
        const val SMALL_FIREBALL_SPREAD = 4f
        const val FIREBALL_EYE_SHARE = 0.5

        const val SOUND_SKELETON_SHOOT = "entity.skeleton.shoot"

        /** parity: `performRangedAttack`: from the eyes, aimed a third up the target, at 1.6. */
        const val ARROW_EYE_SHARE = 0.85
        const val ARROW_AIM_SHARE = 0.3333333333333333
        const val ARROW_ARC = 0.2
        const val ARROW_POWER = 1.6f
    }
}
