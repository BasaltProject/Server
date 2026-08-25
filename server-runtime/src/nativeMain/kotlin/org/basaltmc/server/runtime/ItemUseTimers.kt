package org.basaltmc.server.runtime

import org.basaltmc.server.plugins.PluginEventTypes
import org.basaltmc.server.random.LegacyRandom
import org.basaltmc.server.registry.EntityTypes
import org.basaltmc.server.registry.ItemFood
import org.basaltmc.server.registry.ItemStacking
import org.basaltmc.server.runtime.entity.Projectiles
import org.basaltmc.server.runtime.player.FoodData
import org.basaltmc.server.runtime.player.PlayerInventory
import org.basaltmc.server.runtime.player.PlayerSession
import org.basaltmc.server.runtime.plugins.emit
import org.basaltmc.wire.common.Identifier
import org.basaltmc.wire.protocol.play.CooldownPacket
import org.basaltmc.wire.protocol.play.Gamemode
import org.basaltmc.wire.protocol.play.SetEntityDataPacket
import org.basaltmc.wire.protocol.play.SoundSource
import kotlin.math.PI
import kotlin.math.cos
import kotlin.math.sin
import kotlin.random.Random

/**
 * Eating, drawing a bow and the cooldown clock: what a player is doing with the item in their
 * hand, and for how much longer.
 *
 * A back-reference to the owning session rather than narrow callbacks: this state was already
 * tightly coupled to [ClientSession] before the extraction, and every method here still needs a
 * handful of its collaborators (the world, the plugin bus, the outbound packets).
 */
