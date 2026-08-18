package org.basaltmc.server.runtime.command

import kotlinx.serialization.json.Json
import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.registry.Items
import org.basaltmc.server.runtime.item.ItemComponentCodecs
import org.basaltmc.wire.common.Identifier
import org.basaltmc.wire.nbt.NbtCompound
import org.basaltmc.wire.nbt.NbtTag
import org.basaltmc.wire.nbt.Snbt
import org.basaltmc.wire.protocol.play.ArgumentParser
import org.basaltmc.wire.text.Component
import org.basaltmc.wire.text.ComponentJson
import kotlin.math.floor

/** A parsed `x y z` of block coordinates, still unresolved: `~` needs whoever runs the command. */
public data class BlockCoordinates(
    val x: WorldCoordinate,
    val y: WorldCoordinate,
    val z: WorldCoordinate,
) {
    /** parity: `WorldCoordinates.getBlockPos`, which floors rather than truncating, so -0.5 is -1. */
    public fun resolve(base: Triple<Double, Double, Double>): Triple<Int, Int, Int> =
        Triple(
            floor(x.resolve(base.first)).toInt(),
            floor(y.resolve(base.second)).toInt(),
            floor(z.resolve(base.third)).toInt(),
        )
}

/** `minecraft:block_pos`. Never centre-corrects: a block coordinate is the block, not its middle. */
public object BlockPosArgument : ArgumentType<BlockCoordinates> {
    override val wire: ArgumentParser = ArgumentParser.BlockPosition

    override fun parse(
        reader: StringReader,
        source: CommandSource,
        environment: CommandEnvironment,
    ): BlockCoordinates {
        val start = reader.cursor
        val x = WorldCoordinate.parse(reader, centerCorrect = false)
        expectSeparator(reader, start)
        val y = WorldCoordinate.parse(reader, centerCorrect = false)
        expectSeparator(reader, start)
        val z = WorldCoordinate.parse(reader, centerCorrect = false)
        return BlockCoordinates(x, y, z)
    }

    private fun expectSeparator(
        reader: StringReader,
        start: Int,
    ) {
        if (!reader.canRead() || reader.peek() != ' ') {
            reader.cursor = start
            throw CommandSyntaxException(Component.translatable("argument.pos3d.incomplete"), reader.string, start)
        }
        reader.skip()
    }
}

/** `minecraft:column_pos`: `x z`, the pair `/forceload` works in chunk coordinates with. */
public data class ColumnCoordinates(
    val x: WorldCoordinate,
    val z: WorldCoordinate,
) {
    public fun resolve(base: Triple<Double, Double, Double>): Pair<Int, Int> =
        floor(x.resolve(base.first)).toInt() to floor(z.resolve(base.third)).toInt()
}

public object ColumnPosArgument : ArgumentType<ColumnCoordinates> {
    override val wire: ArgumentParser = ArgumentParser.ColumnPosition

    override fun parse(
        reader: StringReader,
        source: CommandSource,
        environment: CommandEnvironment,
    ): ColumnCoordinates {
        val start = reader.cursor
        val x = WorldCoordinate.parse(reader, centerCorrect = false)
        if (!reader.canRead() || reader.peek() != ' ') {
            reader.cursor = start
            throw CommandSyntaxException(Component.translatable("argument.pos2d.incomplete"), reader.string, start)
        }
        reader.skip()
        val z = WorldCoordinate.parse(reader, centerCorrect = false)
        return ColumnCoordinates(x, z)
    }
}

/**
 * A block plus the properties written next to it, resolved to the state id that goes on the wire.
 *
 * Only the property combinations this build's registry knows resolve. That is every default state
 * plus the non-default states worldgen writes; anything else is refused by name rather than
 * silently placed as the default, because a `/setblock` that quietly drops `facing=north` puts a
 * different block in the world than the one that was asked for.
 */
public data class BlockInput(
    val block: Identifier,
    val stateId: Int,
    val properties: Map<String, String>,
) {
    val isAir: Boolean get() = stateId == Blocks.AIR.stateId
}

/** `minecraft:block_state`, as `/setblock`, `/fill` and `/clone` take one. */
public object BlockStateArgument : ArgumentType<BlockInput> {
    override val wire: ArgumentParser = ArgumentParser.BlockState

    override fun parse(
        reader: StringReader,
        source: CommandSource,
        environment: CommandEnvironment,
    ): BlockInput = parseBlock(reader, allowTag = false).asState(reader)
}

/**
 * What `/fill ... replace <filter>` and `/execute if block` match against: a block, a `#tag`, and
 * in either case only the properties that were written, so `oak_log[axis=y]` matches a subset of
 * `oak_log`.
 */
