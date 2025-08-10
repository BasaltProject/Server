package org.basaltmc.server.runtime

import org.basaltmc.server.plugins.PluginEventBus
import org.basaltmc.server.plugins.PluginEventTypes
import org.basaltmc.server.random.LegacyRandom
import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.registry.Items
import org.basaltmc.server.registry.ParticleTypes
import org.basaltmc.server.runtime.player.PlayerRegistry
import org.basaltmc.server.runtime.player.PlayerSession
import org.basaltmc.server.runtime.plugins.emit
import org.basaltmc.server.runtime.world.DamageTypes
import org.basaltmc.server.runtime.world.Explosions
import org.basaltmc.server.runtime.world.LoadedWorld
import org.basaltmc.server.runtime.world.NeighbourUpdates
import org.basaltmc.server.runtime.world.RealLootTables
import org.basaltmc.server.runtime.world.WorldState
import org.basaltmc.wire.common.Identifier
import org.basaltmc.wire.protocol.play.BlockPos
import org.basaltmc.wire.protocol.play.ClientboundBlockUpdatePacket
import org.basaltmc.wire.protocol.play.ExplodePacket
import org.basaltmc.wire.protocol.play.ExplosionParticle
import kotlin.math.sqrt
import kotlin.random.Random
import kotlin.uuid.Uuid

/**
 * Everything an explosion owes the world and the clients watching it: what it breaks, what that
 * drops, and who it hurts.
 */
