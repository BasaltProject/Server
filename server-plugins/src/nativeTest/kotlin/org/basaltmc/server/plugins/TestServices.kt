package org.basaltmc.server.plugins

/** The server, as much of it as the host borrows, with the clock in the test's hands. */
class TestServices(
    private val root: String,
) : PluginServices {
    override val tasks: TestTasks = TestTasks()
    override val commands: TestCommands = TestCommands()
    override val paths: PluginPaths =
        object : PluginPaths {
            override val packages: String get() = "$root/plugins"

            override fun folder(pluginId: String): String = "$root/plugins/$pluginId"
        }

    override val players: TestPlayers = TestPlayers()
    override val worlds: TestWorlds = TestWorlds()
    override val placeholders: TestPlaceholders = TestPlaceholders(players)
    override val storage: TestStorage = TestStorage()
    override val menus: TestMenus = TestMenus()
    override val packets: TestPackets = TestPackets()
    override val channels: TestChannels = TestChannels()

    /** One owner in a test, so everything is "here" and `at` runs on the spot. */
    override fun at(
        world: String,
        x: Double,
        z: Double,
        task: () -> Unit,
    ): Boolean {
        if (worlds.byName(world) == null) return false
        task()
        return true
    }

    fun reset() {
        commands.replies.clear()
    }
}

/** A player a test can put online, move and read back. */
class TestPlayer(
    override val name: String,
    override val uuid: String = "uuid-$name",
) : PluginPlayerRef {
    override var online: Boolean = true
    override var health: Double = 20.0
    override var food: Int = 20
    override var gamemode: String = "survival"
    override var level: Int = 0
    override var world: String = "world"
    override var x: Double = 0.0
    override var y: Double = 64.0
    override var z: Double = 0.0
    override val ping: Int = 0
    override var sneaking: Boolean = false
    override var heldItem: String? = null
    override val inventory: TestInventory = TestInventory()

    val received: MutableList<String> = mutableListOf()
    val actionBars: MutableList<String> = mutableListOf()
    val given: MutableList<Pair<String, Int>> = mutableListOf()
    var kickedWith: String? = null
    val permissions: MutableSet<String> = mutableSetOf()

    override fun send(message: String) {
        received += message
    }

    override fun actionBar(message: String) {
        actionBars += message
    }

    override fun teleport(
        world: String,
        x: Double,
        y: Double,
        z: Double,
    ): Boolean {
        this.world = world
        this.x = x
        this.y = y
        this.z = z
        return true
    }

    override fun kick(reason: String) {
        kickedWith = reason
        online = false
    }

    override fun may(permission: String): Boolean = permission in permissions

    val attributeBases: MutableMap<String, Double> = mutableMapOf("minecraft:max_health" to 20.0)

    override fun attribute(name: String): Double? = attributeBases[qualify(name)]

    override fun setAttribute(
        name: String,
        value: Double,
    ): Boolean {
        if (qualify(name) !in KNOWN) return false
        attributeBases[qualify(name)] = value
        return true
    }

    override fun attributes(): List<String> = KNOWN.sorted()

    private fun qualify(name: String) = if (':' in name) name else "minecraft:$name"

    val titles: MutableList<String> = mutableListOf()
    val sounds: MutableList<String> = mutableListOf()
    val particles: MutableList<String> = mutableListOf()

    override fun title(
        title: String,
        subtitle: String,
        fadeIn: Int,
        stay: Int,
        fadeOut: Int,
    ) {
        titles += "$title|$subtitle|$fadeIn/$stay/$fadeOut"
    }

    override fun sound(
        sound: String,
        volume: Float,
        pitch: Float,
    ) {
        sounds += "$sound@$volume/$pitch"
    }

    override fun particle(
        particle: String,
        x: Double,
        y: Double,
        z: Double,
        count: Int,
        spread: Double,
    ) {
        particles += "$particle at $x,$y,$z x$count"
    }

    override fun give(
        item: String,
        count: Int,
    ): Boolean {
        given += item to count
        return true
    }
}

/** Forty-one slots in a list, with vanilla's numbering and none of the packets. */
class TestInventory : PluginInventory {
    val slots: Array<PluginItem?> = arrayOfNulls(41)
    override var heldSlot: Int = 0

    override val size: Int get() = slots.size

    override fun get(slot: Int): PluginItem? = slots.getOrNull(slot)

    override fun set(
        slot: Int,
        item: PluginItem?,
    ) {
        if (slot in slots.indices) slots[slot] = item
    }

