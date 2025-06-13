package org.basaltmc.server.runtime.command.commands

import org.basaltmc.server.runtime.command.BlockCoordinates
import org.basaltmc.server.runtime.command.BlockPosArgument
import org.basaltmc.server.runtime.command.Command
import org.basaltmc.server.runtime.command.CommandContext
import org.basaltmc.server.runtime.command.CommandNodeBuilder
import org.basaltmc.server.runtime.command.CommandSyntaxException
import org.basaltmc.server.runtime.command.IntegerArgument
import org.basaltmc.server.runtime.command.NbtCompoundArgument
import org.basaltmc.server.runtime.command.NbtPathArgument
import org.basaltmc.server.runtime.command.NbtTagArgument
import org.basaltmc.server.runtime.command.PermissionLevel
import org.basaltmc.server.runtime.command.PlayersArgument
import org.basaltmc.server.runtime.command.ResourceLocationArgument
import org.basaltmc.server.runtime.command.world
import org.basaltmc.server.runtime.player.PlayerSession
import org.basaltmc.wire.common.Identifier
import org.basaltmc.wire.io.FileIO
import org.basaltmc.wire.nbt.NbtByte
import org.basaltmc.wire.nbt.NbtCompound
import org.basaltmc.wire.nbt.NbtDouble
import org.basaltmc.wire.nbt.NbtFloat
import org.basaltmc.wire.nbt.NbtIO
import org.basaltmc.wire.nbt.NbtInt
import org.basaltmc.wire.nbt.NbtList
import org.basaltmc.wire.nbt.NbtLong
import org.basaltmc.wire.nbt.NbtShort
import org.basaltmc.wire.nbt.NbtString
import org.basaltmc.wire.nbt.NbtTag
import org.basaltmc.wire.nbt.NbtType
import org.basaltmc.wire.nbt.Snbt
import org.basaltmc.wire.text.Component

/**
 * Named NBT compounds a command can keep things in. parity: `CommandStorage`, minus persistence:
 * vanilla writes them to `command_storage_<namespace>.dat`, and here they live for the run.
 */
public class CommandStorage(
    /** Where it is kept between runs. parity: vanilla's `command_storage_<namespace>.dat`, in one file. */
    private val path: String = DEFAULT_PATH,
) {
    private val compounds = mutableMapOf<Identifier, NbtCompound>()

    public operator fun get(id: Identifier): NbtCompound = compounds.getOrPut(id) { NbtCompound() }

    public operator fun set(
        id: Identifier,
        value: NbtCompound,
    ) {
        compounds[id] = value
    }

    public fun ids(): Set<Identifier> = compounds.keys.toSet()

    /** Reads whatever the last run left. A missing or unreadable file simply starts empty. */
    public fun load() {
        val bytes = FileIO.readBytes(path) ?: return
        val root = runCatching { NbtIO.read(bytes).tag as? NbtCompound }.getOrNull() ?: return
        compounds.clear()
        for (key in root.keys) {
            val value = root[key] as? NbtCompound ?: continue
            compounds[Identifier.parse(key)] = value
        }
    }

    public fun save() {
        if (compounds.isEmpty()) return
        val root = NbtCompound()
        for ((id, value) in compounds) root[id.toString()] = value
        // The directory may not exist yet: the server writes this beside `worlds/`, which is
        // created on demand, and a test points it inside `build/`.
        path.substringBeforeLast('/', "").takeIf { it.isNotEmpty() }?.let(FileIO::createDirectories)
        FileIO.writeBytes(path, NbtIO.write(root))
    }

    public companion object {
        /** Beside `worlds/` and `players.db`: it belongs to the server, not to one world. */
        public const val DEFAULT_PATH: String = "command_storage.nbt"
    }
}

/**
 * parity: `DataCommands`, over the three holders this server has NBT for: command storage, a block
 * entity, and a player.
 *
 * A player is a read-only holder: their NBT is a view built from live session state, so a write
 * would land on a copy and quietly do nothing.
 */