public class BlockPredicateInput(
    private val block: Identifier?,
    private val tag: Identifier?,
    private val properties: Map<String, String>,
    private val tagMembers: (Identifier) -> Set<Identifier>?,
) {
    public fun matches(stateId: Int): Boolean {
        val state = Blocks.byStateId(stateId) ?: return false
        if (block != null && state.block != block) return false
        if (tag != null && state.block !in (tagMembers(tag) ?: return false)) return false
        return properties.all { (key, value) -> state.properties[key] == value }
    }
}

public object BlockPredicateArgument : ArgumentType<BlockPredicateInput> {
    override val wire: ArgumentParser = ArgumentParser.BlockPredicate

    override fun parse(
        reader: StringReader,
        source: CommandSource,
        environment: CommandEnvironment,
    ): BlockPredicateInput {
        val parsed = parseBlock(reader, allowTag = true)
        if (parsed.tag == null) parsed.asState(reader)
        return BlockPredicateInput(
            block = if (parsed.tag == null) parsed.id else null,
            tag = parsed.tag,
            properties = parsed.properties,
            tagMembers = environment.blockTagMembers,
        )
    }
}

internal class ParsedBlock(
    val id: Identifier,
    val tag: Identifier?,
    val properties: Map<String, String>,
    val start: Int,
) {
    /** Resolves to a concrete state, or fails naming what could not be resolved. */
    fun asState(reader: StringReader): BlockInput {
        val stateId =
            Blocks.stateIdOf(id, properties)
                ?: run {
                    reader.cursor = start
                    val written = properties.entries.joinToString(",") { "${it.key}=${it.value}" }
                    throw CommandSyntaxException(
                        if (properties.isEmpty()) {
                            Component.translatable("argument.block.id.invalid", Component.text(id.toString()))
                        } else {
                            Component.translatable(
                                "argument.block.property.unknown",
                                Component.text(id.toString()),
                                Component.text(written),
                            )
                        },
                        reader.string,
                        reader.cursor,
                    )
                }
        return BlockInput(id, stateId, properties)
    }
}

/** parity: `BlockStateParser`, minus its NBT tail, which only block entities use. */
internal fun parseBlock(
    reader: StringReader,
    allowTag: Boolean,
): ParsedBlock {
    val start = reader.cursor
    val isTag = reader.canRead() && reader.peek() == '#'
    if (isTag) {
        if (!allowTag) throw reader.syntaxError("Tags aren't allowed here, only actual blocks")
        reader.skip()
    }
    val name = reader.readIdentifier()
    if (name.isEmpty()) {
        reader.cursor = start
        throw reader.syntaxError("Expected block")
    }
    val id = Identifier.parse(name)
    val properties = mutableMapOf<String, String>()
    if (reader.canRead() && reader.peek() == '[') {
        reader.skip()
        reader.skipWhitespace()
        while (reader.canRead() && reader.peek() != ']') {
            reader.skipWhitespace()
            val key = reader.readUnquotedString()
            reader.skipWhitespace()
            reader.expect('=')
            reader.skipWhitespace()
            val value = reader.readUnquotedString()
            properties[key] = value
            reader.skipWhitespace()
            if (reader.canRead() && reader.peek() == ',') {
                reader.skip()
                continue
            }
            break
        }
        reader.expect(']')
    }
    return ParsedBlock(id, if (isTag) id else null, properties, start)
}

/**
 * `minecraft:item_predicate`: an item id or a `#tag` of them, as `/clear` filters with.
 *
 * Components are not parsed: an item stack here carries only its id, so a predicate that named one
 * could never match anything and is refused by [parseBlock]'s own bracket handling instead.
 */
public class ItemPredicateInput(
    private val item: Identifier?,
    private val tag: Identifier?,
    private val tagMembers: (Identifier) -> Set<Identifier>?,
) {
    public fun matches(itemId: String): Boolean {
        val id = Identifier.parse(itemId)
        if (item != null) return id == item
        return id in (tagMembers(tag ?: return false) ?: return false)
    }
}

public object ItemPredicateArgument : ArgumentType<ItemPredicateInput> {
    override val wire: ArgumentParser = ArgumentParser.ItemPredicate

    override fun parse(
        reader: StringReader,
        source: CommandSource,
        environment: CommandEnvironment,
    ): ItemPredicateInput {
        val parsed = parseBlock(reader, allowTag = true)
        if (parsed.tag == null && Items.REGISTRY.byIdentifier(parsed.id) == null) {
            reader.cursor = parsed.start
            throw CommandSyntaxException(
                Component.translatable("argument.item.id.invalid", Component.text(parsed.id.toString())),
                reader.string,
                reader.cursor,
            )
        }
        return ItemPredicateInput(
            item = if (parsed.tag == null) parsed.id else null,
            tag = parsed.tag,
            tagMembers = environment.itemTagMembers,
        )
    }
}