    override fun add(item: PluginItem): Int {
        var remaining = item.count
        for (slot in slots.indices) {
            if (remaining <= 0) break
            val existing = slots[slot]
            if (existing != null && existing.id == item.id) {
                val moved = minOf(MAX_STACK - existing.count, remaining)
                if (moved <= 0) continue
                slots[slot] = existing.copy(count = existing.count + moved)
                remaining -= moved
            }
        }
        for (slot in slots.indices) {
            if (remaining <= 0) break
            if (slots[slot] != null) continue
            val moved = minOf(MAX_STACK, remaining)
            slots[slot] = item.copy(count = moved)
            remaining -= moved
        }
        return remaining
    }

    override fun remove(
        id: String,
        count: Int,
    ): Int {
        var left = count
        for (slot in slots.indices) {
            if (left <= 0) break
            val stack = slots[slot] ?: continue
            if (stack.id != id) continue
            val taken = minOf(stack.count, left)
            slots[slot] = if (stack.count <= taken) null else stack.copy(count = stack.count - taken)
            left -= taken
        }
        return count - left
    }

    override fun count(id: String): Int = slots.filterNotNull().filter { it.id == id }.sumOf { it.count }

    override fun clear() {
        slots.fill(null)
    }

    private companion object {
        const val MAX_STACK = 64
    }
}

/** Menus in memory, so a test can click one without a client. */
class TestMenus : PluginMenus {
    class Open(
        val title: String,
        val rows: Int,
        var items: Map<Int, PluginItem>,
        val onClick: (Int) -> Unit,
    )

    val open: MutableMap<String, Open> = mutableMapOf()

    override fun open(
        player: PluginPlayerRef,
        title: String,
        rows: Int,
        items: Map<Int, PluginItem>,
        onClick: (Int) -> Unit,
    ): PluginHandle {
        open[player.uuid] = Open(title, rows, items, onClick)
        return PluginHandle { open.remove(player.uuid) }
    }

    override fun close(player: PluginPlayerRef) {
        open.remove(player.uuid)
    }

    override fun refresh(
        player: PluginPlayerRef,
        items: Map<Int, PluginItem>,
    ) {
        open[player.uuid]?.items = items
    }

    /** What a click looks like from the other side of the window. */
    fun click(
        player: PluginPlayerRef,
        slot: Int,
    ) {
        open[player.uuid]?.onClick?.invoke(slot)
    }
}

/** The packets this build knows, as a test decides them. */
class TestPackets : PluginPacketSender {
    val known: MutableSet<String> =
        mutableSetOf("clientbound.system_chat", "serverbound.chat_command", "clientbound.set_title_text")
    val sent: MutableList<Pair<String, Int>> = mutableListOf()

    override fun send(
        player: PluginPlayerRef,
        name: String,
        bytes: ByteArray,
    ): Boolean {
        if (name !in known) return false
        sent += name to bytes.size
        return true
    }

    override fun isKnown(name: String): Boolean = name in known

    override fun names(): List<String> = known.sorted()
}

/** What a plugin sent out of the server, without a client to receive it. */
class TestChannels : PluginChannelSender {
    val sent: MutableList<Triple<String, String, Int>> = mutableListOf()

    override fun send(
        player: PluginPlayerRef,
        channel: String,
        bytes: ByteArray,
    ): Boolean {
        sent += Triple(player.name, channel, bytes.size)
        return true
    }
}

private val KNOWN = setOf("minecraft:max_health", "minecraft:movement_speed", "minecraft:attack_damage")

class TestPlayers : PluginPlayers {
    val list: MutableList<TestPlayer> = mutableListOf()

    override fun online(): List<PluginPlayerRef> = list.filter { it.online }

    override fun byName(name: String): PluginPlayerRef? =
        list.firstOrNull { it.name.equals(name, ignoreCase = true) && it.online }

    override fun byUuid(uuid: String): PluginPlayerRef? = list.firstOrNull { it.uuid == uuid && it.online }
}

class TestEntity(
    override val id: Int,
    override val type: String,
    override val world: String,
) : PluginEntityRef {
    override var x: Double = 0.0
    override var y: Double = 64.0
    override var z: Double = 0.0
    override var health: Double = 20.0
    override var alive: Boolean = true

    override fun remove() {
        alive = false
    }

    override fun teleport(
        x: Double,
        y: Double,
        z: Double,
    ): Boolean {
        this.x = x
        this.y = y
        this.z = z
        return true
    }
}

