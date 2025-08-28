package org.basaltmc.server.runtime

import org.basaltmc.server.plugins.PluginEventTypes
import org.basaltmc.server.random.LegacyRandom
import org.basaltmc.server.registry.BlockEntityTypes
import org.basaltmc.server.registry.BlockShapes
import org.basaltmc.server.registry.BlockStateDefinitions
import org.basaltmc.server.registry.BlockSupport
import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.registry.EntityTypes
import org.basaltmc.server.registry.ItemBlocks
import org.basaltmc.server.registry.ItemTools
import org.basaltmc.server.registry.Items
import org.basaltmc.server.runtime.entity.ServerEntity
import org.basaltmc.server.runtime.item.ItemDamageEffects
import org.basaltmc.server.runtime.item.ItemWear
import org.basaltmc.server.runtime.player.Menu
import org.basaltmc.server.runtime.player.PlayerInventory
import org.basaltmc.server.runtime.player.PlayerSession
import org.basaltmc.server.runtime.plugins.emit
import org.basaltmc.server.runtime.world.BedProblem
import org.basaltmc.server.runtime.world.Beds
import org.basaltmc.server.runtime.world.BlockEffect
import org.basaltmc.server.runtime.world.BlockInteractions
import org.basaltmc.server.runtime.world.BlockPlacement
import org.basaltmc.server.runtime.world.Buckets
import org.basaltmc.server.runtime.world.Campfires
import org.basaltmc.server.runtime.world.ChiseledBookshelves
import org.basaltmc.server.runtime.world.Containers
import org.basaltmc.server.runtime.world.DamageTypes
import org.basaltmc.server.runtime.world.DecoratedPots
import org.basaltmc.server.runtime.world.DoubleBlocks
import org.basaltmc.server.runtime.world.EndPortals
import org.basaltmc.server.runtime.world.Explosions
import org.basaltmc.server.runtime.world.Fires
import org.basaltmc.server.runtime.world.FluidFlow
import org.basaltmc.server.runtime.world.Fluids
import org.basaltmc.server.runtime.world.Furnaces
import org.basaltmc.server.runtime.world.Jukeboxes
import org.basaltmc.server.runtime.world.Lecterns
import org.basaltmc.server.runtime.world.LoadedWorld
import org.basaltmc.server.runtime.world.MobEffects
import org.basaltmc.server.runtime.world.MossCarpetTopper
import org.basaltmc.server.runtime.world.NeighbourUpdates
import org.basaltmc.server.runtime.world.NetherPortals
import org.basaltmc.server.runtime.world.NoteBlocks
import org.basaltmc.server.runtime.world.Rails
import org.basaltmc.server.runtime.world.RealLootTables
import org.basaltmc.server.runtime.world.Redstone
import org.basaltmc.server.runtime.world.RedstoneWire
import org.basaltmc.server.runtime.world.ScaffoldingPlacement
import org.basaltmc.server.runtime.world.Shelves
import org.basaltmc.server.runtime.world.Shulkers
import org.basaltmc.server.runtime.world.Signs
import org.basaltmc.server.runtime.world.SnowyBlocks
import org.basaltmc.server.runtime.world.Submersion
import org.basaltmc.server.runtime.world.Vibrations
import org.basaltmc.server.runtime.world.WallVariants
import org.basaltmc.server.runtime.world.WorldState
import org.basaltmc.server.storage.StoredItemStack
import org.basaltmc.server.storage.StoredSpawnPoint
import org.basaltmc.server.storage.StoredStats
import org.basaltmc.server.world.Chunk
import org.basaltmc.server.world.ChunkBlockEntity
import org.basaltmc.server.world.portal.PortalShape
import org.basaltmc.wire.common.Identifier
import org.basaltmc.wire.nbt.NbtCompound
import org.basaltmc.wire.protocol.play.AnimatePacket
import org.basaltmc.wire.protocol.play.BlockDestructionPacket
import org.basaltmc.wire.protocol.play.BlockEntityDataPacket
import org.basaltmc.wire.protocol.play.BlockEventPacket
import org.basaltmc.wire.protocol.play.BlockHitResult
import org.basaltmc.wire.protocol.play.BlockPos
import org.basaltmc.wire.protocol.play.ClientboundBlockChangedAckPacket
import org.basaltmc.wire.protocol.play.ClientboundBlockUpdatePacket
import org.basaltmc.wire.protocol.play.Direction
import org.basaltmc.wire.protocol.play.Gamemode
import org.basaltmc.wire.protocol.play.LecternMenuType
import org.basaltmc.wire.protocol.play.LevelEventPacket
import org.basaltmc.wire.protocol.play.OpenScreenPacket
import org.basaltmc.wire.protocol.play.OpenSignEditorPacket
import org.basaltmc.wire.protocol.play.ServerboundPlayerActionPacket
import org.basaltmc.wire.protocol.play.ServerboundUseItemOnPacket
import org.basaltmc.wire.protocol.play.ServerboundUseItemPacket
import org.basaltmc.wire.protocol.play.SetEntityDataPacket
import org.basaltmc.wire.protocol.play.SignUpdatePacket
import org.basaltmc.wire.protocol.play.SoundPacket
import org.basaltmc.wire.protocol.play.SoundSource
import org.basaltmc.wire.text.Component
import kotlin.math.PI
import kotlin.math.cos
import kotlin.math.floor
import kotlin.math.sin
import kotlin.math.sqrt
import kotlin.random.Random
import kotlin.uuid.Uuid

/**
 * Breaking, placing and using a block: digging, drops, signs, beds, jukeboxes, buckets, spawn eggs,
 * minecarts and boats. See [ClientSession.blockInteraction].
 *
 * A back-reference to the owning session, same as [PacketInterceptor], [ItemUseTimers] and
 * [ContainerMenuController]. Kept as one class rather than split further: [handleUseItemOn] is one
 * long ordered cascade of block interactions, and every branch in it calls a sibling in this file.
 */
