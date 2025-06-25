package org.basaltmc.server.runtime.player

import org.basaltmc.server.runtime.ClientSession
import org.basaltmc.server.runtime.world.ActiveEffect
import org.basaltmc.server.runtime.world.Fluids
import org.basaltmc.server.storage.StoredItemStack
import org.basaltmc.server.storage.StoredPlayer
import org.basaltmc.wire.common.Identifier
import org.basaltmc.wire.protocol.login.GameProfileProperty
import org.basaltmc.wire.protocol.play.Gamemode
import org.basaltmc.wire.protocol.play.SystemChatPacket
import org.basaltmc.wire.text.Component
import kotlin.uuid.ExperimentalUuidApi
import kotlin.uuid.Uuid

/**
 * Server-side state for one logged-in player: identity, live position, and the survival state the
 * client draws in its HUD.
 *
 * Health, hunger and experience live here rather than on the connection because they outlive any
 * single packet exchange and are what a respawn resets; the connection ([connection]) is only how
 * they reach the client. Nothing here is persisted yet: a rejoin starts from these defaults, which
 * is a documented gap, not the intended end state.
 */
@OptIn(ExperimentalUuidApi::class)
public class PlayerSession(
    public val uuid: Uuid,
    public val name: String,
    public val entityId: Int,
    internal val connection: ClientSession,
    gamemode: Gamemode = Gamemode.SURVIVAL,
) {
    /** What the client said in its handshake, and where it connected from: the same facts a ping carries. */
    public val protocolVersion: Int get() = connection.handshakeProtocol
    public val connectedHost: String get() = connection.handshakeHost
    public val address: String? get() = connection.remoteAddress

    public var x: Double = 0.0
    public var y: Double = 0.0
    public var z: Double = 0.0
    public var yaw: Float = 0f
    public var pitch: Float = 0f
    public var onGround: Boolean = false

    /**
     * Which permission group they belong to, loaded from `players.db` at join. Null means the
     * group currently marked as the default, which is resolved when a permission is checked rather
     * than written into the row.
     */
    public var group: String? = null

    /** Which world this player is in. Sweeps use it so one world's chunks are not held by another's players. */
    public var worldName: String = ""

    public var gamemode: Gamemode = gamemode
    public var sprinting: Boolean = false

    /**
     * Whether they are crouching, from `player_input`'s own bitmask. Read because sneaking is what
     * tells "place a block against this one" apart from "open this one's screen".
     */
    public var sneaking: Boolean = false

    /**
     * The signed profile properties from the session server, carrying this player's skin and cape.
     * Empty in offline mode, where nothing signs them and every client draws the default skin.
     */
    public var profileProperties: List<GameProfileProperty> = emptyList()

    /**
     * Which skin layers this player has switched on in their own video settings (hat, jacket,
     * sleeves, trouser legs). The client owns this and reports it; the server only relays it to
     * everyone else, or nobody would see the outer layer of anyone's skin.
     */
    public var skinParts: Byte = ALL_SKIN_PARTS

    public var health: Float = MAX_HEALTH

    /** parity: `LivingEntity.getAbsorptionAmount`, the pool damage eats before health does. */
    public var absorption: Float = 0f

    /** Where this player is asleep, and for how many ticks. parity: `LivingEntity.sleepingPos`. */
    public var sleepingBed: Triple<Int, Int, Int>? = null
    public var sleepTicks: Int = 0

    /** parity: `LivingEntity.invulnerableTime`, the half second after a hit when most others miss. */
    public var invulnerableTicks: Int = 0

    /** parity: `LivingEntity.lastHurt`: inside that window only a bigger hit lands, and only the rest of it. */
    public var lastHurt: Float = 0f

    /**
     * Ticks since the last swing, which is what a hit is charged from.
     *
     * parity: `Player.attackStrengthTicker`. A swing before the bar refills lands a fifth of the
     * damage, which is the whole reason a player waits between hits.
     */
    public var ticksSinceSwing: Int = 0

    public val food: FoodData = FoodData()
    public val experience: ExperienceData = ExperienceData()

    /**
     * Scoreboard tags, as `/tag` sets them. On the live session only: vanilla keeps them with the
     * saved entity, which here needs a column in `players.db`.
     */
    public val tags: MutableSet<String> = mutableSetOf()

    /** Counters, incremented as this server notices things happening. See [PlayerStats]. */
    public val stats: PlayerStats = PlayerStats()

    /** The 41 slots this player carries. See [PlayerInventory] for what is and is not modelled. */
    public val inventory: PlayerInventory = PlayerInventory(connection.enchantmentIds)

    /**
     * The twenty-seven slots that follow this player rather than the block.
     *
     * parity: `PlayerEnderChestContainer`, which is why every ender chest in the world shows the
     * same things to one player and different things to the next. Persisted in `players.db`, not in
     * any world.
     */
    public val enderChest: Array<StoredItemStack?> = arrayOfNulls(StoredPlayer.ENDER_CHEST_SLOTS)

    /**
     * Air left and how long this player keeps burning.
     *
     * parity: `Entity`'s own two synced values. The client draws the bubbles and the flames from
     * what the server sends, and moves neither on its own.
     */
    public var airSupply: Int = Fluids.MAX_AIR

    public var fireTicks: Int = 0

    /** Status effects this player carries, by effect id. parity: `LivingEntity.activeEffects`. */
    public val effects: MutableMap<Identifier, ActiveEffect> = mutableMapOf()

    /**
     * How long this player has been standing in a portal, and how long until one works again.
     *
     * parity: `Entity.portalProcess`/`portalCooldown`, counted on the player rather than on the
     * connection: a portal half entered survives a chunk reload the way vanilla's does.
     */
    public var portalTicks: Int = 0

    public var portalCooldown: Int = 0

    /** Set the moment health hits zero, cleared on respawn. A dead player stops being ticked. */
    public var dead: Boolean = false

    /** Client-driven, and only meaningful when the game mode allows it. */
    public var flying: Boolean = false

    public val mayFly: Boolean
        get() = gamemode == Gamemode.CREATIVE || gamemode == Gamemode.SPECTATOR

    public val invulnerable: Boolean
        get() = gamemode == Gamemode.CREATIVE || gamemode == Gamemode.SPECTATOR

    public val instabuild: Boolean
        get() = gamemode == Gamemode.CREATIVE

    public fun sendSystemMessage(message: Component) {
        connection.sendPlayPacket(SystemChatPacket.ID) { SystemChatPacket(message, isActionBar = false).write(it) }
    }

    /** Back to a freshly spawned player: full health, full food, no experience kept. */
    public fun resetForRespawn() {
        health = MAX_HEALTH
        food.reset()
        experience.reset()
        dead = false
        flying = gamemode == Gamemode.SPECTATOR
        sprinting = false
        // parity: `ServerPlayer.restoreFrom` on a respawn, which starts them breathing and unlit.
        airSupply = Fluids.MAX_AIR
        fireTicks = 0
        effects.clear()
    }

    public companion object {
        public const val MAX_HEALTH: Float = 20f

        /** Every layer on, which is what a client that has not told us otherwise almost certainly has. */
        public const val ALL_SKIN_PARTS: Byte = 0x7F
    }
}