class TestWorld(
    override val name: String,
) : PluginWorldRef {
    override var time: Long = 0
    override var raining: Boolean = false
    override var thundering: Boolean = false
    override var difficulty: String = "easy"

    val blocks: MutableMap<Triple<Int, Int, Int>, String> = mutableMapOf()
    val mobs: MutableList<TestEntity> = mutableListOf()
    private var nextId = 100

    override fun block(
        x: Int,
        y: Int,
        z: Int,
    ): String? = blocks[Triple(x, y, z)]

    override fun setBlock(
        x: Int,
        y: Int,
        z: Int,
        block: String,
    ): Boolean {
        blocks[Triple(x, y, z)] = block
        return true
    }

    override fun entities(): List<PluginEntityRef> = mobs.filter { it.alive }

    val rules: MutableMap<String, String> = mutableMapOf("doDaylightCycle" to "true", "randomTickSpeed" to "3")

    override fun gameRule(name: String): String? = rules[name]

    override fun setGameRule(
        name: String,
        value: String,
    ): Boolean {
        if (name !in rules) return false
        rules[name] = value
        return true
    }

    override fun gameRules(): List<String> = rules.keys.sorted()

    val holograms: MutableList<TestHologram> = mutableListOf()
    val npcs: MutableList<TestNpc> = mutableListOf()

    override fun npc(
        name: String,
        x: Double,
        y: Double,
        z: Double,
        onClick: ((String) -> Unit)?,
    ): PluginNpcRef {
        val made = TestNpc(nextId++, name, this.name, onClick)
        made.x = x
        made.y = y
        made.z = z
        npcs += made
        return made
    }

    override fun hologram(
        text: String,
        x: Double,
        y: Double,
        z: Double,
    ): PluginHologramRef {
        val made = TestHologram(nextId++, name, text)
        made.x = x
        made.y = y
        made.z = z
        holograms += made
        return made
    }

    override fun spawn(
        type: String,
        x: Double,
        y: Double,
        z: Double,
    ): PluginEntityRef? {
        if (!type.startsWith("minecraft:")) return null
        val entity = TestEntity(nextId++, type, name)
        entity.x = x
        entity.y = y
        entity.z = z
        mobs += entity
        return entity
    }
}

/** A player-shaped entity a test can click. */
class TestNpc(
    override val id: Int,
    override var name: String,
    override val world: String,
    private val onClick: ((String) -> Unit)?,
) : PluginNpcRef {
    override val uuid: String = "npc-$id"
    override var x: Double = 0.0
    override var y: Double = 0.0
    override var z: Double = 0.0
    override var alive: Boolean = true
    var skinTexture: String? = null
    var yaw: Float = 0f
    private var shownTo: List<String>? = null

    override fun skin(
        texture: String,
        signature: String?,
    ) {
        skinTexture = texture
    }

    override fun look(
        yaw: Float,
        pitch: Float,
    ) {
        this.yaw = yaw
    }

    override fun teleport(
        x: Double,
        y: Double,
        z: Double,
    ): Boolean {
        this.x = x
        this.y = y
        this.z = z
        return true
    }

    override fun showOnlyTo(uuids: List<String>) {
        shownTo = uuids
    }

    override fun showToAll() {
        shownTo = null
    }

    override val audience: List<String>? get() = shownTo

    override fun remove() {
        alive = false
    }

    /** What a right-click looks like from the other side. */
    fun click(who: PluginPlayerRef) {
        onClick?.invoke(who.uuid)
    }
}

/** Floating text a test can read back. */
class TestHologram(
    override val id: Int,
    override val world: String,
    override var text: String,
) : PluginHologramRef {
    override val type: String = "minecraft:text_display"
    override var x: Double = 0.0
    override var y: Double = 0.0
    override var z: Double = 0.0
    override var health: Double = 1.0
    override var alive: Boolean = true
    private var shownTo: List<String>? = null

    override val audience: List<String>? get() = shownTo

    override fun showOnlyTo(uuids: List<String>) {
        shownTo = uuids
    }

    override fun showToAll() {
        shownTo = null
    }

    override fun remove() {
        alive = false
    }

    override fun teleport(
        x: Double,
        y: Double,
        z: Double,
    ): Boolean {
        this.x = x
        this.y = y
        this.z = z
        return true
    }
}

class TestWorlds : PluginWorlds {
    val list: MutableList<TestWorld> = mutableListOf(TestWorld("world"))

    override fun all(): List<PluginWorldRef> = list

    override fun byName(name: String): PluginWorldRef? = list.firstOrNull { it.name == name }

    override val default: PluginWorldRef? get() = list.firstOrNull()

    override fun create(
        name: String,
        dimension: String,
        type: String,
        seed: Long?,
    ): PluginWorldRef? {
        if (list.any { it.name == name } || dimension !in DIMENSIONS || type !in TYPES) return null
        return TestWorld(name).also { list += it }
    }