/** An item id with the components written after it, as `/give` and `/item` take one. */
public data class ParsedItem(
    val id: Identifier,
    val components: NbtCompound = NbtCompound(),
)

/**
 * `minecraft:item_stack`: an item id and, optionally, `[component=value, ...]`.
 *
 * Every component the registry knows is accepted, because every one of them is stored and
 * persisted. The ones this build has no network codec for do not reach the client, and the command
 * says which by name rather than leaving the writer to notice a missing tooltip.
 */
public object ItemArgument : ArgumentType<ParsedItem> {
    override val wire: ArgumentParser = ArgumentParser.ItemStack

    override fun parse(
        reader: StringReader,
        source: CommandSource,
        environment: CommandEnvironment,
    ): ParsedItem {
        val start = reader.cursor
        val id = Identifier.parse(reader.readIdentifier())
        Items.REGISTRY.byIdentifier(id)
            ?: run {
                reader.cursor = start
                throw CommandSyntaxException(
                    Component.translatable("argument.item.id.invalid", Component.text(id.toString())),
                    reader.string,
                    reader.cursor,
                )
            }
        return ParsedItem(id, parseComponents(reader))
    }
}

/** `[a=1, b={c:2}]`, each value SNBT. Keys are normalised to their full id, as the wire writes them. */
internal fun parseComponents(reader: StringReader): NbtCompound {
    val components = NbtCompound()
    if (!reader.canRead() || reader.peek() != '[') return components
    val start = reader.cursor
    reader.skip()
    reader.skipWhitespace()
    while (reader.canRead() && reader.peek() != ']') {
        reader.skipWhitespace()
        val key = Identifier.parse(reader.readIdentifier()).toString()
        reader.skipWhitespace()
        reader.expect('=')
        reader.skipWhitespace()
        if (!ItemComponentCodecs.exists(key)) {
            reader.cursor = start
            throw CommandSyntaxException(
                Component.text("Unknown item component '$key'."),
                reader.string,
                reader.cursor,
            )
        }
        components[key] = parseTag(reader)
        reader.skipWhitespace()
        if (reader.canRead() && reader.peek() == ',') {
            reader.skip()
            continue
        }
        break
    }
    reader.expect(']')
    return components
}

/**
 * `minecraft:component`: a text component written as JSON, which is what `/tellraw` takes.
 *
 * SNBT is accepted by vanilla too since 1.21.5; this reads JSON only, and says so rather than
 * mangling a compound into a string.
 */
public object ComponentArgument : ArgumentType<Component> {
    override val wire: ArgumentParser = ArgumentParser.TextComponent

    override fun parse(
        reader: StringReader,
        source: CommandSource,
        environment: CommandEnvironment,
    ): Component {
        val start = reader.cursor
        val text = reader.remaining
        reader.cursor = reader.string.length
        return runCatching { ComponentJson.deserialize(Json.parseToJsonElement(text)) }
            .getOrElse {
                reader.cursor = start
                throw CommandSyntaxException(
                    Component.translatable("argument.component.invalid", Component.text(it.message.orEmpty())),
                    reader.string,
                    reader.cursor,
                )
            }
    }
}

/** `minecraft:resource_location`: a namespaced id, checked for shape only. */
public object ResourceLocationArgument : ArgumentType<Identifier> {
    override val wire: ArgumentParser = ArgumentParser.ResourceLocation

    override fun parse(
        reader: StringReader,
        source: CommandSource,
        environment: CommandEnvironment,
    ): Identifier {
        val start = reader.cursor
        val text = reader.readIdentifier()
        return runCatching { Identifier.parse(text) }.getOrElse {
            reader.cursor = start
            throw CommandSyntaxException(
                Component.translatable("argument.id.invalid"),
                reader.string,
                reader.cursor,
            )
        }
    }
}

/**
 * `minecraft:score_holder`: a player selector, or a bare name for a holder that is not a player.
 *
 * The bare-name form is the point: `#total`, `$counter` and any other invented name is a legitimate
 * score holder in vanilla, and a datapack that keeps counters relies on it.
 */
public object ScoreHolderArgument : ArgumentType<List<String>> {
    override val wire: ArgumentParser = ArgumentParser.ScoreHolder(multiple = true)

    /** parity: `ScoreHolderArgument.SUGGEST_SCORE_HOLDERS`, which is where fake players come from. */
    override val suggestions: SuggestionProvider =
        SuggestionProvider {
            it.matching(
                it.environment.scoreboard
                    ?.holders()
                    .orEmpty(),
            )
        }

    /** Vanilla flags every node that takes one of these, so the client asks rather than guessing. */
    override val clientSuggestions: String = SuggestionProviders.ASK_SERVER