public class DataCommand(
    private val storage: CommandStorage,
) : Command(name = "data", permissionLevel = PermissionLevel.GAMEMASTERS) {
    override fun build(root: CommandNodeBuilder) {
        root.literal("get") {
            holders {
                executes { context -> get(context, null) }
                argument("path", NbtPathArgument) { executes { context -> get(context, context.get("path")) } }
            }
        }
        root.literal("merge") {
            holders {
                argument("nbt", NbtCompoundArgument) { executes { context -> merge(context) } }
            }
        }
        root.literal("remove") {
            holders {
                argument("path", NbtPathArgument) { executes { context -> remove(context) } }
            }
        }
        root.literal("modify") {
            holders {
                argument("path", NbtPathArgument) {
                    literal("set") {
                        literal("value") {
                            argument("nbt", NbtTagArgument) { executes { context -> modify(context, false) } }
                        }
                    }
                    literal("merge") {
                        literal("value") {
                            argument("nbt", NbtTagArgument) { executes { context -> modify(context, true) } }
                        }
                    }
                    for (mode in listOf("append", "prepend", "insert")) {
                        literal(mode) {
                            if (mode == "insert") {
                                argument("index", IntegerArgument()) {
                                    literal("value") {
                                        argument("nbt", NbtTagArgument) {
                                            executes { context -> modifyList(context, mode) }
                                        }
                                    }
                                }
                            } else {
                                literal("value") {
                                    argument("nbt", NbtTagArgument) {
                                        executes { context -> modifyList(context, mode) }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    /** The three `(block|entity|storage)` forms, declared once per verb the way vanilla does. */
    private fun CommandNodeBuilder.holders(build: CommandNodeBuilder.() -> Unit) {
        literal("block") { argument("pos", BlockPosArgument) { build() } }
        literal("entity") { argument("target", PlayersArgument(single = true)) { build() } }
        literal("storage") { argument("source", ResourceLocationArgument) { build() } }
    }

    private fun holderOf(context: CommandContext): Holder =
        when {
            context.has("source") -> Holder.Storage(context.get("source"))
            context.has("pos") -> Holder.Block(context.get("pos"))
            context.has("target") -> Holder.Entity(context.get<List<PlayerSession>>("target").single())
            else -> throw CommandSyntaxException(Component.text("No data holder was given."))
        }

    private sealed interface Holder {
        class Storage(
            val id: Identifier,
        ) : Holder

        class Block(
            val pos: BlockCoordinates,
        ) : Holder

        class Entity(
            val player: PlayerSession,
        ) : Holder
    }

    private fun read(
        context: CommandContext,
        holder: Holder,
    ): NbtCompound =
        when (holder) {
            is Holder.Storage -> storage[holder.id]
            is Holder.Block -> {
                val world = context.world()
                val (x, y, z) = holder.pos.resolve(context.source.position)
                val chunk =
                    world.state.chunkIfLoaded(x shr 4, z shr 4)
                        ?: throw CommandSyntaxException(Component.translatable("argument.pos.unloaded"))
                chunk.blockEntityAt(x and 15, y, z and 15)?.data
                    ?: throw CommandSyntaxException(
                        Component.translatable("commands.data.block.invalid"),
                    )
            }
            is Holder.Entity -> playerNbt(holder.player)
        }

    private fun write(
        context: CommandContext,
        holder: Holder,
        value: NbtCompound,
    ) {
        when (holder) {
            is Holder.Storage -> storage[holder.id] = value
            is Holder.Block -> {
                val world = context.world()
                val (x, y, z) = holder.pos.resolve(context.source.position)
                val chunk =
                    world.state.chunkIfLoaded(x shr 4, z shr 4)
                        ?: throw CommandSyntaxException(Component.translatable("argument.pos.unloaded"))
                val existing =
                    chunk.blockEntityAt(x and 15, y, z and 15)
                        ?: throw CommandSyntaxException(
                            Component.translatable("commands.data.block.invalid"),
                        )
                chunk.setBlockEntity(existing.copy(data = value))
            }
            is Holder.Entity ->
                throw CommandSyntaxException(
                    Component.text(
                        "A player's data is a read-only view of live state in this build, so writing to it would " +
                            "change a copy and do nothing.",
                    ),
                )
        }
    }

    private fun get(
        context: CommandContext,
        path: String?,
    ): Int {
        val holder = holderOf(context)
        val root = read(context, holder)
        val value = if (path == null) root else DataPaths.resolve(root, path) ?: throw noSuchPath(path)
        context.success(queryMessage(context, holder, value))
        return numeric(value) ?: 1
    }

    private fun merge(context: CommandContext): Int {
        val holder = holderOf(context)
        val incoming = context.get<NbtCompound>("nbt")
        val merged = DataPaths.merge(read(context, holder), incoming)
        write(context, holder, merged)
        context.success(
            modifiedMessage(context, holder),
            broadcastToAdmins = true,
        )
        return 1
    }

    private fun remove(context: CommandContext): Int {
        val holder = holderOf(context)
        val path = context.get<String>("path")
        val root = read(context, holder)
        val removed = DataPaths.remove(root, path) ?: throw noSuchPath(path)
        write(context, holder, removed)
        context.success(
            modifiedMessage(context, holder),
            broadcastToAdmins = true,
        )
        return 1
    }

    private fun modify(
        context: CommandContext,
        merging: Boolean,
    ): Int {
        val holder = holderOf(context)
        val path = context.get<String>("path")
        val value = context.get<NbtTag>("nbt")
        val root = read(context, holder)
        val existing = DataPaths.resolve(root, path)
        val next =
            if (merging && existing is NbtCompound && value is NbtCompound) {
                DataPaths.merge(existing, value)
            } else {
                value
            }
        write(context, holder, DataPaths.set(root, path, next))
        context.success(
            modifiedMessage(context, holder),
            broadcastToAdmins = true,
        )
        return 1
    }

    /** `append`, `prepend` and `insert <index>`, which are the list forms of `/data modify`. */
    private fun modifyList(
        context: CommandContext,
        mode: String,
    ): Int {
        val holder = holderOf(context)
        val path = context.get<String>("path")
        val value = context.get<NbtTag>("nbt")
        val root = read(context, holder)
        val existing = DataPaths.resolve(root, path)
        val elements =
            (existing as? NbtList)?.toList().orEmpty().toMutableList()
        val index =
            when (mode) {
                "append" -> elements.size
                "prepend" -> 0
                else -> context.get<Int>("index").let { if (it < 0) elements.size + it + 1 else it }
            }
        if (index !in 0..elements.size) {
            throw CommandSyntaxException(
                Component.translatable("commands.data.modify.invalid_index", Component.text(index.toString())),
            )
        }
        elements.add(index, value)
        val elementType = elements.firstOrNull()?.type ?: value.type
        if (elements.any { it.type != elementType }) {
            throw CommandSyntaxException(
                Component.translatable("commands.data.modify.expected_list", Component.text(path)),
            )
        }
        write(context, holder, DataPaths.set(root, path, NbtList(elementType, elements)))
        context.success(
            modifiedMessage(context, holder),
            broadcastToAdmins = true,
        )
        return elements.size
    }

    /**
     * parity: each holder has its own pair of strings, and the block ones take the three
     * coordinates rather than a description, so there is no generic form to share.
     */
    private fun queryMessage(
        context: CommandContext,
        holder: Holder,
        value: NbtTag,
    ): Component {
        val text = Component.text(Snbt.stringify(value))
        return when (holder) {
            is Holder.Storage ->
                Component.translatable("commands.data.storage.query", Component.text(holder.id.toString()), text)
            is Holder.Entity ->
                Component.translatable("commands.data.entity.query", Component.text(holder.player.name), text)
            is Holder.Block -> {
                val (x, y, z) = holder.pos.resolve(context.source.position)
                Component.translatable(
                    "commands.data.block.query",
                    Component.text(x.toString()),
                    Component.text(y.toString()),
                    Component.text(z.toString()),
                    text,
                )
            }
        }
    }

    private fun modifiedMessage(
        context: CommandContext,
        holder: Holder,
    ): Component =
        when (holder) {
            is Holder.Storage ->
                Component.translatable("commands.data.storage.modified", Component.text(holder.id.toString()))
            is Holder.Entity ->
                Component.translatable("commands.data.entity.modified", Component.text(holder.player.name))
            is Holder.Block -> {
                val (x, y, z) = holder.pos.resolve(context.source.position)
                Component.translatable(
                    "commands.data.block.modified",
                    Component.text(x.toString()),
                    Component.text(y.toString()),
                    Component.text(z.toString()),
                )
            }
        }

    private fun noSuchPath(path: String) =
        CommandSyntaxException(Component.translatable("arguments.nbtpath.nothing_found", Component.text(path)))

    /** A live view of the player, in the shape vanilla's own entity NBT uses for the same fields. */
    private fun playerNbt(player: PlayerSession): NbtCompound =
        NbtCompound().apply {
            putString("id", "minecraft:player")
            putString("Name", player.name)
            putFloat("Health", player.health)
            putInt("XpLevel", player.experience.level)
            putFloat("XpP", player.experience.progress)
            putInt("XpTotal", player.experience.total)
            putString("Dimension", player.worldName)
            this["Pos"] =
                NbtList(NbtType.DOUBLE, listOf(NbtDouble(player.x), NbtDouble(player.y), NbtDouble(player.z)))
            this["Rotation"] = NbtList(NbtType.FLOAT, listOf(NbtFloat(player.yaw), NbtFloat(player.pitch)))
            this["Tags"] = NbtList(NbtType.STRING, player.tags.map { NbtString(it) })
            this["foodLevel"] = NbtInt(player.food.foodLevel)
        }

    /** The result `/data get` reports, which for a number is the number itself. */
    private fun numeric(tag: NbtTag): Int? =
        when (tag) {
            is NbtInt -> tag.value
            is NbtFloat -> tag.value.toInt()
            is NbtDouble -> tag.value.toInt()
            else -> null
        }
}

/** Which holder a `/execute ... data` clause is talking about. */
public sealed interface DataHolderRef {
    public class Storage(
        public val id: Identifier,
    ) : DataHolderRef

    public class Block(
        public val pos: Triple<Int, Int, Int>,
    ) : DataHolderRef

    public class Entity(
        public val player: org.basaltmc.server.runtime.player.PlayerSession,
    ) : DataHolderRef
}

/**
 * The slice of `/data` that `/execute` needs: does this path exist, and write this number there.
 *
 * A separate surface rather than a reference to the command, so `/execute` does not depend on a
 * command's tree to read a compound.
 */
public class DataAccess(
    private val storage: CommandStorage,
) {
    public fun pathExists(
        context: CommandContext,
        holder: DataHolderRef,
        path: String,
    ): Boolean {
        val root =
            when (holder) {
                is DataHolderRef.Storage -> storage[holder.id]
                is DataHolderRef.Block -> {
                    val (x, y, z) = holder.pos
                    val chunk = context.world().state.chunkIfLoaded(x shr 4, z shr 4) ?: return false
                    chunk.blockEntityAt(x and 15, y, z and 15)?.data ?: return false
                }
                is DataHolderRef.Entity -> playerCompound(holder.player)
            }
        return DataPaths.resolve(root, path) != null
    }

    /** `/execute store ... storage <id> <path> <type> <scale>`, which writes the typed number. */
    public fun store(
        id: Identifier,
        path: String,
        value: Double,
        type: String,
    ) {
        storage[id] = DataPaths.set(storage[id], path, numberTag(value, type))
    }

    /** `/execute store ... block <pos> <path> <type> <scale>`, which writes into a block entity. */
    public fun storeInBlock(
        context: CommandContext,
        pos: Triple<Int, Int, Int>,
        path: String,
        value: Double,
        type: String,
    ) {
        val (x, y, z) = pos
        val chunk = context.world().state.chunkIfLoaded(x shr 4, z shr 4) ?: return
        val existing = chunk.blockEntityAt(x and 15, y, z and 15) ?: return
        chunk.setBlockEntity(existing.copy(data = DataPaths.set(existing.data, path, numberTag(value, type))))
    }

    private fun numberTag(
        value: Double,
        type: String,
    ): org.basaltmc.wire.nbt.NbtTag =
        when (type) {
            "byte" -> NbtByte(value.toInt().toByte())
            "short" -> NbtShort(value.toInt().toShort())
            "long" -> NbtLong(value.toLong())
            "float" -> NbtFloat(value.toFloat())
            "double" -> NbtDouble(value)
            else -> NbtInt(value.toInt())
        }

    private fun playerCompound(player: org.basaltmc.server.runtime.player.PlayerSession): NbtCompound =
        NbtCompound().apply {
            putString("Name", player.name)
            putFloat("Health", player.health)
            putInt("XpLevel", player.experience.level)
            this["Tags"] = NbtList(NbtType.STRING, player.tags.map { NbtString(it) })
        }
}

/**
 * NBT paths, the part of vanilla's syntax a datapack actually leans on: dotted keys, `list[index]`,
 * and the compound filter `list[{k:v}]`.
 *
 * Shared by `/data` and by `/execute if data`, so both read a path the same way.
 */
internal object DataPaths {
    fun merge(
        base: NbtCompound,
        incoming: NbtCompound,
    ): NbtCompound {
        val result = NbtCompound()
        for (key in base.keys) base[key]?.let { result[key] = it }
        for (key in incoming.keys) {
            val existing = result[key]
            val value = incoming[key] ?: continue
            result[key] = if (existing is NbtCompound && value is NbtCompound) merge(existing, value) else value
        }
        return result
    }

    fun resolve(
        root: NbtTag,
        path: String,
    ): NbtTag? {
        var current: NbtTag? = root
        for (step in steps(path)) {
            current =
                when (step) {
                    is Step.Key -> (current as? NbtCompound)?.get(step.name)
                    is Step.Index -> (current as? NbtList)?.takeIf { step.index in 0 until it.size }?.get(step.index)
                    is Step.Filter -> (current as? NbtList)?.firstOrNull { matches(it, step.filter) }
                }
            if (current == null) return null
        }
        return current
    }

    fun set(
        root: NbtCompound,
        path: String,
        value: NbtTag,
    ): NbtCompound {
        val steps = steps(path)
        require(steps.isNotEmpty()) { "an empty path cannot be written" }
        val copy = merge(root, NbtCompound())
        var cursor: NbtCompound = copy
        for ((index, step) in steps.withIndex()) {
            val key = (step as? Step.Key)?.name ?: error("only a key may be written to")
            if (index == steps.lastIndex) {
                cursor[key] = value
                return copy
            }
            val next = cursor[key] as? NbtCompound ?: NbtCompound()
            val copied = merge(next, NbtCompound())
            cursor[key] = copied
            cursor = copied
        }
        return copy
    }

    fun remove(
        root: NbtCompound,
        path: String,
    ): NbtCompound? {
        val steps = steps(path)
        if (steps.isEmpty()) return null
        val copy = merge(root, NbtCompound())
        var cursor: NbtCompound = copy
        for ((index, step) in steps.withIndex()) {
            val key = (step as? Step.Key)?.name ?: return null
            if (index == steps.lastIndex) {
                if (cursor[key] == null) return null
                cursor.remove(key)
                return copy
            }
            val next = cursor[key] as? NbtCompound ?: return null
            val copied = merge(next, NbtCompound())
            cursor[key] = copied
            cursor = copied
        }
        return copy
    }

    /** parity: a compound filter matches when every key it names matches, recursively. */
    private fun matches(
        candidate: NbtTag,
        filter: NbtCompound,
    ): Boolean {
        val compound = candidate as? NbtCompound ?: return false
        return filter.keys.all { key ->
            val wanted = filter[key] ?: return@all false
            val actual = compound[key] ?: return@all false
            if (wanted is NbtCompound && actual is NbtCompound) matches(actual, wanted) else wanted == actual
        }
    }

    sealed interface Step {
        class Key(
            val name: String,
        ) : Step

        class Index(
            val index: Int,
        ) : Step

        class Filter(
            val filter: NbtCompound,
        ) : Step
    }

    fun steps(path: String): List<Step> {
        val steps = mutableListOf<Step>()
        for (part in splitOutsideBraces(path)) {
            if (part.isEmpty()) continue
            val bracket = part.indexOf('[')
            if (bracket < 0) {
                steps += Step.Key(part)
                continue
            }
            if (bracket > 0) steps += Step.Key(part.substring(0, bracket))
            var rest = part.substring(bracket)
            while (rest.startsWith("[")) {
                val close = rest.lastIndexOf(']')
                if (close < 0) error("unclosed '[' in path '$path'")
                val inner = rest.substring(1, close)
                steps +=
                    if (inner.startsWith("{")) {
                        Step.Filter(Snbt.parse(inner) as? NbtCompound ?: error("bad filter in '$path'"))
                    } else {
                        Step.Index(inner.toIntOrNull() ?: error("bad list index in '$path'"))
                    }
                rest = rest.substring(close + 1)
            }
        }
        return steps
    }

    /** Splits on dots that are not inside a `{...}` filter, so `a[{id:"x.y"}].b` stays in one piece. */
    private fun splitOutsideBraces(path: String): List<String> {
        val parts = mutableListOf<String>()
        val current = StringBuilder()
        var depth = 0
        for (character in path) {
            when {
                character == '{' -> {
                    depth++
                    current.append(character)
                }
                character == '}' -> {
                    depth--
                    current.append(character)
                }
                character == '.' && depth == 0 -> {
                    parts += current.toString()
                    current.clear()
                }
                else -> current.append(character)
            }
        }
        parts += current.toString()
        return parts
    }
}