    private companion object {
        val DIMENSIONS = setOf("overworld", "nether", "the_end")
        val TYPES = setOf("normal", "flat", "void", "amplified")
    }
}

class TestPlaceholders(
    private val players: TestPlayers,
) : PluginPlaceholders {
    val registered: MutableMap<String, (PluginPlayerRef?) -> String> = mutableMapOf()

    override fun register(
        pluginId: String,
        name: String,
        value: (PluginPlayerRef?) -> String,
    ): PluginHandle {
        val identifier = "${pluginId}_$name"
        registered[identifier] = value
        return PluginHandle { registered.remove(identifier) }
    }

    override fun fill(
        text: String,
        player: PluginPlayerRef?,
    ): String {
        var out = text
        for ((identifier, value) in registered) out = out.replace("%$identifier%", value(player))
        return out
    }
}

class TestTasks : PluginTasks {
    private class Job(
        var due: Long,
        val period: Int,
        val task: () -> Unit,
    ) {
        var cancelled = false
    }

    private val jobs = mutableListOf<Job>()
    private var now = 0L

    /** How many times a repeating job has run, so a test can say when a timer went off. */
    var ranTimers: Int = 0
        private set

    override val tick: Long get() = now

    override fun after(
        delayTicks: Int,
        task: () -> Unit,
    ): PluginHandle {
        val job = Job(now + delayTicks, -1, task)
        jobs += job
        return PluginHandle { job.cancelled = true }
    }

    override fun every(
        delayTicks: Int,
        periodTicks: Int,
        task: () -> Unit,
    ): PluginHandle {
        val job = Job(now + delayTicks, periodTicks, task)
        jobs += job
        return PluginHandle { job.cancelled = true }
    }

    /** Runs the clock forward one tick at a time, like the server's own loop does. */
    fun advance(ticks: Int) {
        repeat(ticks) {
            now++
            for (job in jobs.toList()) {
                if (job.cancelled || job.due > now) continue
                if (job.period > 0) {
                    job.due = now + job.period
                    ranTimers++
                } else {
                    jobs.remove(job)
                }
                job.task()
            }
            jobs.removeAll { it.cancelled }
        }
    }
}

class TestCommands : PluginCommands {
    class Registered(
        val pluginId: String,
        val spec: PluginCommandSpec,
        val run: (PluginCommandCall) -> Unit,
    )

    val registered: MutableList<Registered> = mutableListOf()
    val replies: MutableList<String> = mutableListOf()

    override fun register(
        pluginId: String,
        spec: PluginCommandSpec,
        run: (PluginCommandCall) -> Unit,
    ): PluginHandle {
        val entry = Registered(pluginId, spec, run)
        registered += entry
        return PluginHandle { registered.remove(entry) }
    }

    /**
     * Runs a command the way the dispatcher would, with the arguments already parsed.
     *
     * With [by] it runs as that player, which is the case most commands are written for: the reply
     * goes to them and `players.by_name(src.name)` finds them.
     */
    fun run(
        name: String,
        arguments: Map<String, Any> = emptyMap(),
        by: TestPlayer? = null,
    ) {
        val entry = registered.firstOrNull { it.spec.name == name } ?: error("no command '$name' is registered")
        entry.run(
            object : PluginCommandCall {
                override val source: String get() = by?.name ?: "tester"

                override val isConsole: Boolean get() = by == null

                override fun reply(message: String) {
                    if (by == null) replies += message else by.send(message)
                }

                override fun argument(name: String): Any? = arguments[name]
            },
        )
    }
}

/** The plugin data table, in memory, with the same namespacing the real one has. */
class TestStorage : PluginStorage {
    val rows: MutableMap<Triple<String, String, String>, String> = mutableMapOf()

    override fun playerValue(
        pluginId: String,
        uuid: String,
        key: String,
    ): String? = rows[Triple(pluginId, uuid, key)]

    override fun setPlayerValue(
        pluginId: String,
        uuid: String,
        key: String,
        value: String?,
    ) {
        val at = Triple(pluginId, uuid, key)
        if (value == null) rows.remove(at) else rows[at] = value
    }

    override fun playerValues(
        pluginId: String,
        uuid: String,
    ): Map<String, String> = rows.filterKeys { it.first == pluginId && it.second == uuid }.mapKeys { it.key.third }

    override fun top(
        pluginId: String,
        key: String,
        limit: Int,
    ): List<Pair<String, String>> =
        rows
            .filterKeys { it.first == pluginId && it.third == key }
            .map { it.key.second to it.value }
            .sortedByDescending { it.second.toDoubleOrNull() ?: 0.0 }
            .take(limit)
}