    override fun parse(
        reader: StringReader,
        source: CommandSource,
        environment: CommandEnvironment,
    ): List<String> {
        if (reader.canRead() && reader.peek() == '@') {
            return PlayersArgument(single = false).parse(reader, source, environment).map { it.name }
        }
        if (reader.canRead() && reader.peek() == '*') {
            reader.skip()
            return environment.players.snapshot().map { it.name }
        }
        val start = reader.cursor
        val name = readHolderName(reader)
        if (name.isEmpty()) {
            reader.cursor = start
            throw reader.syntaxError("Expected score holder")
        }
        return listOf(name)
    }

    /** Vanilla accepts any unquoted word plus the sigils fake players conventionally start with. */
    private fun readHolderName(reader: StringReader): String {
        val start = reader.cursor
        while (reader.canRead() && !reader.peek().isWhitespace()) reader.skip()
        return reader.string.substring(start, reader.cursor)
    }
}

/** `minecraft:operation`: what `/scoreboard players operation` does with the two scores. */
public object OperationArgument : ArgumentType<String> {
    override val wire: ArgumentParser = ArgumentParser.Operation

    private val OPERATIONS = setOf("=", "+=", "-=", "*=", "/=", "%=", "<", ">", "><")

    override fun parse(
        reader: StringReader,
        source: CommandSource,
        environment: CommandEnvironment,
    ): String {
        val start = reader.cursor
        while (reader.canRead() && !reader.peek().isWhitespace()) reader.skip()
        val text = reader.string.substring(start, reader.cursor)
        if (text !in OPERATIONS) {
            reader.cursor = start
            throw CommandSyntaxException(
                Component.translatable("arguments.operation.invalid"),
                reader.string,
                reader.cursor,
            )
        }
        return text
    }
}

/** `minecraft:vec2`: an `x z` pair of doubles, which is what `/worldborder center` takes. */
public object Vec2Argument : ArgumentType<Coordinates> {
    override val wire: ArgumentParser = ArgumentParser.Vec2

    override fun parse(
        reader: StringReader,
        source: CommandSource,
        environment: CommandEnvironment,
    ): Coordinates {
        val start = reader.cursor
        val x = WorldCoordinate.parse(reader, centerCorrect = false)
        if (!reader.canRead() || reader.peek() != ' ') {
            reader.cursor = start
            throw CommandSyntaxException(Component.translatable("argument.pos2d.incomplete"), reader.string, start)
        }
        reader.skip()
        val z = WorldCoordinate.parse(reader, centerCorrect = false)
        return Coordinates(x, WorldCoordinate(relative = false, value = 0.0), z)
    }
}

/**
 * `minecraft:nbt_compound_tag`: an SNBT compound, consumed exactly as far as it goes.
 *
 * Declaring the real parser matters beyond parsing: the client validates the argument locally with
 * the same one, so a compound declared as a greedy string is drawn as an error even when the server
 * accepts it.
 */
public object NbtCompoundArgument : ArgumentType<NbtCompound> {
    override val wire: ArgumentParser = ArgumentParser.NbtCompound

    override fun parse(
        reader: StringReader,
        source: CommandSource,
        environment: CommandEnvironment,
    ): NbtCompound =
        parseTag(reader) as? NbtCompound
            ?: throw reader.syntaxError("Expected a compound")
}

/** `minecraft:nbt_tag`: any SNBT value, which is what `/data modify ... value` takes. */
public object NbtTagArgument : ArgumentType<NbtTag> {
    override val wire: ArgumentParser = ArgumentParser.NbtTag

    override fun parse(
        reader: StringReader,
        source: CommandSource,
        environment: CommandEnvironment,
    ): NbtTag = parseTag(reader)
}

private fun parseTag(reader: StringReader): NbtTag {
    val start = reader.cursor
    val (tag, end) =
        runCatching { Snbt.parseFrom(reader.string, start) }
            .getOrElse {
                reader.cursor = start
                throw reader.syntaxError(it.message ?: "Invalid NBT")
            }
    reader.cursor = end
    return tag
}

/**
 * `minecraft:nbt_path`: everything up to the next space, so `crew[{name:"bo"}].age` is one
 * argument. A quoted section may contain a space, which is why quotes are tracked rather than
 * splitting on the first one.
 */
public object NbtPathArgument : ArgumentType<String> {
    override val wire: ArgumentParser = ArgumentParser.NbtPath

    override fun parse(
        reader: StringReader,
        source: CommandSource,
        environment: CommandEnvironment,
    ): String {
        val start = reader.cursor
        var quoted = false
        while (reader.canRead()) {
            val next = reader.peek()
            if (next == '"') quoted = !quoted
            if (next == ' ' && !quoted) break
            reader.skip()
        }
        val text = reader.string.substring(start, reader.cursor)
        if (text.isEmpty()) throw reader.syntaxError("Expected an NBT path")
        return text
    }
}
