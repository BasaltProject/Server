package org.basaltmc.server.runtime

import org.basaltmc.server.random.LegacyRandom
import org.basaltmc.server.runtime.entity.Breeding
import org.basaltmc.server.runtime.entity.EntityCombat
import org.basaltmc.server.runtime.entity.MobBrain
import org.basaltmc.server.runtime.entity.MobCategory
import org.basaltmc.server.runtime.entity.MobDespawn
import org.basaltmc.server.runtime.entity.Mobs
import org.basaltmc.server.runtime.entity.ServerEntity
import org.basaltmc.server.runtime.entity.WanderingTraderLifetime
import org.basaltmc.server.runtime.entity.Wardens
import org.basaltmc.server.runtime.entity.ai.PlayerView
import org.basaltmc.server.runtime.player.PlayerSession
import org.basaltmc.server.runtime.world.LoadedWorld
import org.basaltmc.server.runtime.world.SkeletonHorseTrap
import org.basaltmc.wire.protocol.play.Gamemode

/** Ticks one mob vehicle and its passenger tree in vanilla's root-first order. */
internal class MobTickSystem(
    private val actions: MobActionSystem,
) {
    /** Whether the outer entity loop must leave this entity to its vehicle's recursive turn. */
    fun hasActiveVehicle(
        world: LoadedWorld,
        entity: ServerEntity,
    ): Boolean {
        val vehicleId = entity.vehicleId ?: return false
        val vehicle = world.state.entity(vehicleId)
        if (vehicle != null && !vehicle.removed && entity.entityId in vehicle.passengerIds) return true
        vehicle?.passengerIds?.remove(entity.entityId)
        entity.vehicleId = null
        return false
    }

    fun tickTree(
        world: LoadedWorld,
        root: ServerEntity,
        watchers: List<PlayerSession>,
        views: List<PlayerView>,
        living: List<ServerEntity>,
        random: LegacyRandom,
    ) {
        tickMob(world, root, watchers, views, living, random)
        tickPassengers(world, root, watchers, views, living, random, mutableSetOf(root.entityId))
    }

    fun tickPassengers(
        world: LoadedWorld,
        vehicle: ServerEntity,
        watchers: List<PlayerSession>,
        views: List<PlayerView>,
        living: List<ServerEntity>,
        random: LegacyRandom,
        visited: MutableSet<Int> = mutableSetOf(vehicle.entityId),
    ) {
        for (passengerId in vehicle.passengerIds.toList()) {
            val player = watchers.firstOrNull { it.entityId == passengerId }
            if (player != null) {
                if (player.vehicleId != vehicle.entityId) vehicle.passengerIds.remove(passengerId)
                continue
            }
            val passenger = world.state.entity(passengerId)
            if (
                passenger == null ||
                passenger.removed ||
                passenger.vehicleId != vehicle.entityId ||
                !visited.add(passengerId)
            ) {
                vehicle.passengerIds.remove(passengerId)
                if (passenger?.vehicleId == vehicle.entityId) passenger.vehicleId = null
                continue
            }
            passenger.velocityX = 0.0
            passenger.velocityY = 0.0
            passenger.velocityZ = 0.0
            if (Mobs.isMob(passenger.type)) {
                tickMob(world, passenger, watchers, views, living, random)
            }
            EntityRiding.position(vehicle, passenger)
            tickPassengers(world, passenger, watchers, views, living, random, visited)
        }
    }

    private fun tickMob(
        world: LoadedWorld,
        entity: ServerEntity,
        watchers: List<PlayerSession>,
        views: List<PlayerView>,
        living: List<ServerEntity>,
        random: LegacyRandom,
    ) {
        entity.idleTicks++
        entity.tickCount++
        EntityCombat.tick(entity)
        if (Wardens.isWarden(entity.type)) {
            Wardens.tick(entity)
            if (entity.touchCooldown > 0) entity.touchCooldown--
        }
        if (entity.panicTicks > 0) entity.panicTicks--
        actions.tickFire(world, watchers, entity, random)
        if (entity.type.path == "skeleton_horse" && entity.skeletonTrap) {
            val rangeSquared = SkeletonHorseTrap.PLAYER_RANGE * SkeletonHorseTrap.PLAYER_RANGE
            val nearbyAlivePlayer =
                !entity.noAi &&
                    watchers.any { player ->
                        if (player.dead || player.gamemode == Gamemode.SPECTATOR) return@any false
                        val dx = player.x - entity.x
                        val dy = player.y - entity.y
                        val dz = player.z - entity.z
                        dx * dx + dy * dy + dz * dz <= rangeSquared
                    }
            if (nearbyAlivePlayer) actions.activateSkeletonTrap(world, entity, random)
            SkeletonHorseTrap.tickLifetime(entity)
            if (entity.removed) return
        }
        if (!entity.noAi) {
            actions.tickMobAi(world, entity, views, living, random)
            if (entity.type.path == "villager") actions.tickVillageGolem(world, entity, living, random)
            val partner = Breeding.tick(entity, living)
            if (partner != null) actions.breed(world, entity, partner)
        }
        if (entity.type.path == "wandering_trader" || entity.type.path == "trader_llama") {
            WanderingTraderLifetime.tick(entity, living, watchers.mapTo(hashSetOf()) { it.entityId })
            if (entity.removed) return
        }
        val category = MobCategory.byName(Mobs.categoryOf(entity.type)) ?: MobCategory.MISC
        val nearest = watchers.minOfOrNull { MobBrain.distanceTo(entity, it.x, it.y, it.z) }
        if (MobDespawn.shouldDespawn(entity, category, nearest, random)) entity.removed = true
    }
}

/** Attachment points used by the rider combinations Basalt currently creates. */
internal object EntityRiding {
    fun position(
        vehicle: ServerEntity,
        passenger: ServerEntity,
    ) {
        passenger.x = vehicle.x
        passenger.y = vehicle.y + passengerAttachment(vehicle) - vehicleAttachment(passenger)
        passenger.z = vehicle.z
    }

    private fun passengerAttachment(vehicle: ServerEntity): Double =
        when (vehicle.type.path) {
            "spider" -> SPIDER_PASSENGER_Y
            else -> vehicle.height
        }

    private fun vehicleAttachment(passenger: ServerEntity): Double =
        when (passenger.type.path) {
            "bogged", "parched", "skeleton", "stray" -> ABSTRACT_SKELETON_VEHICLE_Y
            "wither_skeleton" -> WITHER_SKELETON_VEHICLE_Y
            else -> 0.0
        }

    private const val SPIDER_PASSENGER_Y = 0.765
    private const val ABSTRACT_SKELETON_VEHICLE_Y = 0.7
    private const val WITHER_SKELETON_VEHICLE_Y = 0.875
}