public class ExplosionSystem(
    private val playerRegistry: PlayerRegistry,
    private val pluginEvents: PluginEventBus,
    private val lootTables: RealLootTables,
) {
    /** parity: `Explosion.isSmall`, which is what picks the small flash over the big one. */
    private val largeExplosion = 2.0f

    public fun particleId(path: String): Int = ParticleTypes[Identifier.vanilla(path)]?.protocolId ?: 0

    /**
     * Sets off an explosion, drops what it broke and hurts whoever was standing in it.
     *
     * parity: `ServerExplosion`, the same shape as the bed's own: what it breaks drops with no tool
     * involved, and the neighbours of every hole are told afterwards.
     */
    public fun blowUp(
        world: LoadedWorld,
        watchers: List<PlayerSession>,
        x: Double,
        y: Double,
        z: Double,
        power: Float,
        /** What blew up, for a plugin that lets creepers off and stops TNT. */
        cause: String = "explosion",
        sourceEntityId: Int? = null,
    ) {
        val stopped =
            pluginEvents.emit(PluginEventTypes.BLOCK_EXPLODE) { event ->
                event.set(0, world.name)
                event.set(1, x)
                event.set(2, y)
                event.set(3, z)
                event.set(4, power.toDouble())
                event.set(5, cause)
                event.set(6, sourceEntityId)
            }
        if (stopped?.cancelled == true) return
        val broken = Explosions.explode(world, x, y, z, power, LegacyRandom(Random.nextLong()))
        // parity: `ServerExplosion.createDrops`, which gathers everything the blast broke and only
        // then drops it: what an explosion leaves is a handful of merged stacks, not one item per
        // block, and that is what a real client shows.
        val drops = mutableListOf<ExplosionDrop>()
        // parity: `ServerLevel.explode`'s own packet, which is the flash, the smoke thrown off the
        // blocks and the bang. The blocks themselves go out as ordinary updates below, so without
        // this an explosion was a wall of stone silently turning into air.
        val explode =
            ExplodePacket(
                x = x,
                y = y,
                z = z,
                radius = power,
                blockCount = broken.size,
                particleId = particleId(if (power < largeExplosion) "explosion" else "explosion_emitter"),
                sound = "entity.generic.explode",
                blockParticles =
                    listOf(
                        ExplosionParticle(particleId("poof"), ExplodePacket.POOF_SCALING, 1f),
                        ExplosionParticle(particleId("smoke"), ExplodePacket.SMOKE_SCALING, 1f),
                    ),
            )
        for (player in watchers) {
            player.connection.sendPlayPacket(ExplodePacket.ID) { explode.write(it) }
        }
        for (block in broken) {
            val at = BlockPos(block.x, block.y, block.z)
            for (player in watchers) {
                player.connection.sendPlayPacket(ClientboundBlockUpdatePacket.ID) {
                    ClientboundBlockUpdatePacket(at, Blocks.AIR.stateId).write(it)
                }
            }
            Blocks.byStateId(block.stateId)?.let { collectExplodedBlock(world, it.block, at, power, drops) }
        }
        for (block in broken) {
            NeighbourUpdates.afterChange(
                world,
                block.x,
                block.y,
                block.z,
                onReshaped = { stateId, bx, by, bz ->
                    val at = BlockPos(bx, by, bz)
                    for (player in watchers) {
                        player.connection.sendPlayPacket(ClientboundBlockUpdatePacket.ID) {
                            ClientboundBlockUpdatePacket(at, stateId).write(it)
                        }
                    }
                },
            ) { stateId, bx, by, bz ->
                val at = BlockPos(bx, by, bz)
                for (player in watchers) {
                    player.connection.sendPlayPacket(ClientboundBlockUpdatePacket.ID) {
                        ClientboundBlockUpdatePacket(at, Blocks.AIR.stateId).write(it)
                    }
                }
                // What the blast knocked loose afterwards falls as an ordinary break, not as part
                // of the explosion: parity with `dropResources`, which carries no radius.
                Blocks.byStateId(stateId)?.let { collectExplodedBlock(world, it.block, at, power = 0f, drops) }
            }
        }
        for (drop in drops) popResource(world, drop)
        for (player in watchers) {
            val distance =
                sqrt(
                    (player.x - x) * (player.x - x) + (player.y - y) * (player.y - y) + (player.z - z) * (player.z - z),
                )
            val damage = Explosions.damageAt(distance, power, power * 2f)
            if (damage <= 0f) continue
            player.connection.hurt(damage, "explosion", DamageTypes.EXPLOSION)
        }
    }

    /** One stack the blast is holding on to until it decides where it lands. */
    private class ExplosionDrop(
        val item: Identifier,
        var count: Int,
        val x: Int,
        val y: Int,
        val z: Int,
    )

    /**
     * What one exploded block leaves, merged into what the blast is already carrying.
     *
     * parity: `ServerExplosion.addOrAppendStack`, which merges into the first stack that will take
     * it, up to sixteen, and only starts a new one when none will.
     */
    private fun collectExplodedBlock(
        world: LoadedWorld,
        block: Identifier,
        pos: BlockPos,
        power: Float,
        drops: MutableList<ExplosionDrop>,
    ) {
        val table = lootTables.forBlock(block) ?: return
        // parity: `LootContextParams.EXPLOSION_RADIUS`, which `explosion_decay` turns into the one
        // in `power` chance each item has of surviving the blast.
        for (stack in table.generate(LegacyRandom(Random.nextLong()), explosionRadius = power)) {
            if (Items.REGISTRY.byIdentifier(stack.item) == null) continue
            var remaining = stack.count
            for (drop in drops) {
                if (drop.item != stack.item || drop.count >= EXPLOSION_MERGE_LIMIT) continue
                val moved = minOf(remaining, EXPLOSION_MERGE_LIMIT - drop.count)
                drop.count += moved
                remaining -= moved
                if (remaining <= 0) break
            }
            if (remaining > 0) drops += ExplosionDrop(stack.item, remaining, pos.x, pos.y, pos.z)
        }
    }

    /** parity: `Block.popResource`, which scatters the stack inside its own block and pops it up. */
    private fun popResource(
        world: LoadedWorld,
        drop: ExplosionDrop,
    ) {
        world.state.spawnItem(
            WorldState.DroppedItem(
                entityId = playerRegistry.allocateEntityId(),
                uuid = Uuid.random(),
                item = drop.item,
                count = drop.count,
                x = drop.x + 0.5 + jitter(),
                y = drop.y + 0.5 + jitter() - ITEM_HALF_HEIGHT,
                z = drop.z + 0.5 + jitter(),
                velocityX = Random.nextDouble() * POP_SPREAD - POP_SPREAD / 2.0,
                velocityY = POP_LIFT,
                velocityZ = Random.nextDouble() * POP_SPREAD - POP_SPREAD / 2.0,
            ),
        )
    }

    /** parity: `Mth.nextDouble(random, -0.25, 0.25)`, the quarter block a popped stack lands within. */
    private fun jitter(): Double = Random.nextDouble() * 2 * POP_JITTER - POP_JITTER

    private companion object {
        /** parity: `ItemEntity.merge(stack, input, 16)`, the cap on what one blast stacks together. */
        const val EXPLOSION_MERGE_LIMIT = 16

        /** parity: `Block.popResource` and `ItemEntity`'s own opening movement. */
        const val POP_JITTER = 0.25
        const val POP_SPREAD = 0.2
        const val POP_LIFT = 0.2
        const val ITEM_HALF_HEIGHT = 0.125
    }
}