internal class BlockInteractionHandler(
    private val owner: ClientSession,
) {
    /** What an enchantment takes off a durability cost, read from the definitions themselves. */
    private val damageEffects =
        ItemDamageEffects(
            definitionOf = { owner.enchantments?.definitionOf(it) },
            itemInTag = { tag, item -> owner.itemTags?.contains(tag, item) == true },
        )

    /** Where this player is digging, for how long, and which crack stage everyone else has seen. */
    private var diggingAt: BlockPos? = null

    private var diggingTicks = 0

    private var diggingStage = -1

    /**
     * Whether the block in front of this player comes apart in a single tick, tool and enchantments
     * included. parity: `ServerPlayerGameMode.handleBlockBreakAction`, which asks the same question
     * before deciding that a dig is over the moment it starts.
     */
    private fun breaksInOneTick(
        block: Identifier?,
        hardness: Blocks.BlockHardness?,
    ): Boolean {
        if (block == null || hardness == null) return false
        if (hardness.isUnbreakable) return false
        if (hardness.isInstant) return true
        return destroyProgressPerTick(block, hardness) >= 1f
    }

    /** How much of a block this player takes off per tick, tool, enchantments and effects included. */
    private fun destroyProgressPerTick(
        block: Identifier,
        hardness: Blocks.BlockHardness? = Blocks.hardnessOf(block),
    ): Float {
        if (hardness == null || hardness.isUnbreakable) return 0f
        val held = owner.playerSession?.inventory?.heldItem
        val heldItem = held?.let { runCatching { Identifier.parse(it.id) }.getOrNull() }
        return ItemTools.destroyProgressPerTick(
            item = heldItem,
            block = block,
            hardness = hardness.destroyTime,
            requiresCorrectTool = hardness.requiresCorrectTool,
            efficiencyLevel = held?.let { PlayerInventory.enchantmentLevel(it, ClientSession.EFFICIENCY) } ?: 0,
            hasteAmplifier =
                owner.playerSession
                    ?.effects
                    ?.get(MobEffects.HASTE)
                    ?.amplifier ?: -1,
            miningFatigueAmplifier =
                owner.playerSession
                    ?.effects
                    ?.get(MobEffects.MINING_FATIGUE)
                    ?.amplifier ?: -1,
            underwater = eyesInWater(),
            inAir = owner.playerSession?.onGround == false,
        ) { tag, tagged -> owner.blockTags?.contains(tag, tagged) == true }
    }

    /** parity: `Player.getDestroySpeed` asks whether the eyes are under water, not the feet. */
    private fun eyesInWater(): Boolean {
        val session = owner.playerSession ?: return false
        val eyeY = session.y + Fluids.eyeHeight(session.sneaking)
        val state = owner.world.getBlockState(floor(session.x).toInt(), floor(eyeY).toInt(), floor(session.z).toInt())
        return state != null && Fluids.of(state) == Submersion.WATER
    }

    /**
     * Breaking a block, on whichever packet the player's game mode breaks on.
     *
     * // parity: ServerPlayerGameMode. Creative destroys on `START_DESTROY_BLOCK`, survival times
     * the dig client-side and sends `STOP_DESTROY_BLOCK`; matching that split is what keeps the
     * client's prediction in step. Adventure and spectator break nothing, as in vanilla.
     *
     * Survival's timing is trusted rather than re-derived from hardness and tool, so a client that
     * finishes early is believed.
     */

    internal fun handlePlayerAction(action: ServerboundPlayerActionPacket) {
        when (action.action) {
            ServerboundPlayerActionPacket.Action.DROP_ITEM -> dropHeldItem(all = false)
            ServerboundPlayerActionPacket.Action.DROP_ALL_ITEMS -> dropHeldItem(all = true)
            // parity: letting go before the animation ends eats nothing at all.
            ServerboundPlayerActionPacket.Action.RELEASE_USE_ITEM -> {
                owner.itemUseTimers.stopEating()
                owner.itemUseTimers.releaseBow()
            }
            ServerboundPlayerActionPacket.Action.SWAP_ITEM_WITH_OFFHAND -> owner.swapWithOffhand()
            else -> Unit
        }
        // The cracks everyone else sees. The digger draws its own, which is why this never goes
        // back to them. parity: `ServerPlayerGameMode.destroyAndAck` and its `destroyProgress`.
        when (action.action) {
            ServerboundPlayerActionPacket.Action.START_DESTROY_BLOCK -> {
                // parity: `NoteBlock.attack`, which plays the note without moving it on.
                owner.currentWorld?.let { loaded ->
                    val struck = NoteBlocks.strike(loaded, action.pos.x, action.pos.y, action.pos.z)
                    if (struck.isNotEmpty()) {
                        val stateId = owner.world.getBlockState(action.pos.x, action.pos.y, action.pos.z) ?: 0
                        broadcastEffects(action.pos, stateId, struck)
                    }
                }
                startDigging(action.pos)
            }
            ServerboundPlayerActionPacket.Action.ABORT_DESTROY_BLOCK,
            ServerboundPlayerActionPacket.Action.STOP_DESTROY_BLOCK,
            -> stopDigging()
            else -> Unit
        }

        val gamemode = owner.playerSession?.gamemode ?: owner.defaultGamemode
        val targetHardness =
            owner.world
                .getBlockState(action.pos.x, action.pos.y, action.pos.z)
                ?.let(Blocks::byStateId)
                ?.let { Blocks.hardnessOf(it.block) }
        val targetBlock =
            owner.world
                .getBlockState(action.pos.x, action.pos.y, action.pos.z)
                ?.let(Blocks::byStateId)
                ?.block
        val breaksOn =
            when (gamemode) {
                Gamemode.CREATIVE -> ServerboundPlayerActionPacket.Action.START_DESTROY_BLOCK
                Gamemode.SURVIVAL ->
                    // A block that comes apart in one tick is destroyed by the client the moment the
                    // dig starts, and it never sends the end of one: waiting for `STOP` there is why
                    // flowers, grass and leaves could not be broken in survival at all, and why an
                    // efficiency pickaxe felt like a bare one.
                    if (breaksInOneTick(targetBlock, targetHardness)) {
                        ServerboundPlayerActionPacket.Action.START_DESTROY_BLOCK
                    } else {
                        ServerboundPlayerActionPacket.Action.STOP_DESTROY_BLOCK
                    }
                Gamemode.ADVENTURE, Gamemode.SPECTATOR -> null
            }
        // Bedrock and the portal frames stay put for anyone who is not in creative.
        if (gamemode == Gamemode.SURVIVAL && targetHardness?.isUnbreakable == true) {
            owner.sendCompressed(ClientboundBlockChangedAckPacket.ID) {
                ClientboundBlockChangedAckPacket(action.sequence).write(it)
            }
            return
        }
        if (action.action != breaksOn) {
            owner.sendCompressed(ClientboundBlockChangedAckPacket.ID) {
                ClientboundBlockChangedAckPacket(action.sequence).write(it)
            }
            return
        }

        val broken = owner.world.getBlockState(action.pos.x, action.pos.y, action.pos.z)
        // The question a protection plugin answers, asked where the server would have gone ahead.
        // Cancelling puts the block back on the digger's screen: the client broke it locally the
        // moment it started, so saying nothing would leave a hole only they can see.
        val breakEvent =
            owner.pluginEvents.emit(PluginEventTypes.BLOCK_BREAK) { event ->
                event.set(0, owner.playerSession?.name)
                event.set(1, owner.playerSession?.uuid?.toString())
                event.set(2, owner.currentWorld?.name)
                event.set(3, action.pos.x)
                event.set(4, action.pos.y)
                event.set(5, action.pos.z)
                event.set(6, broken?.let { BlockStateDefinitions.blockOf(it) })
            }
        if (breakEvent?.cancelled == true) {
            owner.sendCompressed(ClientboundBlockChangedAckPacket.ID) {
                ClientboundBlockChangedAckPacket(action.sequence).write(it)
            }
            owner.sendPlayPacket(ClientboundBlockUpdatePacket.ID) {
                ClientboundBlockUpdatePacket(action.pos, broken ?: 0).write(it)
            }
            breakEvent.cancelMessage?.let { message -> owner.sendPluginNotice(message) }
            stopDigging()
            return
        }
        // parity: `ShulkerBoxBlock.getDrops`, read before the block goes: breaking it takes the
        // block entity with it, and the contents have to travel in the item.
        val carried =
            owner.currentWorld
                ?.takeIf {
                    Shulkers.isShulkerBox(BlockStateDefinitions.blockOf(broken ?: 0)?.path)
                }?.let { Shulkers.contentsOf(it, action.pos.x, action.pos.y, action.pos.z) }
        val leaves = meltedIce(broken, action.pos) ?: Blocks.AIR.stateId
        val changed = owner.world.setBlockState(action.pos.x, action.pos.y, action.pos.z, leaves)
        owner.sendCompressed(ClientboundBlockChangedAckPacket.ID) {
            ClientboundBlockChangedAckPacket(action.sequence).write(it)
        }
        if (!changed) return
        val brokenBlock = broken?.let(Blocks::byStateId)
        brokenBlock?.let { owner.playerSession?.stats?.increment(StoredStats.mined(it.block.toString())) }

        for (player in owner.playersInSameWorld()) {
            player.connection.sendPlayPacket(ClientboundBlockUpdatePacket.ID) {
                ClientboundBlockUpdatePacket(action.pos, leaves).write(it)
            }
        }

        // parity: `Level.levelEvent(player, 2001, pos, state)`: the breaker predicts its own puff of
        // particles and its sound, everyone else needs telling.
        if (broken != null) {
            val effect = LevelEventPacket(LevelEventPacket.PARTICLES_DESTROY_BLOCK, action.pos, broken)
            for (other in owner.playersInSameWorld()) {
                if (other.entityId == owner.playerSession?.entityId) continue
                other.connection.sendPlayPacket(LevelEventPacket.ID) { effect.write(it) }
            }
        }
        stopDigging()
        refreshSnowUnder(action.pos)
        notifyNeighbours(action.pos)
        // parity: `GameEvent.BLOCK_DESTROY`, which a sculk sensor hears as twelve.
        heardBySculk("block_destroy", action.pos)
        // Breaking the frame puts the portal out, which is the other half of lighting one.
        owner.currentWorld?.let { loaded ->
            for (block in NetherPortals.extinguishAround(loaded, action.pos.x, action.pos.y, action.pos.z)) {
                broadcastBlockUpdate(BlockPos(block[0], block[1], block[2]), Blocks.AIR.stateId)
            }
            // Whatever fluid was being held back by that block now has somewhere to go.
            for (block in FluidFlow.onBlockChanged(loaded, action.pos.x, action.pos.y, action.pos.z)) {
                broadcastBlockUpdate(BlockPos(block[0], block[1], block[2]), block[3])
            }
        }

        if (gamemode == Gamemode.SURVIVAL && brokenBlock != null) {
            // parity: `Player.hasCorrectToolForDrops`. A block marked as needing the right tool
            // gives nothing to the wrong one, which is what makes punching stone a waste of time
            // rather than a free cobblestone.
            val heldItem =
                owner.playerSession
                    ?.inventory
                    ?.heldItem
                    ?.let { runCatching { Identifier.parse(it.id) }.getOrNull() }
            val needsTool = Blocks.hardnessOf(brokenBlock.block)?.requiresCorrectTool == true
            val correctTool =
                !needsTool ||
                    ItemTools.isCorrectForDrops(heldItem, brokenBlock.block) { tag, block ->
                        owner.blockTags?.contains(tag, block) == true
                    }
            if (correctTool) dropLootFor(brokenBlock.block, action.pos, carried)
        }
        if (brokenBlock != null) wearHeldItemOnBreak(brokenBlock.block)
    }

    /**
     * parity: `Item.mineBlock`, which wears the held item only when it has a `minecraft:tool`
     * component, the block has some hardness, and that component costs something per block. A
     * creative player wears nothing, as `ItemStack.processDurabilityChange` decides up front.
     */
    private fun wearHeldItemOnBreak(block: Identifier) {
        val session = owner.playerSession ?: return
        if (session.gamemode == Gamemode.CREATIVE) return
        if (Blocks.hardnessOf(block)?.isInstant != false) return
        val heldSlot = PlayerInventory.HOTBAR_START + session.inventory.heldSlot
        val held = session.inventory[heldSlot] ?: return
        val cost = ItemWear.costPerBlock(held)
        if (cost <= 0) return
        val worn =
            ItemWear.hurt(held, cost, LegacyRandom(Random.nextLong())) { stack, amount, random ->
                damageEffects.process(stack, amount, random)
            }
        if (worn == held) return
        val wearing =
            owner.playerEvent(PluginEventTypes.PLAYER_ITEM_DAMAGE) { event ->
                event.set(3, held.id)
                event.set(4, cost)
                event.set(5, worn == null)
            }
        if (wearing?.cancelled == true) return
        session.inventory[heldSlot] = worn
        owner.sendInventorySlot(heldSlot)
    }

    /**
     * Sets fire where the flint and steel points, and lights a Nether portal if that fire lands
     * inside an obsidian frame.
     *
     * parity: `BaseFireBlock.onPlace` is what checks the frame, not the item, which is why any
     * fire in the right place opens a portal and why the portal replaces the fire rather than
     * burning next to it.
     */
    private fun igniteAt(
        session: PlayerSession,
        pos: BlockPos,
        heldSlot: Int,
        held: StoredItemStack,
    ) {
        val world = owner.currentWorld ?: return
        val existing = world.state.getBlockState(pos.x, pos.y, pos.z) ?: return
        if (!isAirLike(existing)) return

        val shape = NetherPortals.light(world, pos.x, pos.y, pos.z)
        if (shape != null) {
            val portal = PortalShape.portalState(shape.axis)
            for (block in shape.interior()) broadcastBlockUpdate(BlockPos(block[0], block[1], block[2]), portal)
        } else {
            if (!NetherPortals.placeFire(world, pos.x, pos.y, pos.z)) return
            broadcastBlockUpdate(pos, ClientSession.FIRE_STATE)
            // parity: `BaseFireBlock.onPlace`, which is what starts fire acting at all.
            Fires.schedule(world, pos.x, pos.y, pos.z, LegacyRandom(Random.nextLong()))
        }

        if (session.gamemode == Gamemode.CREATIVE) return
        val worn =
            ItemWear.hurt(held, 1, LegacyRandom(Random.nextLong())) { stack, amount, random ->
                damageEffects.process(stack, amount, random)
            }
        if (worn == held) return
        session.inventory[heldSlot] = worn
        owner.sendInventorySlot(heldSlot)
    }

    /** Arms an end portal frame, and opens the portal when that was the twelfth eye. */
    private fun placeEnderEye(
        session: PlayerSession,
        pos: BlockPos,
        heldSlot: Int,
        held: StoredItemStack,
    ) {
        val world = owner.currentWorld ?: return
        val changed = EndPortals.placeEye(world, pos.x, pos.y, pos.z) ?: return
        for (block in changed) broadcastBlockUpdate(BlockPos(block[0], block[1], block[2]), block[3])
        if (session.gamemode == Gamemode.CREATIVE) return
        val remaining = held.count - 1
        session.inventory[heldSlot] = if (remaining <= 0) null else held.copy(count = remaining)
        owner.sendInventorySlot(heldSlot)
    }

    /**
     * What breaking a block leaves behind, which is water when it was ice over something solid or
     * liquid. parity: `IceBlock.playerDestroy`. Without it a frozen ocean answers a broken block
     * with a dry hole, and there is no way into the water at all.
     *
     * Silk touch is not read yet, so ice always melts here; the Nether evaporates it as vanilla does.
     */
    private fun meltedIce(
        broken: Int?,
        pos: BlockPos,
    ): Int? {
        if (broken == null || broken !in ClientSession.ICE_STATES) return null
        val world = owner.currentWorld ?: return null
        if (world.declaration.dimension.hasCeiling) return null
        val below = world.state.getBlockState(pos.x, pos.y - 1, pos.z) ?: return null
        return if (Chunk.HeightmapType.MOTION_BLOCKING.matches(below)) Blocks.WATER.stateId else null
    }

    /** One block change, to everyone standing in this world. */
    internal fun broadcastBlockUpdate(
        pos: BlockPos,
        stateId: Int,
    ) {
        for (player in owner.playersInSameWorld()) {
            player.connection.sendPlayPacket(ClientboundBlockUpdatePacket.ID) {
                ClientboundBlockUpdatePacket(pos, stateId).write(it)
            }
        }
    }

    /**
     * parity: `BellBlock.onHit` with `isProperHit`: a bell only swings when it is struck on a side
     * it can swing towards, and never from a hand above or below it.
     */
    private fun ringBell(hit: BlockHitResult): Boolean {
        val stateId = owner.world.getBlockState(hit.pos.x, hit.pos.y, hit.pos.z) ?: return false
        val properties = BlockStateDefinitions.propertiesOf(stateId)
        val face = hit.direction
        if (face == Direction.UP || face == Direction.DOWN) return false
        if (hit.clickY > owner.bellHitTop) return false
        val facing = properties["facing"] ?: "north"
        val sameAxis = axisOf(facing) == axisOf(face.name.lowercase())
        val proper =
            when (properties["attachment"]) {
                "floor" -> sameAxis
                "ceiling" -> true
                else -> !sameAxis
            }
        if (!proper) return false
        broadcastEffects(
            hit.pos,
            stateId,
            listOf(
                BlockEffect.Event(b0 = 1, b1 = owner.frameDirectionData(face.name.lowercase())),
                BlockEffect.Sound("block.bell.use", SoundSource.BLOCKS, volume = 2f, pitch = 1f),
            ),
        )
        return true
    }

    private fun axisOf(direction: String): String = if (direction == "north" || direction == "south") "z" else "x"

    /** The other half of a block change: what it asks everyone nearby to hear or see. */
    internal fun broadcastEffects(
        pos: BlockPos,
        stateId: Int,
        effects: List<BlockEffect>,
    ) {
        for (effect in effects) {
            when (effect) {
                is BlockEffect.Sound -> {
                    val sound =
                        SoundPacket(
                            sound = effect.name,
                            source = effect.source,
                            x = pos.x + 0.5,
                            y = pos.y + 0.5,
                            z = pos.z + 0.5,
                            volume = effect.volume,
                            pitch = effect.pitch,
                            seed = Random.nextLong(),
                        )
                    for (player in owner.playersInSameWorld()) {
                        player.connection.sendPlayPacket(SoundPacket.ID) { sound.write(it) }
                    }
                }

                is BlockEffect.Event -> {
                    val blockId = BlockSupport.blockRegistryIdOf(stateId)
                    for (player in owner.playersInSameWorld()) {
                        player.connection.sendPlayPacket(BlockEventPacket.ID) {
                            BlockEventPacket(pos, effect.b0, effect.b1, blockId).write(it)
                        }
                    }
                }

                is BlockEffect.Effect -> {
                    for (player in owner.playersInSameWorld()) {
                        player.connection.sendPlayPacket(LevelEventPacket.ID) {
                            LevelEventPacket(effect.type, pos, effect.data).write(it)
                        }
                    }
                }
            }
        }
    }

    /**
     * `Q`/ctrl+`Q`: tosses the held stack (or all of it) into the world. parity:
     * `ServerPlayerGameMode` reads this off the same `DROP_ITEM`/`DROP_ALL_ITEMS`
     * [ServerboundPlayerActionPacket] this build does. Real vanilla arcs the toss forward from eye
     * height; this build drops it straight down at the player's own feet, a documented
     * simplification (see [WorldState.DroppedItem]'s own KDoc: nothing here ticks item physics).
     */
    private fun dropHeldItem(all: Boolean) {
        val session = owner.playerSession ?: return
        if (session.gamemode == Gamemode.SPECTATOR) return
        val heldSlot = PlayerInventory.HOTBAR_START + session.inventory.heldSlot
        val held = session.inventory[heldSlot] ?: return
        val dropCount = if (all) held.count else 1
        val dropped =
            owner.playerEvent(PluginEventTypes.PLAYER_ITEM_DROP) { event ->
                event.set(3, held.id)
                event.set(4, dropCount)
            }
        if (dropped?.cancelled == true) {
            dropped.cancelMessage?.let { owner.sendPluginNotice(it) }
            // The client has already taken it out of its own hand.
            owner.sendInventorySlot(heldSlot)
            return
        }
        val remaining = held.count - dropCount
        session.inventory[heldSlot] = if (remaining <= 0) null else held.copy(count = remaining)
        owner.sendInventorySlot(heldSlot)

        // parity: `Player.drop` throws it forward from eye height along the look direction, rather
        // than dropping it on the player's own feet where they would instantly walk back into it.
        val yaw = session.yaw * PI / 180.0
        val pitch = session.pitch * PI / 180.0
        val tossAngle = Random.nextDouble() * 2 * PI
        val tossSpread = ClientSession.TOSS_SPREAD * Random.nextDouble()
        spawnDroppedItem(
            item = Identifier.parse(held.id),
            count = dropCount,
            x = session.x,
            y = session.y + ClientSession.THROW_EYE_HEIGHT,
            z = session.z,
            velocityX = -sin(yaw) * cos(pitch) * ClientSession.THROW_SPEED + cos(tossAngle) * tossSpread,
            velocityY = -sin(pitch) * ClientSession.THROW_SPEED + ClientSession.THROW_LIFT + tossLift(),
            velocityZ = cos(yaw) * cos(pitch) * ClientSession.THROW_SPEED + sin(tossAngle) * tossSpread,
        )
    }

    /** A newly placed sign: its block entity, then the editor for whoever put it there. */
    private fun openNewSign(
        pos: BlockPos,
        stateId: Int,
    ) {
        val world = owner.currentWorld ?: return
        val type = Signs.typeOf(stateId) ?: return
        val chunk = world.state.chunkIfLoaded(pos.x shr 4, pos.z shr 4) ?: return
        chunk.setBlockEntity(Signs.blockEntity(pos.x and 15, pos.y, pos.z and 15, type))
        owner.sendCompressed(OpenSignEditorPacket.ID) { OpenSignEditorPacket(pos, isFrontText = true).write(it) }
    }

    /** Reopens an existing sign, unless it has been waxed. Answers whether it did. */
    private fun editSign(pos: BlockPos): Boolean {
        val world = owner.currentWorld ?: return false
        val chunk = world.state.chunkIfLoaded(pos.x shr 4, pos.z shr 4) ?: return false
        val existing = chunk.blockEntityAt(pos.x and 15, pos.y, pos.z and 15) ?: return false
        if (Signs.isWaxed(existing.data)) return false
        owner.sendCompressed(OpenSignEditorPacket.ID) { OpenSignEditorPacket(pos, isFrontText = true).write(it) }
        return true
    }

    /**
     * What someone typed on a sign, kept and shown to everyone.
     *
     * parity: `ServerPlayer.updateSignText`, minus the checks this build has nothing to make: no
     * chat filtering, and no owner, since nothing records who placed it.
     */
    internal fun writeSign(update: SignUpdatePacket) {
        val world = owner.currentWorld ?: return
        val pos = update.pos
        val stateId = world.state.getBlockState(pos.x, pos.y, pos.z) ?: return
        val type = Signs.typeOf(stateId) ?: return
        val chunk = world.state.chunkIfLoaded(pos.x shr 4, pos.z shr 4) ?: return
        val existing = chunk.blockEntityAt(pos.x and 15, pos.y, pos.z and 15)
        if (existing != null && Signs.isWaxed(existing.data)) return
        var lines = update.lines
        val signed =
            owner.playerEvent(PluginEventTypes.PLAYER_SIGN_CHANGE) { event ->
                event.set(3, pos.x)
                event.set(4, pos.y)
                event.set(5, pos.z)
                for (line in 0 until 4) event.set(6 + line, lines.getOrNull(line).orEmpty())
            }
        if (signed != null) {
            if (signed.cancelled) {
                signed.cancelMessage?.let { owner.sendPluginNotice(it) }
                return
            }
            lines = List(4) { line -> (signed.get(6 + line) as? String).orEmpty() }
        }
        val data = Signs.withText(existing?.data ?: Signs.emptyData(), update.isFrontText, lines)
        chunk.setBlockEntity(Signs.blockEntity(pos.x and 15, pos.y, pos.z and 15, type, data))
        val typeId = BlockEntityTypes.idOf(type) ?: return
        for (player in owner.playersInSameWorld()) {
            player.connection.sendPlayPacket(BlockEntityDataPacket.ID) {
                BlockEntityDataPacket(pos, typeId, data).write(it)
            }
        }
    }

    /**
     * Sets off an explosion, drops what it broke and hurts whoever was standing in it.
     *
     * parity: `ServerExplosion`. The knockback it gives is not sent: this build has no entity
     * motion packet yet, and that is the packet audit's business rather than this one's.
     */
    private fun blowUp(
        world: LoadedWorld,
        x: Double,
        y: Double,
        z: Double,
        power: Float,
        cause: String = "explosion",
    ) {
        val stopped =
            owner.pluginEvents.emit(PluginEventTypes.BLOCK_EXPLODE) { event ->
                event.set(0, world.name)
                event.set(1, x)
                event.set(2, y)
                event.set(3, z)
                event.set(4, power.toDouble())
                event.set(5, cause)
                event.set(6, owner.playerSession?.entityId)
            }
        if (stopped?.cancelled == true) return
        val broken = Explosions.explode(world, x, y, z, power, LegacyRandom(Random.nextLong()))
        for (block in broken) {
            val at = BlockPos(block.x, block.y, block.z)
            broadcastBlockUpdate(at, Blocks.AIR.stateId)
            Blocks.byStateId(block.stateId)?.let { dropLootFor(it.block, at) }
        }
        for (block in broken) {
            NeighbourUpdates.afterChange(
                world,
                block.x,
                block.y,
                block.z,
                onReshaped = { stateId, bx, by, bz -> broadcastBlockUpdate(BlockPos(bx, by, bz), stateId) },
            ) { stateId, bx, by, bz ->
                val at = BlockPos(bx, by, bz)
                broadcastBlockUpdate(at, Blocks.AIR.stateId)
                Blocks.byStateId(stateId)?.let { dropLootFor(it.block, at) }
            }
        }
        for (player in owner.playersInSameWorld()) {
            val distance =
                kotlin.math.sqrt(
                    (player.x - x) * (player.x - x) + (player.y - y) * (player.y - y) + (player.z - z) * (player.z - z),
                )
            val damage = Explosions.damageAt(distance, power, power * 2f)
            if (damage <= 0f) continue
            player.connection.hurt(damage, "badRespawnPoint", DamageTypes.BAD_RESPAWN_POINT)
        }
    }

    /**
     * Tells the sculk sensors around [pos] that something happened there.
     *
     * parity: `Level.gameEvent`, whose whole audience in this build is the sensors: what they hear
     * is the event's own frequency, and what that lights is ordinary redstone.
     */
    private fun heardBySculk(
        event: String,
        pos: BlockPos,
    ) {
        val world = owner.currentWorld ?: return
        val changes = mutableListOf<org.basaltmc.server.runtime.world.BlockChange>()
        Vibrations.emit(world, event, pos.x + 0.5, pos.y + 0.5, pos.z + 0.5, changes)
        for (change in changes) {
            broadcastBlockUpdate(BlockPos(change.x, change.y, change.z), change.stateId)
            for (settled in Redstone.settle(world, change.x, change.y, change.z)) {
                broadcastBlockUpdate(BlockPos(settled.x, settled.y, settled.z), settled.stateId)
            }
        }
    }

    /**
     * Tells the blocks around [pos] that it changed, and clears up whatever stopped standing.
     *
     * parity: `Level.updateNeighborsAt`. What falls drops its loot with no tool involved, because
     * nobody mined it: vanilla's `dropResources` is called by the block itself.
     */
    internal fun notifyNeighbours(pos: BlockPos) {
        val world = owner.currentWorld ?: return
        val effects = mutableListOf<org.basaltmc.server.runtime.world.BlockChange>()
        NeighbourUpdates.afterChange(
            world,
            pos.x,
            pos.y,
            pos.z,
            onReshaped = { stateId, x, y, z -> broadcastBlockUpdate(BlockPos(x, y, z), stateId) },
            effects = effects,
            // parity: `updateNeighbourShapes` reshapes the neighbours and never the block itself.
            reshapeSelf = false,
        ) { stateId, x, y, z ->
            val at = BlockPos(x, y, z)
            broadcastBlockUpdate(at, Blocks.AIR.stateId)
            Blocks.byStateId(stateId)?.let { dropLootFor(it.block, at) }
        }
        for (change in effects) {
            broadcastEffects(BlockPos(change.x, change.y, change.z), change.stateId, change.effects)
        }
    }

    /**
     * Rolls [block]'s loot table (see [RealLootTables]) and drops each resulting stack, snapped to
     * the ground below [pos] rather than left floating in the hole the break just made.
     *
     * The roll itself is not RNG-parity with vanilla: a real block break consumes the world's own
     * `RandomSequences` entry (`minecraft:blocks/<path>`, `LootTable.getDefaultParamSet`'s own
     * `RandomSource`), so the exact same seed and break order would not currently reproduce
     * vanilla's drops. A documented gap, not a silent one: see [LegacyRandom]'s use below.
     */
    private fun dropLootFor(
        block: Identifier,
        pos: BlockPos,
        carried: NbtCompound? = null,
    ) {
        val table = owner.lootTables?.forBlock(block) ?: return
        val drops = table.generate(LegacyRandom(Random.nextLong()))
        if (drops.isEmpty()) return
        for (stack in drops) {
            // Falls under its own gravity now ([WorldState.tickItems]) instead of being snapped to
            // the ground up front, so a block broken in mid-air drops as it should.
            val components = NbtCompound()
            if (carried != null && stack.item == block) components[Shulkers.CONTAINER] = carried
            // parity: `Block.popResource`, which scatters the stack inside the block it came from
            // and pops it up, rather than dropping every stack on the same point.
            spawnDroppedItem(
                item = stack.item,
                count = stack.count,
                x = pos.x + 0.5 + popJitter(),
                y = pos.y + 0.5 + popJitter() - ClientSession.ITEM_HALF_HEIGHT,
                z = pos.z + 0.5 + popJitter(),
                velocityX = Random.nextDouble() * ClientSession.POP_SPREAD - ClientSession.POP_SPREAD / 2.0,
                velocityY = ClientSession.BREAK_DROP_LIFT,
                velocityZ = Random.nextDouble() * ClientSession.POP_SPREAD - ClientSession.POP_SPREAD / 2.0,
                pickupDelay = ClientSession.BREAK_DROP_PICKUP_DELAY,
                components = components,
            )
        }
    }

    internal fun isAirLike(stateId: Int): Boolean =
        stateId == Blocks.AIR.stateId || stateId == Blocks.CAVE_AIR.stateId || stateId == Blocks.VOID_AIR.stateId

    /** parity: `Block.popResource`'s own `Mth.nextDouble(random, -0.25, 0.25)`. */
    private fun popJitter(): Double = Random.nextDouble() * 2 * ClientSession.POP_JITTER - ClientSession.POP_JITTER

    /** parity: `createItemStackToDrop`, whose toss is never quite straight. */
    internal fun tossLift(): Double = (Random.nextDouble() - Random.nextDouble()) * ClientSession.TOSS_LIFT_SPREAD

    /** Puts a new item entity into the world and tells everyone who can currently see it. */
    internal fun spawnDroppedItem(
        item: Identifier,
        count: Int,
        x: Double,
        y: Double,
        z: Double,
        velocityX: Double = 0.0,
        velocityY: Double = 0.0,
        velocityZ: Double = 0.0,
        pickupDelay: Int = WorldState.DEFAULT_PICKUP_DELAY,
        components: NbtCompound = NbtCompound(),
    ) {
        if (count <= 0) return
        if (Items.REGISTRY.byIdentifier(item) == null) return
        val spawning =
            owner.pluginEvents.emit(PluginEventTypes.ITEM_SPAWN) { event ->
                event.set(0, owner.currentWorld?.name)
                event.set(1, x)
                event.set(2, y)
                event.set(3, z)
                event.set(4, item.toString())
                event.set(5, count)
                event.set(6, "drop")
            }
        if (spawning?.cancelled == true) return
        val entityId = owner.playerRegistry?.allocateEntityId() ?: return
        // Only put into the world here. Telling anyone about it is [syncItemEntities]'s job, on
        // each connection's own thread: broadcasting from here reached whoever happened to be
        // online at the instant of the drop and nobody who walked up afterwards.
        owner.world.spawnItem(
            WorldState.DroppedItem(
                entityId = entityId,
                uuid = Uuid.random(),
                item = item,
                count = count,
                x = x,
                y = y,
                z = z,
                velocityX = velocityX,
                velocityY = velocityY,
                velocityZ = velocityZ,
                pickupDelay = pickupDelay,
                components = components,
            ),
        )
    }

    /**
     * Placing a block: resolves the held item to a block state by matching identifiers (so only
     * plain blocks whose item shares the block's own name place correctly), then places it in the
     * empty space [ServerboundUseItemOnPacket.blockHit] points at.
     *
     * parity: `ServerPlayerGameMode.useItemOn`, which answers a click on a block before it
     * considers placing anything: a container opens, a lever flips, a bed is slept in. Off-hand
     * placing is a documented gap, since only the main hand is read.
     */

    internal fun handleUseItemOn(useItemOn: ServerboundUseItemOnPacket) {
        fun ack() {
            owner.sendCompressed(ClientboundBlockChangedAckPacket.ID) {
                ClientboundBlockChangedAckPacket(useItemOn.sequence).write(it)
            }
        }

        val session = owner.playerSession
        if (session == null || useItemOn.hand != ServerboundUseItemOnPacket.Hand.MAIN_HAND) return ack()
        if (session.gamemode == Gamemode.SPECTATOR || session.gamemode == Gamemode.ADVENTURE) return ack()

        val hit = useItemOn.blockHit
        // Asked before the click does anything at all: opening a chest, using a door and placing a
        // block are all this one event, which is why it carries what was clicked and what was held.
        val heldStack = session.inventory[PlayerInventory.HOTBAR_START + session.inventory.heldSlot]
        val clickedState = owner.world.getBlockState(hit.pos.x, hit.pos.y, hit.pos.z)
        val interaction =
            owner.playerEvent(PluginEventTypes.PLAYER_INTERACT) { event ->
                event.set(3, "right_click_block")
                event.set(4, hit.pos.x)
                event.set(5, hit.pos.y)
                event.set(6, hit.pos.z)
                event.set(7, clickedState?.let { BlockStateDefinitions.blockOf(it) })
                event.set(8, heldStack?.id)
                event.set(9, "main")
            }
        if (interaction?.cancelled == true) {
            interaction.cancelMessage?.let { owner.sendPluginNotice(it) }
            // The client predicted whatever it was about to do: put the block and the hand back.
            owner.sendPlayPacket(ClientboundBlockUpdatePacket.ID) {
                ClientboundBlockUpdatePacket(hit.pos, clickedState ?: 0).write(it)
            }
            owner.sendInventory()
            return ack()
        }
        // parity: `ServerPlayerGameMode.useItemOn` runs the block's own use before it considers the
        // item as something to place, and sneaking overrides that and places instead.
        if (!session.sneaking) {
            val clicked = owner.world.getBlockState(hit.pos.x, hit.pos.y, hit.pos.z)?.let(Blocks::byStateId)
            if (clicked?.block == ClientSession.CRAFTING_TABLE) {
                owner.menuController.openCraftingTable(session)
                return ack()
            }
            val cookerPath = clicked?.block?.path
            if (cookerPath != null && cookerPath in Furnaces.COOKERS) {
                owner.menuController.openFurnace(session, hit.pos, cookerPath)
                return ack()
            }
            if (cookerPath != null && cookerPath in Containers.PLAIN) {
                owner.menuController.openChest(session, hit.pos, cookerPath)
                return ack()
            }
            if (cookerPath == "crafter") {
                owner.menuController.openCrafter(session, hit.pos)
                return ack()
            }
            // parity: `JukeboxBlock.useItemOn`/`useWithoutItem`: a disc goes in, and clicking a
            // playing one takes it back out.
            if (cookerPath == "jukebox" && useJukebox(session, hit.pos)) return ack()
            // parity: `LecternBlock.useItemOn`, which takes a book, and `useWithoutItem`, which
            // opens the one already on it.
            if (cookerPath == "lectern" && useLectern(session, hit.pos)) return ack()
            // parity: `CampfireBlock.useItemOn`, which takes one piece of food onto the fire and
            // refuses anything a campfire cannot cook.
            if (cookerPath != null && Campfires.isCampfire(cookerPath) && placeOnCampfire(session, hit.pos)) {
                return ack()
            }
            // parity: `SmithingTableBlock.useWithoutItem`, whose four slots belong to the screen.
            if (cookerPath == "smithing_table") {
                owner.menuController.openSmithingMenu(session)
                return ack()
            }
            // parity: `StonecutterBlock.useWithoutItem`, whose two slots belong to the screen.
            if (cookerPath == "stonecutter") {
                owner.menuController.openStonecutterMenu(session)
                return ack()
            }
            // parity: `EnderChestBlock.useWithoutItem`, the player's own slots and not the block's.
            if (cookerPath == "ender_chest") {
                owner.menuController.openEnderChest(session, hit.pos)
                return ack()
            }
            // parity: `ChiseledBookShelfBlock`, which takes a book into the slot that was clicked
            // and gives it back from the same one.
            if (ChiseledBookshelves.isBookshelf(cookerPath)) {
                useBookshelf(session, hit)
                return ack()
            }
            // parity: `ShelfBlock.useItemOn`, one slot by hand and the whole hotbar when powered.
            if (Shelves.isShelf(cookerPath)) {
                useShelf(session, hit)
                return ack()
            }
            // parity: `DecoratedPotBlock.useItemOn`, one item at a time into its single slot, and
            // `useWithoutItem`, which only rocks it.
            if (DecoratedPots.isPot(cookerPath)) {
                usePot(session, hit.pos)
                return ack()
            }
            // parity: `NoteBlock.useWithoutItem`, which moves the note on one and plays it.
            if (cookerPath == "note_block") {
                val loaded = owner.currentWorld ?: return ack()
                NoteBlocks.tune(loaded, hit.pos.x, hit.pos.y, hit.pos.z)?.let { change ->
                    broadcastBlockUpdate(hit.pos, change.stateId)
                    broadcastEffects(hit.pos, change.stateId, change.effects)
                }
                return ack()
            }
            // parity: `BellBlock.useWithoutItem`, which only rings from a side that can swing it.
            if (cookerPath == "bell" && ringBell(hit)) return ack()
            val clickedState = owner.world.getBlockState(hit.pos.x, hit.pos.y, hit.pos.z)
            if (clickedState != null && Beds.isBed(clickedState)) {
                useBed(session, hit.pos, clickedState)
                return ack()
            }
            if (clickedState != null && BlockInteractions.isToggleable(clickedState)) {
                toggleBlock(hit.pos)
                return ack()
            }
            // parity: `RepeaterBlock.useWithoutItem` and `ComparatorBlock.useWithoutItem`, the
            // right click that moves the delay on a notch or flips the comparator's mode.
            if (clickedState != null && BlockInteractions.isCycleable(clickedState)) {
                cycleBlock(hit.pos)
                return ack()
            }
            // parity: `SignBlockEntity.executeClickCommandsIfPresent` aside, clicking a sign that is
            // not waxed opens it for editing again.
            if (clickedState != null && Signs.isSign(clickedState) && editSign(hit.pos)) return ack()
        }

        val heldSlot = PlayerInventory.HOTBAR_START + session.inventory.heldSlot
        val held = session.inventory[heldSlot]

        // Flint and steel is not a block, so it has to be answered before the "is this placeable"
        // check below throws the click away. parity: `FlintAndSteelItem.useOn`.
        if (held != null && Identifier.parse(held.id) == ClientSession.FLINT_AND_STEEL) {
            igniteAt(session, hit.pos.offset(hit.direction), heldSlot, held)
            return ack()
        }
        // parity: `EnderEyeItem.useOn`, which acts on the frame itself rather than next to it.
        if (held != null && Identifier.parse(held.id) == ClientSession.ENDER_EYE) {
            placeEnderEye(session, hit.pos, heldSlot, held)
            return ack()
        }

        val heldBlock = held?.let { Identifier.parse(it.id) }
        // parity: `ServerPlayerGameMode.useItemOn` falls through to using the item itself, which is
        // what lets anyone eat while looking at the ground rather than only at the sky.
        if (held != null &&
            heldBlock != null &&
            owner.itemUseTimers.startEating(session, heldBlock, heldSlot)
        ) {
            return ack()
        }
        // A bucket aimed at a block: the client only sends `use_item` when nothing is in the way,
        // so without this, water poured anywhere near a wall went nowhere. parity: `BucketItem.use`.
        // parity: `MinecartItem.useOn`, which only works on a rail and sits the cart on it.
        if (held != null && heldBlock != null && heldBlock.path.endsWith("minecart")) {
            if (placeMinecart(session, heldBlock, heldSlot, held, hit)) return ack()
        }
        // parity: `BoatItem.use`, which puts the boat where the player is looking.
        if (held != null && heldBlock != null && isBoat(heldBlock)) {
            if (placeBoat(session, heldBlock, heldSlot, held, hit)) return ack()
        }
        // parity: `SpawnEggItem.useOn`, which drops the mob on top of what was clicked.
        if (held != null && heldBlock != null && heldBlock.path.endsWith("_spawn_egg")) {
            if (useSpawnEgg(session, heldBlock, heldSlot, held, hit)) return ack()
        }
        if (held != null && heldBlock != null && Buckets.isBucket(heldBlock)) {
            useBucketOn(session, heldBlock, heldSlot, held, hit)
            return ack()
        }
        // parity: `StandingAndWallBlockItem` and friends: a torch, a sign, a banner or a head put
        // against a side goes down as its wall form, not as the standing one refusing to stand.
        val againstAWall =
            hit.direction != org.basaltmc.wire.protocol.play.Direction.UP &&
                hit.direction != org.basaltmc.wire.protocol.play.Direction.DOWN
        // parity: `BlockItem.getBlock`, which is not the item's own name for redstone dust, string
        // or any seed.
        val placedBlock =
            heldBlock?.let { ItemBlocks.of(it) }?.let { if (againstAWall) WallVariants.of(it) ?: it else it }
        val defaultStateId = placedBlock?.let { Blocks.REGISTRY.idOf(it) }
        if (held == null || placedBlock == null || defaultStateId == null) return ack()

        // parity: `BlockPlaceContext`: the clicked block is replaced when it is one of the things
        // that give way (grass, a fern, a snow layer, fire, a fluid), and the block goes next to it
        // otherwise. A flower, a firefly bush and leaves are not replaceable, so those get built on.
        val clickedForPlacing = owner.world.getBlockState(hit.pos.x, hit.pos.y, hit.pos.z)
        val replaceClicked =
            clickedForPlacing != null &&
                BlockShapes.isReplaceable(clickedForPlacing) &&
                BlockStateDefinitions.blockOf(clickedForPlacing) != heldBlock
        // parity: `ScaffoldingBlockItem.updatePlacementContext`, the one placement rule that lives in
        // the item: clicking a scaffolding builds up the tower instead of hanging one off the face.
        val placementWorld = owner.currentWorld
        val scaffoldTarget =
            if (heldBlock.path == "scaffolding" && placementWorld != null) {
                ScaffoldingPlacement.target(
                    placementWorld,
                    hit.pos.x,
                    hit.pos.y,
                    hit.pos.z,
                    hit.direction,
                    session.yaw,
                    session.sneaking,
                    hit.inside,
                )
            } else {
                null
            }
        val placeAt =
            when {
                scaffoldTarget != null -> BlockPos(scaffoldTarget.first, scaffoldTarget.second, scaffoldTarget.third)
                replaceClicked -> hit.pos
                else -> hit.pos.offset(hit.direction)
            }
        val existing = owner.world.getBlockState(placeAt.x, placeAt.y, placeAt.z)
        if (existing == null || !BlockShapes.isReplaceable(existing)) return ack()

        val placedStateId =
            BlockPlacement.stateFor(
                block = placedBlock,
                defaultStateId = defaultStateId,
                face = hit.direction,
                clickY = hit.clickY,
                yaw = session.yaw,
                pitch = session.pitch,
                replacing = existing,
                world = owner.currentWorld,
                x = placeAt.x,
                y = placeAt.y,
                z = placeAt.z,
                clickX = hit.clickX,
                clickZ = hit.clickZ,
            )
        // The half a two-block one starts as, whatever the state table's first entry happens to be.
        val blockStateId = DoubleBlocks.firstHalf(placedStateId)
        // parity: `BlockItem.canPlace`: a block never goes down inside somebody. Without this you
        // can wall yourself in, which is the one thing vanilla will not let you do by accident.
        if (BlockPlacement.isObstructed(blockStateId, placeAt.x, placeAt.y, placeAt.z, buildersInTheWay())) {
            return ack()
        }
        // parity: `BlockItem.canPlace` also asks `canSurvive`, which is what refuses a torch hung
        // on nothing rather than letting it be placed and drop on the next update.
        val loadedWorld = owner.currentWorld
        if (loadedWorld != null &&
            !NeighbourUpdates.isSupported(loadedWorld, placeAt.x, placeAt.y, placeAt.z, blockStateId)
        ) {
            return ack()
        }
        // parity: `setPlacedBy` on a door, a bed or a tall flower: the client asks for one block and
        // the game puts down two. Without the second one the first has no partner and `updateShape`
        // takes it straight back out, which is what made beds and doors impossible to place.
        val second = DoubleBlocks.secondHalf(blockStateId, placeAt.x, placeAt.y, placeAt.z)
        if (second != null) {
            val room = owner.world.getBlockState(second.x, second.y, second.z)
            if (room == null || !BlockShapes.isReplaceable(room)) return ack()
        }
        // Asked once the server has decided the block would go down: a plugin that says no is
        // answering the same question the game just answered yes to, and the client is told to put
        // its own prediction back.
        val placeEvent =
            owner.pluginEvents.emit(PluginEventTypes.BLOCK_PLACE) { event ->
                event.set(0, owner.playerSession?.name)
                event.set(1, owner.playerSession?.uuid?.toString())
                event.set(2, owner.currentWorld?.name)
                event.set(3, placeAt.x)
                event.set(4, placeAt.y)
                event.set(5, placeAt.z)
                event.set(6, BlockStateDefinitions.blockOf(blockStateId))
            }
        if (placeEvent?.cancelled == true) {
            ack()
            owner.sendPlayPacket(ClientboundBlockUpdatePacket.ID) {
                ClientboundBlockUpdatePacket(placeAt, owner.world.getBlockState(placeAt.x, placeAt.y, placeAt.z) ?: 0)
                    .write(it)
            }
            // The client took the item out of its own hand when it predicted the placement.
            owner.sendInventory()
            placeEvent.cancelMessage?.let { message -> owner.sendPluginNotice(message) }
            return
        }
        val changed = owner.world.setBlockState(placeAt.x, placeAt.y, placeAt.z, blockStateId)
        ack()
        if (!changed) return
        // parity: `getStateForPlacement` joins a fence, a wall or a staircase to what is already
        // there, and the announcement below has to carry the state it settled on rather than the one
        // it was placed as.
        var settledStateId = blockStateId
        loadedWorld?.let { placedIn ->
            NeighbourUpdates.connect(placedIn, placeAt.x, placeAt.y, placeAt.z, blockStateId)?.let {
                settledStateId = it
            }
            // parity: `BaseRailBlock.onPlace` into `RailState.place`, which bends the new rail and
            // every rail it decided to meet.
            Rails.place(placedIn, placeAt.x, placeAt.y, placeAt.z) { x, y, z, stateId ->
                if (x == placeAt.x && y == placeAt.y && z == placeAt.z) {
                    settledStateId = stateId
                } else {
                    broadcastBlockUpdate(BlockPos(x, y, z), stateId)
                }
            }
            // parity: `RedStoneWireBlock.getStateForPlacement`, which draws the dust as it lands.
            RedstoneWire.reshape(placedIn, placeAt.x, placeAt.y, placeAt.z, settledStateId)?.let {
                settledStateId = it
            }
        }
        second?.let { owner.world.setBlockState(it.x, it.y, it.z, it.stateId) }
        // parity: `AbstractFurnaceBlock.newBlockEntity`: a cooker is nothing without the entity that
        // holds its slots and its fire.
        val placedPath = BlockStateDefinitions.blockOf(blockStateId)?.path
        // parity: `Level.setBlock` gives a block its block entity as it lands. Every block that owns
        // one needs it, not the handful of containers this used to list: a bell without one is a
        // bell whose body the client never draws.
        if (placedBlock.let { BlockEntityTypes.ofBlock(it) } != null) {
            loadedWorld?.state?.chunkIfLoaded(placeAt.x shr 4, placeAt.z shr 4)?.setBlockEntity(
                ChunkBlockEntity(placeAt.x and 15, placeAt.y, placeAt.z and 15, placedBlock, NbtCompound()),
            )
        }
        // parity: `BlockItem.updateCustomBlockEntityTag`: a shulker box placed from an item that
        // carries contents gets them back, which is the other half of breaking one.
        if (Shulkers.isShulkerBox(placedPath)) {
            (held.components[Shulkers.CONTAINER] as? NbtCompound)?.let { carried ->
                loadedWorld?.let { Shulkers.restore(it, placeAt.x, placeAt.y, placeAt.z, carried) }
            }
        }
        // parity: `MossyCarpetBlock.setPlacedBy`, which grows a second carpet over the one placed.
        if (BlockStateDefinitions.blockOf(blockStateId)?.path == "pale_moss_carpet") {
            loadedWorld?.let { placedIn ->
                MossCarpetTopper
                    .above(placedIn, placeAt.x, placeAt.y, placeAt.z) { Random.nextBoolean() }
                    ?.let { topper -> owner.world.setBlockState(placeAt.x, placeAt.y + 1, placeAt.z, topper) }
            }
        }

        if (session.gamemode == Gamemode.SURVIVAL) {
            val remaining = held.count - 1
            session.inventory[heldSlot] = if (remaining <= 0) null else held.copy(count = remaining)
            owner.sendInventorySlot(heldSlot)
        }

        for (player in owner.playersInSameWorld()) {
            player.connection.sendPlayPacket(ClientboundBlockUpdatePacket.ID) {
                ClientboundBlockUpdatePacket(placeAt, settledStateId).write(it)
            }
        }
        second?.let { broadcastBlockUpdate(BlockPos(it.x, it.y, it.z), it.stateId) }
        // Both the block under it (a snow layer put on grass) and the block itself (grass put
        // under snow) can change how they look.
        refreshSnowUnder(placeAt)
        refreshSnowAt(placeAt)
        notifyNeighbours(placeAt)
        // parity: `GameEvent.BLOCK_PLACE`, thirteen: a block going down is louder than one coming up.
        heardBySculk("block_place", placeAt)
        if (Signs.isSign(blockStateId)) openNewSign(placeAt, blockStateId)
        owner.currentWorld?.let { loaded ->
            for (block in FluidFlow.onBlockChanged(loaded, placeAt.x, placeAt.y, placeAt.z)) {
                broadcastBlockUpdate(BlockPos(block[0], block[1], block[2]), block[3])
            }
        }
    }

    /**
     * Using an item with nothing under the crosshair, which is how a bucket reaches a fluid.
     *
     * The ack comes first and unconditionally: the client already changed its own copy, and the ack
     * is what undoes it when the server disagrees.
     */
    internal fun handleUseItem(useItem: ServerboundUseItemPacket) {
        owner.sendCompressed(ClientboundBlockChangedAckPacket.ID) {
            ClientboundBlockChangedAckPacket(useItem.sequence).write(it)
        }
        val session = owner.playerSession ?: return
        if (useItem.hand != ServerboundUseItemOnPacket.Hand.MAIN_HAND) return
        if (session.gamemode == Gamemode.SPECTATOR || session.gamemode == Gamemode.ADVENTURE) return
        val world = owner.currentWorld ?: return
        val heldSlot = PlayerInventory.HOTBAR_START + session.inventory.heldSlot
        val held = session.inventory[heldSlot] ?: return
        val item = Identifier.parse(held.id)
        // parity: `ItemCooldowns.isOnCooldown`, which refuses the use outright rather than
        // letting it through and greying the slot afterwards.
        if (owner.itemUseTimers.onCooldown(item)) return
        if (owner.itemUseTimers.startEating(session, item, heldSlot)) return
        if (owner.itemUseTimers.startDrawing(session, item, heldSlot)) return
        if (owner.itemUseTimers.throwEnderPearl(session, item, heldSlot)) return
        if (!Buckets.isBucket(item)) return

        val bucketing =
            owner.playerEvent(PluginEventTypes.PLAYER_BUCKET) { event ->
                event.set(3, session.x.toInt())
                event.set(4, session.y.toInt())
                event.set(5, session.z.toInt())
                event.set(6, item.toString())
                event.set(7, item.path == "bucket")
            }
        if (bucketing?.cancelled == true) {
            bucketing.cancelMessage?.let { owner.sendPluginNotice(it) }
            owner.sendInventory()
            return
        }
        val result =
            Buckets.use(
                world = world,
                item = item,
                eyeX = session.x,
                eyeY = session.y + Fluids.eyeHeight(session.sneaking),
                eyeZ = session.z,
                yaw = useItem.yaw,
                pitch = useItem.pitch,
                creative = session.gamemode == Gamemode.CREATIVE,
            ) ?: return

        for (block in result.changes) broadcastBlockUpdate(BlockPos(block[0], block[1], block[2]), block[3])
        // parity: `BucketItem.emptyContents` calls `setBlock`, so what was around it hears about it:
        // grass that had a snow layer over it until the water took its place stops wearing snow.
        for (block in result.changes) notifyNeighbours(BlockPos(block[0], block[1], block[2]))
        val replacement = result.replacement ?: return
        session.inventory[heldSlot] = held.copy(id = replacement.toString(), count = 1)
        owner.sendInventorySlot(heldSlot)
    }

    /**
     * A click on a lectern: put the held book on it, or open the one already there.
     *
     * parity: `LecternBlock.tryPlaceBook` and its menu. Turning a page pulses the block for two
     * ticks, which is the whole reason a lectern is a redstone component at all.
     */
    private fun useLectern(
        session: PlayerSession,
        pos: BlockPos,
    ): Boolean {
        val world = owner.currentWorld ?: return false
        val stateId = world.state.getBlockState(pos.x, pos.y, pos.z) ?: return false
        val hasBook = BlockStateDefinitions.propertiesOf(stateId)["has_book"] == "true"
        if (hasBook) {
            openLectern(session, pos)
            return true
        }
        val heldSlot = PlayerInventory.HOTBAR_START + session.inventory.heldSlot
        val held = session.inventory[heldSlot] ?: return false
        val item = Identifier.parse(held.id)
        if (!Lecterns.isBook(item)) return false
        val change = Lecterns.placeBook(world, pos.x, pos.y, pos.z, item) ?: return false
        broadcastBlockUpdate(BlockPos(change.x, change.y, change.z), change.stateId)
        session.inventory[heldSlot] = if (held.count <= 1) null else held.copy(count = held.count - 1)
        owner.sendInventorySlot(heldSlot)
        notifyNeighbours(pos)
        return true
    }

    /** parity: `LecternMenu`, one book slot and the page it is open at. */
    private fun openLectern(
        session: PlayerSession,
        pos: BlockPos,
    ) {
        owner.returnCraftingGrid(session)
        owner.nextContainerId =
            if (owner.nextContainerId >=
                ClientSession.MAX_CONTAINER_ID
            ) {
                ClientSession.FIRST_BLOCK_CONTAINER_ID
            } else {
                owner.nextContainerId + 1
            }
        owner.openMenu = Menu.playerInventory()
        owner.openBlock = pos
        owner.menuController.openLecternAt = pos
        owner.sendCompressed(OpenScreenPacket.ID) {
            OpenScreenPacket(
                containerId = owner.nextContainerId,
                menuType = LecternMenuType.ID,
                title = Component.translatable("block.minecraft.lectern"),
            ).write(it)
        }
    }

    /**
     * A click on a jukebox: put the held disc in, or take out what is playing.
     *
     * parity: `JukeboxBlock`, whose whole interaction is those two. What a comparator then reads is
     * the song's own number, and while it plays the block itself gives off a full signal.
     */
    private fun useJukebox(
        session: PlayerSession,
        pos: BlockPos,
    ): Boolean {
        val world = owner.currentWorld ?: return false
        val ejected = Jukeboxes.eject(world, pos.x, pos.y, pos.z)
        if (ejected != null) {
            val (change, disc) = ejected
            broadcastBlockUpdate(BlockPos(change.x, change.y, change.z), change.stateId)
            spawnDroppedItem(disc, 1, pos.x + 0.5, pos.y + 1.0, pos.z + 0.5)
            notifyNeighbours(pos)
            return true
        }
        val heldSlot = PlayerInventory.HOTBAR_START + session.inventory.heldSlot
        val held = session.inventory[heldSlot] ?: return false
        val item = Identifier.parse(held.id)
        if (!Jukeboxes.isDisc(item)) return false
        val song = Jukeboxes.songOf(item)
        val change =
            Jukeboxes.insert(world, pos.x, pos.y, pos.z, item, owner.jukeboxes?.comparatorOutputOf(song) ?: 0)
                ?: return false
        broadcastBlockUpdate(BlockPos(change.x, change.y, change.z), change.stateId)
        if (session.gamemode != Gamemode.CREATIVE) {
            session.inventory[heldSlot] = if (held.count <= 1) null else held.copy(count = held.count - 1)
            owner.sendInventorySlot(heldSlot)
        }
        notifyNeighbours(pos)
        return true
    }

    /**
     * parity: `MinecartItem.useOn`: a cart only goes down on a rail, a sixteenth of a block above
     * it, and half a block higher again when the rail is a slope.
     */
    private fun placeMinecart(
        session: PlayerSession,
        item: Identifier,
        heldSlot: Int,
        held: StoredItemStack,
        hit: BlockHitResult,
    ): Boolean {
        val world = owner.currentWorld ?: return false
        val rail = world.state.getBlockState(hit.pos.x, hit.pos.y, hit.pos.z) ?: return false
        if (!Rails.isRail(rail)) return false
        val shape = BlockStateDefinitions.propertiesOf(rail)["shape"] ?: return false
        val slope = if (shape.startsWith("ascending")) 0.5 else 0.0
        val type =
            if (item.path ==
                "minecart"
            ) {
                item
            } else {
                Identifier.vanilla(item.path.removeSuffix("_minecart") + "_minecart")
            }
        return spawnFromItem(
            session,
            type,
            heldSlot,
            held,
            hit.pos.x + 0.5,
            hit.pos.y + owner.minecartRestHeight + slope,
            hit.pos.z + 0.5,
        )
    }

    /** parity: `BoatItem.use`, which drops the boat on what the player is looking at. */
    private fun placeBoat(
        session: PlayerSession,
        item: Identifier,
        heldSlot: Int,
        held: StoredItemStack,
        hit: BlockHitResult,
    ): Boolean {
        // parity: `getBoat` puts it at the hit itself, which is the face of the block clicked.
        val at = hit.pos.offset(hit.direction)
        return spawnFromItem(session, boatEntity(item), heldSlot, held, at.x + 0.5, at.y.toDouble(), at.z + 0.5)
    }

    /** parity: the boat items, whose entity is the item without its `_boat`/`_chest_boat` tail. */
    private fun isBoat(item: Identifier): Boolean =
        item.path.endsWith("_boat") || item.path == "bamboo_raft" || item.path == "bamboo_chest_raft"

    private fun boatEntity(item: Identifier): Identifier = item

    private fun spawnFromItem(
        session: PlayerSession,
        type: Identifier,
        heldSlot: Int,
        held: StoredItemStack,
        x: Double,
        y: Double,
        z: Double,
    ): Boolean {
        val world = owner.currentWorld ?: return false
        val entityType = EntityTypes[type] ?: return false
        val entityId = owner.playerRegistry?.allocateEntityId() ?: return false
        world.state.spawnEntity(
            ServerEntity(
                entityId = entityId,
                uuid = Uuid.random(),
                type = type,
                protocolType = entityType.protocolId,
                x = x,
                y = y,
                z = z,
            ),
        )
        if (session.gamemode != Gamemode.CREATIVE) {
            session.inventory[heldSlot] = if (held.count <= 1) null else held.copy(count = held.count - 1)
            owner.sendInventorySlot(heldSlot)
        }
        return true
    }

    /**
     * parity: `SpawnEggItem.useOn`: the mob lands on the face that was clicked, unless the block
     * itself has no collision, in which case it lands inside it.
     */
    private fun useSpawnEgg(
        session: PlayerSession,
        egg: Identifier,
        heldSlot: Int,
        held: StoredItemStack,
        hit: BlockHitResult,
    ): Boolean {
        val world = owner.currentWorld ?: return false
        val type = Identifier.vanilla(egg.path.removeSuffix("_spawn_egg"))
        val entityType = EntityTypes[type] ?: return false
        val clicked = world.state.getBlockState(hit.pos.x, hit.pos.y, hit.pos.z) ?: return false
        val at =
            if (BlockShapes.isEmptyShape(clicked)) hit.pos else hit.pos.offset(hit.direction)
        val entityId = owner.playerRegistry?.allocateEntityId() ?: return false
        world.state.spawnEntity(
            ServerEntity(
                entityId = entityId,
                uuid = Uuid.random(),
                type = type,
                protocolType = entityType.protocolId,
                x = at.x + 0.5,
                // parity: `EntityType.spawn` puts it on top of the block, not inside it.
                y = at.y.toDouble(),
                z = at.z + 0.5,
            ),
        )
        if (session.gamemode != Gamemode.CREATIVE) {
            session.inventory[heldSlot] = if (held.count <= 1) null else held.copy(count = held.count - 1)
            owner.sendInventorySlot(heldSlot)
        }
        return true
    }

    /**
     * parity: `ChiseledBookShelfBlock.useItemOn` and `useWithoutItem`: a book goes into the slot the
     * click landed on, and an empty hand takes back whatever is in that one.
     */
    private fun useBookshelf(
        session: PlayerSession,
        hit: BlockHitResult,
    ) {
        val world = owner.currentWorld ?: return
        val stateId = world.state.getBlockState(hit.pos.x, hit.pos.y, hit.pos.z) ?: return
        val slot =
            ChiseledBookshelves.slotAt(stateId, hit.direction.name.lowercase(), hit.clickX, hit.clickY, hit.clickZ)
                ?: return
        val heldSlot = PlayerInventory.HOTBAR_START + session.inventory.heldSlot
        val held = session.inventory[heldSlot]
        val book = held?.let { Identifier.parse(it.id) }
        val isBook = book != null && owner.itemTags?.contains("minecraft:bookshelf_books", book) == true
        val interaction =
            if (isBook) {
                ChiseledBookshelves.putBook(world, hit.pos.x, hit.pos.y, hit.pos.z, slot, checkNotNull(book))
            } else {
                ChiseledBookshelves.takeBook(world, hit.pos.x, hit.pos.y, hit.pos.z, slot)
            } ?: return
        broadcastBlockUpdate(hit.pos, interaction.change.stateId)
        broadcastEffects(hit.pos, interaction.change.stateId, interaction.change.effects)
        val taken = interaction.taken
        if (taken != null) {
            giveOrDrop(session, StoredItemStack(taken.toString(), 1), hit.pos)
        } else if (held != null && session.gamemode != Gamemode.CREATIVE) {
            session.inventory[heldSlot] = if (held.count <= 1) null else held.copy(count = held.count - 1)
            owner.sendInventorySlot(heldSlot)
        }
    }

    /**
     * parity: `ShelfBlock.useItemOn`: unpowered it swaps the held item with the slot that was
     * clicked, and powered it swaps the whole hotbar against the shelves in the chain.
     */
    private fun useShelf(
        session: PlayerSession,
        hit: BlockHitResult,
    ) {
        val world = owner.currentWorld ?: return
        val stateId = world.state.getBlockState(hit.pos.x, hit.pos.y, hit.pos.z) ?: return
        val slot = Shelves.slotAt(stateId, hit.direction.name.lowercase(), hit.clickX, hit.clickY, hit.clickZ) ?: return
        if (BlockStateDefinitions.propertiesOf(stateId)["powered"] == "true") {
            swapHotbarWithShelf(session, hit.pos, stateId)
            return
        }
        val heldSlot = PlayerInventory.HOTBAR_START + session.inventory.heldSlot
        val held = session.inventory[heldSlot]
        val placed =
            held?.let {
                org.basaltmc.server.world.loot
                    .ItemStack(Identifier.parse(it.id), it.count)
            }
        val result = Shelves.swap(world, hit.pos.x, hit.pos.y, hit.pos.z, slot, placed) ?: return
        session.inventory[heldSlot] =
            result.removed?.let { StoredItemStack(it.item.toString(), it.count) }
        owner.sendInventorySlot(heldSlot)
        broadcastEffects(hit.pos, stateId, result.effects)
    }

    /** parity: `ShelfBlock.swapHotbar`, nine slots against the chain's nine. */
    private fun swapHotbarWithShelf(
        session: PlayerSession,
        pos: BlockPos,
        stateId: Int,
    ) {
        val world = owner.currentWorld ?: return
        val hotbar =
            Array(PlayerInventory.HOTBAR_SIZE) { index ->
                session.inventory[PlayerInventory.HOTBAR_START + index]?.let {
                    org.basaltmc.server.world.loot
                        .ItemStack(Identifier.parse(it.id), it.count)
                }
            }
        if (!Shelves.swapHotbar(world, pos.x, pos.y, pos.z, hotbar)) return
        for (index in hotbar.indices) {
            session.inventory[PlayerInventory.HOTBAR_START + index] =
                hotbar[index]?.let { StoredItemStack(it.item.toString(), it.count) }
        }
        owner.sendInventoryAll()
        broadcastEffects(
            pos,
            stateId,
            listOf(BlockEffect.Sound("block.shelf.multi_swap", SoundSource.BLOCKS, 1f, 1f)),
        )
    }

    /** Into the player's inventory if it fits, and onto the floor if it does not. */
    private fun giveOrDrop(
        session: PlayerSession,
        stack: StoredItemStack,
        pos: BlockPos,
    ) {
        val leftover = session.inventory.add(stack)
        owner.sendInventoryAll()
        if (leftover <= 0) return
        spawnDroppedItem(
            item = Identifier.parse(stack.id),
            count = leftover,
            x = pos.x + 0.5,
            y = pos.y + 0.5,
            z = pos.z + 0.5,
        )
    }

    /**
     * parity: `DecoratedPotBlock.useItemOn` and `useWithoutItem`. An empty hand, a full pot or the
     * wrong item all end the same way: the pot rocks and takes nothing.
     */
    private fun usePot(
        session: PlayerSession,
        pos: BlockPos,
    ) {
        val world = owner.currentWorld ?: return
        val stateId = world.state.getBlockState(pos.x, pos.y, pos.z) ?: return
        val heldSlot = PlayerInventory.HOTBAR_START + session.inventory.heldSlot
        val held = session.inventory[heldSlot]
        val inserted =
            held?.let { DecoratedPots.insert(world, pos.x, pos.y, pos.z, Identifier.parse(it.id)) }
        if (inserted == null) {
            broadcastEffects(pos, stateId, DecoratedPots.rebuff())
            return
        }
        if (session.gamemode != Gamemode.CREATIVE) {
            session.inventory[heldSlot] = if (held.count <= 1) null else held.copy(count = held.count - 1)
            owner.sendInventorySlot(heldSlot)
        }
        broadcastEffects(pos, stateId, inserted)
    }

    /**
     * Puts what the player is holding onto a campfire.
     *
     * parity: `CampfireBlock.useItemOn`: one piece into the first free slot, at the recipe's own
     * cooking time, and nothing at all if the campfire cannot cook it.
     */
    private fun placeOnCampfire(
        session: PlayerSession,
        pos: BlockPos,
    ): Boolean {
        val world = owner.currentWorld ?: return false
        val stateId = world.state.getBlockState(pos.x, pos.y, pos.z) ?: return false
        if (!Campfires.isLitCampfire(stateId)) return false
        val heldSlot = PlayerInventory.HOTBAR_START + session.inventory.heldSlot
        val held = session.inventory[heldSlot] ?: return false
        val chunk = world.state.chunkIfLoaded(pos.x shr 4, pos.z shr 4) ?: return false
        val entity =
            chunk.blockEntityAt(pos.x and 15, pos.y, pos.z and 15)
                ?: ChunkBlockEntity(pos.x and 15, pos.y, pos.z and 15, Campfires.BLOCK, NbtCompound())
                    .also { chunk.setBlockEntity(it) }
        val state = Campfires.read(entity.data)
        val stack =
            org.basaltmc.server.world.loot
                .ItemStack(Identifier.parse(held.id), 1)
        if (!Campfires.placeFood(
                state,
                stack,
            ) { food -> owner.recipes?.cookingFor(owner.campfireCooker, food) }
        ) {
            return false
        }
        Campfires.write(state, entity.data)
        chunk.markDirty()
        if (session.gamemode != Gamemode.CREATIVE) {
            session.inventory[heldSlot] = if (held.count <= 1) null else held.copy(count = held.count - 1)
            owner.sendInventorySlot(heldSlot)
        }
        return true
    }

    /**
     * Takes the snow off the block under [pos], or puts it on. parity: `SnowyBlock.updateShape`,
     * which vanilla runs through a block update and this build runs where the block changed.
     */
    private fun refreshSnowUnder(pos: BlockPos) = refreshSnowAt(BlockPos(pos.x, pos.y - 1, pos.z))

    private fun refreshSnowAt(pos: BlockPos) {
        val world = owner.currentWorld ?: return
        val updated = SnowyBlocks.updated(world, pos.x, pos.y, pos.z) ?: return
        if (!world.state.setBlockState(pos.x, pos.y, pos.z, updated)) return
        broadcastBlockUpdate(pos, updated)
    }

    /**
     * The arm swing everyone else sees. parity: `ServerGamePacketListenerImpl.handleAnimate`, which
     * forwards it as `ClientboundAnimatePacket` to whoever is tracking that player.
     *
     * The swinging client animates its own arm, which is why this goes to the others and not back.
     */
    internal fun handleSwing(hand: Int) {
        val session = owner.playerSession ?: return
        val action = if (hand == ClientSession.OFFHAND) AnimatePacket.SWING_OFFHAND else AnimatePacket.SWING_MAIN_HAND
        val packet = AnimatePacket(session.entityId, action)
        for (other in owner.playersInSameWorld()) {
            if (other.entityId == session.entityId) continue
            other.connection.sendPlayPacket(AnimatePacket.ID) { packet.write(it) }
        }
    }

    /** Starts drawing cracks on [pos] for everyone else, at the speed this player actually mines. */
    private fun startDigging(pos: BlockPos) {
        val session = owner.playerSession ?: return
        if (session.gamemode != Gamemode.SURVIVAL && session.gamemode != Gamemode.ADVENTURE) return
        val digging =
            owner.playerEvent(PluginEventTypes.BLOCK_DAMAGE) { event ->
                event.set(3, pos.x)
                event.set(4, pos.y)
                event.set(5, pos.z)
                event.set(6, owner.world.getBlockState(pos.x, pos.y, pos.z)?.let { BlockStateDefinitions.blockOf(it) })
            }
        if (digging?.cancelled == true) {
            digging.cancelMessage?.let { owner.sendPluginNotice(it) }
            return
        }
        diggingAt = pos
        diggingTicks = 0
        diggingStage = -1
    }

    private fun stopDigging() {
        val pos = diggingAt ?: return
        diggingAt = null
        diggingStage = -1
        broadcastDestruction(pos, BlockDestructionPacket.NONE)
    }

    /**
     * One tick of the dig: works out how far along it is and, when the stage changes, tells everyone.
     *
     * The progress comes from the same [ItemTools.destroyProgressPerTick] the break itself uses, so
     * the cracks run at the speed the tool really mines at rather than a guess.
     */
    internal fun tickDigging(session: PlayerSession) {
        val pos = diggingAt ?: return
        val state = owner.currentWorld?.state?.getBlockState(pos.x, pos.y, pos.z)
        val block = state?.let(Blocks::byStateId)?.block
        if (block == null || Blocks.isAir(state)) {
            stopDigging()
            return
        }
        diggingTicks++
        val perTick = destroyProgressPerTick(block)
        if (perTick <= 0f) return
        val stage = ((diggingTicks * perTick) * BlockDestructionPacket.STAGES).toInt()
        if (stage >= BlockDestructionPacket.STAGES || stage == diggingStage) return
        diggingStage = stage
        broadcastDestruction(pos, stage)
    }

    private fun broadcastDestruction(
        pos: BlockPos,
        stage: Int,
    ) {
        val session = owner.playerSession ?: return
        val packet = BlockDestructionPacket(session.entityId, pos, stage)
        for (other in owner.playersInSameWorld()) {
            if (other.entityId == session.entityId) continue
            other.connection.sendPlayPacket(BlockDestructionPacket.ID) { packet.write(it) }
        }
    }

    /** parity: `BucketItem.use` with the block the client says it hit, rather than a second raycast. */
    private fun useBucketOn(
        session: PlayerSession,
        item: Identifier,
        slot: Int,
        held: StoredItemStack,
        hit: BlockHitResult,
    ) {
        val world = owner.currentWorld ?: return
        val result =
            Buckets.useOn(
                world = world,
                item = item,
                x = hit.pos.x,
                y = hit.pos.y,
                z = hit.pos.z,
                face = hit.direction,
                creative = session.gamemode == Gamemode.CREATIVE,
            ) ?: return
        for (block in result.changes) broadcastBlockUpdate(BlockPos(block[0], block[1], block[2]), block[3])
        val replacement = result.replacement ?: return
        session.inventory[slot] = held.copy(id = replacement.toString(), count = 1)
        owner.sendInventorySlot(slot)
    }

    /**
     * The boxes of everything in this world that stops a block being placed inside it.
     *
     * parity: `Entity.blocksBuilding`, which is on for anything alive and off for dropped items and
     * arrows, plus spectators, who are built straight through.
     */
    private fun buildersInTheWay(): List<DoubleArray> {
        val session = owner.playerSession ?: return emptyList()
        val players = owner.playersInSameWorld().toMutableList()
        if (players.none { it.entityId == session.entityId }) players += session
        return players
            .filter { it.gamemode != Gamemode.SPECTATOR }
            .map { BlockPlacement.playerBox(it.x, it.y, it.z, it.sneaking) }
    }

    /** Opening a door, flipping a lever: the change goes to everyone, and so does the sound. */
    private fun toggleBlock(pos: BlockPos) {
        val world = owner.currentWorld ?: return
        val before = world.state.getBlockState(pos.x, pos.y, pos.z)
        val toggled = BlockInteractions.toggle(world, pos.x, pos.y, pos.z) ?: return
        // parity: `ButtonBlock.press` schedules its own release. Everything else here stays put
        // until something touches it again.
        BlockInteractions.pressedTicks(before ?: 0)?.let { delay ->
            world.state.scheduleBlockTick(pos.x, pos.y, pos.z, ClientSession.BUTTON_TICK, delay)
        }
        for (block in toggled.changes) broadcastBlockUpdate(BlockPos(block[0], block[1], block[2]), block[3])
        // parity: `LeverBlock.updateNeighbours` and `ButtonBlock.updateNeighbours`. Without this the
        // lever moved and powered nothing at all, which is most of what a lever is for.
        for (block in toggled.changes) notifyNeighbours(BlockPos(block[0], block[1], block[2]))
        val sound =
            SoundPacket(
                sound = toggled.sound,
                source = SoundSource.BLOCKS,
                x = pos.x + 0.5,
                y = pos.y + 0.5,
                z = pos.z + 0.5,
                volume = 1f,
                pitch = 1f,
                seed = Random.nextLong(),
            )
        owner.sendPlayPacket(SoundPacket.ID) { sound.write(it) }
        for (other in owner.playersInSameWorld()) {
            if (other.entityId == owner.playerSession?.entityId) continue
            other.connection.sendPlayPacket(SoundPacket.ID) { sound.write(it) }
        }
    }

    /** Moving a repeater's delay on, or a comparator's mode over: the change and its click. */
    private fun cycleBlock(pos: BlockPos) {
        val world = owner.currentWorld ?: return
        val cycled = BlockInteractions.cycle(world, pos.x, pos.y, pos.z) ?: return
        broadcastBlockUpdate(pos, cycled.stateId)
        // parity: both of them are components, so the change has to reach the dust and the diodes
        // around them: a repeater set to four notches re-books its own turn from here.
        notifyNeighbours(pos)
        val sound = cycled.sound ?: return
        val packet =
            SoundPacket(
                sound = sound,
                source = SoundSource.BLOCKS,
                x = pos.x + 0.5,
                y = pos.y + 0.5,
                z = pos.z + 0.5,
                volume = cycled.volume,
                pitch = cycled.pitch,
                seed = Random.nextLong(),
            )
        owner.sendPlayPacket(SoundPacket.ID) { packet.write(it) }
        for (other in owner.playersInSameWorld()) {
            if (other.entityId == owner.playerSession?.entityId) continue
            other.connection.sendPlayPacket(SoundPacket.ID) { packet.write(it) }
        }
    }

    /**
     * Clicking a bed: sleep in it, or say why not.
     *
     * parity: `BedBlock.useWithoutItem` over `ServerPlayer.startSleepInBed`. The spawn point is set
     * before the night check, as vanilla does, so a bed clicked in daylight still becomes home.
     */
    private fun useBed(
        session: PlayerSession,
        pos: BlockPos,
        state: Int,
    ) {
        val world = owner.currentWorld ?: return
        val step = Beds.headOffset(state)
        val head = if (Beds.isHead(state)) pos else BlockPos(pos.x + step[0], pos.y + step[1], pos.z + step[2])
        val headState = world.state.getBlockState(head.x, head.y, head.z) ?: return
        if (!Beds.isBed(headState)) return

        if (Beds.explodesIn(world.declaration.dimension)) {
            for (block in Beds.destroy(world, head.x, head.y, head.z)) {
                broadcastBlockUpdate(BlockPos(block[0], block[1], block[2]), block[3])
            }
            owner.sendActionBar(Component.translatable("block.minecraft.bed.not_valid"))
            // parity: `BedBlock.useWithoutItem`, which does not merely refuse: it takes the bed out
            // and sets off an explosion of five where it stood.
            blowUp(world, head.x + 0.5, head.y + 0.5, head.z + 0.5, Explosions.BED_POWER, "bed")
            return
        }
        // parity: `ServerPlayer.startSleepInBed` sets the spawn point once the bed is reachable and
        // free, and only then asks whether the night allows sleeping.
        val reachProblem = bedReachProblem(session, head, headState)
        if (reachProblem != null) {
            owner.sendActionBar(Component.translatable(reachProblem.translationKey))
            return
        }
        setSpawnPointAt(session, head)
        if (!Beds.isNight(world.clock.totalTicks) && !world.weather.thundering) {
            owner.sendActionBar(Component.translatable(BedProblem.NOT_POSSIBLE_NOW.translationKey))
            return
        }

        world.state.setBlockState(head.x, head.y, head.z, Beds.withOccupied(headState, occupied = true))
        broadcastBlockUpdate(head, Beds.withOccupied(headState, occupied = true))
        val sleeping =
            owner.playerEvent(PluginEventTypes.PLAYER_BED) { event ->
                event.set(3, head.x)
                event.set(4, head.y)
                event.set(5, head.z)
                event.set(6, true)
            }
        if (sleeping?.cancelled == true) {
            sleeping.cancelMessage?.let { owner.sendPluginNotice(it) }
            return
        }
        session.sleepingBed = Triple(head.x, head.y, head.z)
        session.sleepTicks = 0
        session.stats.reset(StoredStats.TIME_SINCE_REST)
        val packet = SetEntityDataPacket.sleeping(session.entityId, head)
        owner.sendPlayPacket(SetEntityDataPacket.ID) { packet.write(it) }
        for (other in owner.playersInSameWorld()) {
            if (other.entityId == session.entityId) continue
            other.connection.sendPlayPacket(SetEntityDataPacket.ID) { packet.write(it) }
        }
    }

    private fun bedReachProblem(
        session: PlayerSession,
        head: BlockPos,
        headState: Int,
    ): BedProblem? {
        if (Beds.isOccupied(headState)) return BedProblem.OCCUPIED
        val dx = session.x - (head.x + 0.5)
        val dy = session.y - head.y
        val dz = session.z - (head.z + 0.5)
        if (dx * dx + dy * dy + dz * dz > Beds.RANGE * Beds.RANGE) return BedProblem.TOO_FAR_AWAY
        return null
    }

    /** parity: the bed becomes the respawn point whether or not the night lets anyone sleep. */
    private fun setSpawnPointAt(
        session: PlayerSession,
        bed: BlockPos,
    ) {
        val access = owner.playerStore?.takeIf { it.isOpen }?.access ?: return
        access.setSpawnPoint(
            StoredSpawnPoint(
                uuid = session.uuid.toString(),
                world = session.worldName,
                x = bed.x + 0.5,
                y = bed.y.toDouble(),
                z = bed.z + 0.5,
                yaw = session.yaw,
            ),
        )
        owner.sendActionBar(Component.translatable("block.minecraft.set_spawn"))
    }

    /** Out of bed: the flag on the block and the pose. Nobody was moved into it, so nobody moves out. */
    internal fun wakeUp() {
        val session = owner.playerSession ?: return
        val bed = session.sleepingBed ?: return
        session.sleepingBed = null
        session.sleepTicks = 0
        val world = owner.currentWorld
        val state = world?.state?.getBlockState(bed.first, bed.second, bed.third)
        if (world != null && state != null && Beds.isBed(state)) {
            val awake = Beds.withOccupied(state, occupied = false)
            world.state.setBlockState(bed.first, bed.second, bed.third, awake)
            broadcastBlockUpdate(BlockPos(bed.first, bed.second, bed.third), awake)
        }
        val packet = SetEntityDataPacket.sleeping(session.entityId, null)
        owner.sendPlayPacket(SetEntityDataPacket.ID) { packet.write(it) }
        for (other in owner.playersInSameWorld()) {
            if (other.entityId == session.entityId) continue
            other.connection.sendPlayPacket(SetEntityDataPacket.ID) { packet.write(it) }
        }
    }
}