internal class ItemUseTimers(
    private val owner: ClientSession,
) {
    /** Which slot is being eaten from, and how many ticks of the mouthful are left. */
    private var usingItem = false
    private var eatingSlot = -1
    private var eatingTicks = 0

    /** What cannot be used again yet, by item name and the tick it frees up on. parity: `ItemCooldowns`. */
    private val itemCooldowns = HashMap<String, Long>()

    /** The bow being drawn, and for how long. parity: `LivingEntity.useItem`/`useItemRemaining`. */
    private var drawingSlot = -1
    private var drawingTicks = 0

    /**
     * Starts eating what is in hand, if it is food and there is room for it.
     *
     * parity: `ServerPlayer.startUsingItem`. The server times the mouthful itself rather than
     * trusting the client to say when it finished, which is what stops one click feeding a player
     * twice.
     */
    internal fun startEating(
        session: PlayerSession,
        item: Identifier,
        slot: Int,
    ): Boolean {
        val food = ItemFood.of(item) ?: return false
        // A click on a block and the click that follows it are one mouthful, not two: the client
        // sends `use_item_on` and then `use_item` for the same press, and restarting the timer on
        // the second one meant a held button never finished a single one. parity: vanilla starts the
        // use once because its `useItemOn` never falls through to `use` on the same packet.
        if (eatingSlot == slot) return true
        val eating =
            owner.playerEvent(PluginEventTypes.PLAYER_ITEM_CONSUME) { event -> event.set(3, item.toString()) }
        if (eating?.cancelled == true) {
            eating.cancelMessage?.let { owner.sendPluginNotice(it) }
            owner.sendInventory()
            return false
        }
        if (!food.canAlwaysEat && session.food.foodLevel >= FoodData.MAX_FOOD) {
            // parity: `ServerPlayerGameMode.useItem` sends the whole inventory back whenever the use
            // never started. Without it a client that had already predicted the mouthful kept the
            // animation and the missing item for good, which is what a full player saw here.
            owner.sendInventoryAll()
            owner.syncHealth()
            return true
        }
        eatingSlot = slot
        eatingTicks = food.consumeTicks
        setUsingItem(true)
        return true
    }

    /**
     * Starts drawing a bow, if there is anything to shoot with it.
     *
     * parity: `BowItem.use`, which refuses without an arrow unless the player has infinite
     * materials, and otherwise just starts the draw: the shot happens on release.
     */
    internal fun startDrawing(
        session: PlayerSession,
        item: Identifier,
        slot: Int,
    ): Boolean {
        if (item.path != ClientSession.BOW) return false
        if (session.gamemode != Gamemode.CREATIVE && session.inventory.indexOf(ClientSession.ARROW_ITEM) == null) {
            return true
        }
        drawingSlot = slot
        drawingTicks = 0
        setUsingItem(true)
        return true
    }

    internal fun stopDrawing() {
        drawingSlot = -1
        drawingTicks = 0
        setUsingItem(false)
    }

    /**
     * Lets a drawn bow go.
     *
     * parity: `BowItem.releaseUsing`: the power is the square of the draw, a shot under a tenth is
     * no shot at all, and a full draw flies at three times that and counts as a critical.
     */
    internal fun releaseBow() {
        val session = owner.playerSession ?: return
        if (drawingSlot < 0) return
        val held = session.inventory[drawingSlot]
        val drawn = drawingTicks
        stopDrawing()
        if (held == null || Identifier.parse(held.id).path != ClientSession.BOW) return
        val power = bowPower(drawn)
        if (power < ClientSession.MIN_BOW_POWER) return
        val creative = session.gamemode == Gamemode.CREATIVE
        val arrowSlot = session.inventory.indexOf(ClientSession.ARROW_ITEM)
        if (!creative && arrowSlot == null) return
        val world = owner.currentWorld ?: return
        val entityId = owner.playerRegistry?.allocateEntityId() ?: return
        val protocolType = EntityTypes[Projectiles.ARROW.type]?.protocolId ?: return
        val shooting =
            owner.pluginEvents.emit(PluginEventTypes.ENTITY_SHOOT) { event ->
                event.set(0, session.entityId)
                event.set(1, "minecraft:player")
                event.set(2, owner.currentWorld?.name)
                event.set(3, Projectiles.ARROW.type.toString())
                event.set(4, power.toDouble())
            }
        if (shooting?.cancelled == true) return

        val pitch = session.pitch * PI.toFloat() / ClientSession.HALF_CIRCLE
        val yaw = session.yaw * PI.toFloat() / ClientSession.HALF_CIRCLE
        val arrow =
            Projectiles.spawn(
                world = world.state,
                kind = Projectiles.ARROW,
                entityId = entityId,
                protocolType = protocolType,
                x = session.x,
                y = session.y + ClientSession.THROW_EYE_HEIGHT,
                z = session.z,
                dirX = (-sin(yaw) * cos(pitch)).toDouble(),
                dirY = (-sin(pitch)).toDouble(),
                dirZ = (cos(yaw) * cos(pitch)).toDouble(),
                power = power * ClientSession.BOW_POWER_SCALE,
                uncertainty = ClientSession.BOW_UNCERTAINTY,
                random = LegacyRandom(Random.nextLong()),
            )
        arrow.ownerId = session.entityId
        arrow.critical = power >= 1f
        owner.broadcastSound(
            ClientSession.SOUND_ARROW_SHOOT,
            session.x,
            session.y,
            session.z,
            SoundSource.PLAYERS,
            pitch = 1f / (Random.nextFloat() * 0.4f + 1.2f) + power * 0.5f,
        )
        if (creative || arrowSlot == null) return
        val stack = session.inventory[arrowSlot] ?: return
        session.inventory[arrowSlot] = if (stack.count <= 1) null else stack.copy(count = stack.count - 1)
        owner.sendInventorySlot(arrowSlot)
    }

    /** One tick of the draw. Letting go of the bow, or of the slot it is in, gives it up. */
    internal fun tickDrawing(session: PlayerSession) {
        if (drawingSlot < 0) return
        val held = session.inventory[drawingSlot]
        if (held == null ||
            Identifier.parse(held.id).path != ClientSession.BOW ||
            drawingSlot != PlayerInventory.HOTBAR_START + session.inventory.heldSlot
        ) {
            stopDrawing()
            return
        }
        drawingTicks++
    }

    /**
     * parity: `EnderpearlItem.use`: the pearl flies at one and a half, with a degree of spread, and
     * one leaves the hand however far it gets.
     */
    internal fun throwEnderPearl(
        session: PlayerSession,
        item: Identifier,
        slot: Int,
    ): Boolean {
        if (item.path != ClientSession.ENDER_PEARL) return false
        val world = owner.currentWorld ?: return true
        val entityId = owner.playerRegistry?.allocateEntityId() ?: return true
        val protocolType = EntityTypes[Projectiles.ENDER_PEARL.type]?.protocolId ?: return true
        val pitch = session.pitch * PI.toFloat() / ClientSession.HALF_CIRCLE
        val yaw = session.yaw * PI.toFloat() / ClientSession.HALF_CIRCLE
        val pearl =
            Projectiles.spawn(
                world = world.state,
                kind = Projectiles.ENDER_PEARL,
                entityId = entityId,
                protocolType = protocolType,
                x = session.x,
                y = session.y + ClientSession.THROW_EYE_HEIGHT,
                z = session.z,
                dirX = (-sin(yaw) * cos(pitch)).toDouble(),
                dirY = (-sin(pitch)).toDouble(),
                dirZ = (cos(yaw) * cos(pitch)).toDouble(),
                power = ClientSession.PEARL_POWER,
                uncertainty = ClientSession.PEARL_UNCERTAINTY,
                random = LegacyRandom(Random.nextLong()),
            )
        pearl.ownerId = session.entityId
        startCooldown(item)
        owner.broadcastSound(
            ClientSession.SOUND_PEARL_THROW,
            session.x,
            session.y,
            session.z,
            SoundSource.NEUTRAL,
            volume = 0.5f,
            pitch = 0.4f / (Random.nextFloat() * 0.4f + 0.8f),
        )
        if (session.gamemode == Gamemode.CREATIVE) return true
        val held = session.inventory[slot] ?: return true
        session.inventory[slot] = if (held.count <= 1) null else held.copy(count = held.count - 1)
        owner.sendInventorySlot(slot)
        return true
    }

    /** Whether this item is still greyed out. parity: `ItemCooldowns.isOnCooldown`. */
    internal fun onCooldown(item: Identifier): Boolean {
        val until = itemCooldowns[item.path] ?: return false
        if (until > owner.playTimeTicks) return true
        itemCooldowns.remove(item.path)
        return false
    }

    /**
     * Starts the wait an item's own `use_cooldown` component asks for, and tells the client so the
     * slot greys out. parity: `UseCooldown.apply`.
     */
    internal fun startCooldown(item: Identifier) {
        val ticks = ItemStacking.cooldownTicks(item)
        if (ticks <= 0) return
        itemCooldowns[item.path] = owner.playTimeTicks + ticks
        owner.sendPlayPacket(CooldownPacket.ID) { CooldownPacket(item.toString(), ticks).write(it) }
    }

    /**
     * The "this one is using an item" flag, sent to everybody including its owner.
     *
     * parity: `LivingEntity.setLivingEntityFlag(1, ...)`. The owner's own client needs it as much as
     * the onlookers do: `LocalPlayer.isUsingItem` is its own flag, and the only thing that clears it
     * is this metadata coming back saying the server is no longer using anything. Without it the
     * client thought it was still eating for ever, so holding the button never started a second
     * mouthful and the animation stayed up.
     */
    internal fun setUsingItem(using: Boolean) {
        if (usingItem == using) return
        usingItem = using
        val session = owner.playerSession ?: return
        val flags = if (using) ClientSession.LIVING_FLAG_USING_ITEM else 0
        val packet =
            SetEntityDataPacket(
                session.entityId,
                listOf(SetEntityDataPacket.Entry.Bytes(SetEntityDataPacket.LIVING_FLAGS_INDEX, flags.toByte())),
            )
        owner.sendPlayPacket(SetEntityDataPacket.ID) { packet.write(it) }
        for (other in owner.playersInSameWorld()) {
            if (other.entityId == session.entityId) continue
            other.connection.sendPlayPacket(SetEntityDataPacket.ID) { packet.write(it) }
        }
    }

    /** parity: `BowItem.getPowerForTime`. */
    internal fun bowPower(ticks: Int): Float {
        val drawn = ticks / ClientSession.BOW_DRAW_TICKS
        return minOf((drawn * drawn + drawn * 2f) / 3f, 1f)
    }

    internal fun stopEating() {
        eatingSlot = -1
        eatingTicks = 0
        setUsingItem(false)
    }

    /** One tick of the mouthful, and the swallow at the end of it. */
    internal fun tickEating(session: PlayerSession) {
        if (eatingSlot < 0) return
        val held = session.inventory[eatingSlot]
        val food = held?.let { ItemFood.of(Identifier.parse(it.id)) }
        // Anything that moved the stack under them (dropping it, a click, swapping slots) stops it.
        if (food == null || eatingSlot != PlayerInventory.HOTBAR_START + session.inventory.heldSlot) {
            stopEating()
            return
        }
        if (--eatingTicks > 0) return
        val slot = eatingSlot
        stopEating()
        session.food.eat(food.nutrition, food.saturation)
        owner.applyConsumeEffects(session, food.effects)
        if (session.gamemode != Gamemode.CREATIVE) {
            val remaining = held.count - 1
            // parity: `use_remainder` replaces what was eaten: the bowl, the bottle, the bucket.
            val leftover = food.remainder?.let { held.copy(id = it.toString(), count = 1) }
            session.inventory[slot] =
                when {
                    remaining > 0 -> held.copy(count = remaining)
                    else -> leftover
                }
            owner.sendInventorySlot(slot)
            // A stack of two bowls of stew leaves the second bowl looking for a slot of its own,
            // and on the floor when there is none.
            if (remaining > 0 && leftover != null && session.inventory.add(leftover) > 0) {
                owner.spawnDroppedItem(
                    item = Identifier.parse(leftover.id),
                    count = 1,
                    x = session.x,
                    y = session.y + ClientSession.THROW_EYE_HEIGHT,
                    z = session.z,
                )
            }
            if (remaining > 0 && leftover != null) owner.sendInventory()
        }
        owner.syncHealth()
    }
}
