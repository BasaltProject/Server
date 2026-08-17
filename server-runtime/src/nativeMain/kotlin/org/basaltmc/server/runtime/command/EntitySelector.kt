package org.basaltmc.server.runtime.command

import org.basaltmc.server.registry.EntityTypes
import org.basaltmc.server.runtime.world.LoadedWorld
import org.basaltmc.wire.common.Identifier
import org.basaltmc.wire.common.IdentifierFormatException
import org.basaltmc.wire.nbt.NbtCompound
import org.basaltmc.wire.nbt.Snbt
import org.basaltmc.wire.protocol.play.Gamemode
import org.basaltmc.wire.text.Component
import kotlin.math.sqrt

/** A `min..max` bound as a selector writes it. An absent end does not bound that side. */
public data class SelectorRange<T : Comparable<T>>(
    public val min: T?,
    public val max: T?,
) {
    public fun matches(value: T): Boolean = (min == null || value >= min) && (max == null || value <= max)
}

/** parity: the four `sort=` values, which is also what each selector letter starts with. */
public enum class SelectorOrder {
    ARBITRARY,
    NEAREST,
    FURTHEST,
    RANDOM,
}

/**
 * A parsed `@a[...]`, `@e[...]`, a plain player name or a UUID: what to look for and how many.
 *
 * parity: `EntitySelector` plus `EntitySelectorParser`, split the same way because the split is
 * observable: an unknown option is a syntax error while parsing, not an empty result later.
 *
 * Two answers differ from vanilla for want of data, not by choice: `dx/dy/dz` tests a real box for
 * players and a point for summoned entities (no dimensions table), and `nbt=` compares against the
 * partial entity view [targetNbt] exposes.
 */
public class EntitySelector internal constructor(
    private val maxResults: Int,
    private val includesEntities: Boolean,
    private val worldLimited: Boolean,
    private val predicates: List<(CommandTarget) -> Boolean>,
    private val distance: SelectorRange<Double>?,
    private val anchorX: Double?,
    private val anchorY: Double?,
    private val anchorZ: Double?,
    private val deltaX: Double?,
    private val deltaY: Double?,
    private val deltaZ: Double?,
    private val order: SelectorOrder,
    private val currentEntity: Boolean,
    private val playerName: String?,
    private val entityUuid: String?,
    private val typeLimit: Identifier?,
) {
    /**
     * Everything this selector matches right now.
     *
     * [playersOnly] is the argument type's own restriction (`minecraft:entity` with `playersOnly`),
     * not the selector's: `@e` handed to `/kick` matches players and nothing else, exactly as
     * vanilla's `findPlayers` does with the same selector.
     */
    public fun select(
        source: CommandSource,
        environment: CommandEnvironment,
        playersOnly: Boolean,
    ): List<CommandTarget> {
        val withEntities = includesEntities && !playersOnly
        playerName?.let { name ->
            return environment.players
                .snapshot()
                .filter { it.name.equals(name, ignoreCase = true) }
                .map { CommandTarget.Player(it) }
        }
        entityUuid?.let { uuid ->
            environment.players.snapshot().firstOrNull { it.uuid.toString() == uuid }?.let {
                return listOf(CommandTarget.Player(it))
            }
            if (!withEntities) return emptyList()
            return entitiesIn(environment, null).filter { it.uuid == uuid }.take(1)
        }

        val (sourceX, sourceY, sourceZ) = source.position
        val x = anchorX ?: sourceX
        val y = anchorY ?: sourceY
        val z = anchorZ ?: sourceZ

        if (currentEntity) {
            // `/execute as` may have set something that is not a player, and `@s` is whoever it set.
            val self =
                (source as? ExecutionSource)?.entity
                    ?: source.player?.let { CommandTarget.Player(it) }
                    ?: return emptyList()
            return if (matches(self, x, y, z)) listOf(self) else emptyList()
        }

        val world = if (worldLimited) source.worldIn(environment) else null
        val candidates = mutableListOf<CommandTarget>()
        for (player in environment.players.snapshot()) {
            if (world != null && player.worldName != world.name) continue
            candidates += CommandTarget.Player(player)
        }
        if (withEntities) candidates += entitiesIn(environment, world)
        return sortAndLimit(candidates.filter { matches(it, x, y, z) }, x, y, z)
    }

    private fun entitiesIn(
        environment: CommandEnvironment,
        world: LoadedWorld?,
    ): List<CommandTarget> {
        val worlds = if (world != null) listOf(world) else environment.worlds.values
        val result = mutableListOf<CommandTarget>()
        for (loaded in worlds) {
            for (entity in loaded.state.entities()) {
                if (entity.removed) continue
                result += CommandTarget.Entity(entity, loaded.name)
            }
        }
        return result
    }

    private fun matches(
        target: CommandTarget,
        x: Double,
        y: Double,
        z: Double,
    ): Boolean {
        if (typeLimit != null && target.type != typeLimit) return false
        if (!withinBox(target, x, y, z)) return false
        if (distance != null && !distance.matches(distanceTo(target, x, y, z))) return false
        return predicates.all { it(target) }
    }

    /** parity: `AABB.intersects`, with the box built by `EntitySelectorParser.createAabb` and moved to the anchor. */
    private fun withinBox(
        target: CommandTarget,
        x: Double,
        y: Double,
        z: Double,
    ): Boolean {
        if (deltaX == null && deltaY == null && deltaZ == null) return true
        val dx = deltaX ?: 0.0
        val dy = deltaY ?: 0.0
        val dz = deltaZ ?: 0.0
        val minX = x + if (dx < 0) dx else 0.0
        val minY = y + if (dy < 0) dy else 0.0
        val minZ = z + if (dz < 0) dz else 0.0
        val maxX = x + (if (dx < 0) 0.0 else dx) + 1.0
        val maxY = y + (if (dy < 0) 0.0 else dy) + 1.0
        val maxZ = z + (if (dz < 0) 0.0 else dz) + 1.0
        // A target this build has no dimensions for is a point, and vanilla's strict intersection
        // test can never contain one, so it is asked the containment question instead.
        if (target.width == 0.0 && target.height == 0.0) {
            return target.x in minX..maxX && target.y in minY..maxY && target.z in minZ..maxZ
        }
        val halfWidth = target.width / 2.0
        return minX < target.x + halfWidth &&
            maxX > target.x - halfWidth &&
            minY < target.y + target.height &&
            maxY > target.y &&
            minZ < target.z + halfWidth &&
            maxZ > target.z - halfWidth
    }

    private fun distanceTo(
        target: CommandTarget,
        x: Double,
        y: Double,
        z: Double,
    ): Double {
        val dx = target.x - x
        val dy = target.y - y
        val dz = target.z - z
        return sqrt(dx * dx + dy * dy + dz * dz)
    }

    private fun sortAndLimit(
        matched: List<CommandTarget>,
        x: Double,
        y: Double,
        z: Double,
    ): List<CommandTarget> {
        if (matched.size <= 1) return matched
        val ordered =
            when (order) {
                SelectorOrder.ARBITRARY -> matched
                SelectorOrder.NEAREST -> matched.sortedBy { distanceTo(it, x, y, z) }
                SelectorOrder.FURTHEST -> matched.sortedByDescending { distanceTo(it, x, y, z) }
                SelectorOrder.RANDOM -> matched.shuffled()
            }
        return if (ordered.size <= maxResults) ordered else ordered.take(maxResults)
    }

    public companion object {
        /** Parses one selector, a player name or a UUID. What it may match is decided later, by [select]. */
        public fun parse(
            reader: StringReader,
            source: CommandSource,
            environment: CommandEnvironment,
        ): EntitySelector = SelectorParser(reader, source, environment).parse()
    }
}

/**
 * The bracket syntax, one option at a time. parity: `EntitySelectorParser.parseOptions` and
 * `EntitySelectorOptions`, error keys included: the client shows them, so they are specification.
 *
 * The "can this option appear again" rules are vanilla's own and are not cosmetic:
 * `type=zombie,type=creeper` can never match, so it is refused, while `type=!zombie,type=!creeper`
 * is fine. Same for `name`, `gamemode` and `team`.
 */
private class SelectorParser(
    private val reader: StringReader,
    private val source: CommandSource,
    private val environment: CommandEnvironment,
) {
    private var maxResults = 0
    private var includesEntities = false
    private var worldLimited = false
    private var distance: SelectorRange<Double>? = null
    private var level: SelectorRange<Int>? = null
    private var rotX: SelectorRange<Double>? = null
    private var rotY: SelectorRange<Double>? = null
    private var anchorX: Double? = null
    private var anchorY: Double? = null
    private var anchorZ: Double? = null
    private var deltaX: Double? = null
    private var deltaY: Double? = null
    private var deltaZ: Double? = null
    private var order = SelectorOrder.ARBITRARY
    private var currentEntity = false
    private var playerName: String? = null
    private var entityUuid: String? = null
    private var typeLimit: Identifier? = null
    private val predicates = mutableListOf<(CommandTarget) -> Boolean>()

    private val nameOption = InvertableOption()
    private val gamemodeOption = InvertableOption()
    private val teamOption = InvertableOption()
    private val typeOption = InvertableOption()
    private var limitSet = false
    private var sortSet = false
    private var scoresSet = false
    private var advancementsSet = false

    fun parse(): EntitySelector {
        if (reader.canRead() && reader.peek() == '@') {
            reader.skip()
            parseSelector()
        } else {
            parseNameOrUuid()
        }
        finalizePredicates()
        return EntitySelector(
            maxResults = maxResults,
            includesEntities = includesEntities,
            worldLimited = worldLimited,
            predicates = predicates.toList(),
            distance = distance,
            anchorX = anchorX,
            anchorY = anchorY,
            anchorZ = anchorZ,
            deltaX = deltaX,
            deltaY = deltaY,
            deltaZ = deltaZ,
            order = order,
            currentEntity = currentEntity,
            playerName = playerName,
            entityUuid = entityUuid,
            typeLimit = typeLimit,
        )
    }

    private fun parseSelector() {
        if (!reader.canRead()) throw error("argument.entity.selector.missing")
        val start = reader.cursor
        val letter = reader.read()
        val livingOnly =
            when (letter) {
                'a' -> {
                    maxResults = Int.MAX_VALUE
                    typeLimit = EntityTypes.PLAYER
                    false
                }
                'e' -> {
                    maxResults = Int.MAX_VALUE
                    includesEntities = true
                    true
                }
                'n' -> {
                    maxResults = 1
                    includesEntities = true
                    order = SelectorOrder.NEAREST
                    true
                }
                'p' -> {
                    maxResults = 1
                    order = SelectorOrder.NEAREST
                    typeLimit = EntityTypes.PLAYER
                    false
                }
                'r' -> {
                    maxResults = 1
                    order = SelectorOrder.RANDOM
                    typeLimit = EntityTypes.PLAYER
                    false
                }
                's' -> {
                    maxResults = 1
                    includesEntities = true
                    currentEntity = true
                    false
                }
                else -> {
                    reader.cursor = start
                    throw error("argument.entity.selector.unknown", "@$letter")
                }
            }
        if (livingOnly) predicates += { it.alive }
        if (reader.canRead() && reader.peek() == '[') {
            reader.skip()
            parseOptions()
        }
    }

    /** parity: a bare word is a player name, unless it parses as a UUID, and never an entity's custom name. */
    private fun parseNameOrUuid() {
        val start = reader.cursor
        val name = reader.readString()
        if (UUID_LENGTH == name.length && name.count { it == '-' } == UUID_DASHES) {
            entityUuid = name
            includesEntities = true
        } else {
            if (name.isEmpty() || name.length > MAX_PLAYER_NAME) {
                reader.cursor = start
                throw error("argument.entity.invalid")
            }
            playerName = name
        }
        maxResults = 1
    }

    private fun parseOptions() {
        reader.skipWhitespace()
        while (reader.canRead() && reader.peek() != ']') {
            reader.skipWhitespace()
            val start = reader.cursor
            val key = reader.readString()
            reader.skipWhitespace()
            if (!reader.canRead() || reader.peek() != '=') {
                reader.cursor = start
                throw error("argument.entity.options.valueless", key)
            }
            reader.skip()
            reader.skipWhitespace()
            readOption(key, start)
            reader.skipWhitespace()
            if (reader.canRead()) {
                if (reader.peek() != ',') {
                    if (reader.peek() != ']') throw error("argument.entity.options.unterminated")
                    break
                }
                reader.skip()
            }
        }
        if (!reader.canRead()) throw error("argument.entity.options.unterminated")
        reader.skip()
    }

    private fun readOption(
        key: String,
        start: Int,
    ) {
        when (key) {
            "name" -> readName(start)
            "distance" -> readDistance(start)
            "level" -> readLevel(start)
            "x" -> anchorX = readAnchor(start, key, anchorX)
            "y" -> anchorY = readAnchor(start, key, anchorY)
            "z" -> anchorZ = readAnchor(start, key, anchorZ)
            "dx" -> deltaX = readAnchor(start, key, deltaX)
            "dy" -> deltaY = readAnchor(start, key, deltaY)
            "dz" -> deltaZ = readAnchor(start, key, deltaZ)
            "x_rotation" -> rotX = readRotation(start, rotX, key)
            "y_rotation" -> rotY = readRotation(start, rotY, key)
            "limit" -> readLimit(start)
            "sort" -> readSort(start)
            "gamemode" -> readGamemode(start)
            "team" -> readTeam(start)
            "type" -> readType(start)
            "tag" -> readTag()
            "nbt" -> readNbt()
            "scores" -> readScores(start)
            "advancements" -> readAdvancements(start)
            "predicate" -> readPredicate()
            else -> {
                reader.cursor = start
                throw error("argument.entity.options.unknown", key)
            }
        }
    }

    private fun readName(start: Int) {
        val inverted = shouldInvert()
        val name = reader.readString()
        if (!nameOption.canParse(inverted)) throw inapplicable(start, "name")
        nameOption.mark(inverted)
        predicates += { (it.name == name) != inverted }
    }

    private fun readDistance(start: Int) {
        if (distance != null) throw inapplicable(start, "distance")
        val range = readDoubleRange()
        if ((range.min ?: 0.0) < 0.0 || (range.max ?: 0.0) < 0.0) {
            reader.cursor = start
            throw error("argument.entity.options.distance.negative")
        }
        distance = range
        worldLimited = true
    }

    private fun readLevel(start: Int) {
        if (level != null) throw inapplicable(start, "level")
        val range = readIntRange()
        if ((range.min ?: 0) < 0 || (range.max ?: 0) < 0) {
            reader.cursor = start
            throw error("argument.entity.options.level.negative")
        }
        level = range
        includesEntities = false
    }

    private fun readAnchor(
        start: Int,
        key: String,
        current: Double?,
    ): Double {
        if (current != null) throw inapplicable(start, key)
        worldLimited = true
        return reader.readDouble()
    }

    private fun readRotation(
        start: Int,
        current: SelectorRange<Double>?,
        key: String,
    ): SelectorRange<Double> {
        if (current != null) throw inapplicable(start, key)
        return readRotationRange()
    }

    private fun readLimit(start: Int) {
        if (currentEntity || limitSet) throw inapplicable(start, "limit")
        val cursor = reader.cursor
        val count = reader.readInt()
        if (count < 1) {
            reader.cursor = cursor
            throw error("argument.entity.options.limit.toosmall")
        }
        maxResults = count
        limitSet = true
    }

    private fun readSort(start: Int) {
        if (currentEntity || sortSet) throw inapplicable(start, "sort")
        val cursor = reader.cursor
        val name = reader.readUnquotedString()
        order =
            when (name) {
                "nearest" -> SelectorOrder.NEAREST
                "furthest" -> SelectorOrder.FURTHEST
                "random" -> SelectorOrder.RANDOM
                "arbitrary" -> SelectorOrder.ARBITRARY
                else -> {
                    reader.cursor = cursor
                    throw error("argument.entity.options.sort.irreversible", name)
                }
            }
        sortSet = true
    }

    private fun readGamemode(start: Int) {
        val inverted = shouldInvert()
        if (!gamemodeOption.canParse(inverted)) throw inapplicable(start, "gamemode")
        val cursor = reader.cursor
        val name = reader.readUnquotedString()
        val expected =
            Gamemode.entries.firstOrNull { it.name.equals(name, ignoreCase = true) }
                ?: run {
                    reader.cursor = cursor
                    throw error("argument.entity.options.mode.invalid", name)
                }
        includesEntities = false
        gamemodeOption.mark(inverted)
        predicates += { target -> ((target as? CommandTarget.Player)?.session?.gamemode == expected) != inverted }
    }

    private fun readTeam(start: Int) {
        val inverted = shouldInvert()
        val expected = reader.readUnquotedString()
        if (!teamOption.canParse(inverted)) throw inapplicable(start, "team")
        teamOption.mark(inverted)
        val teams = environment.teams
        predicates += { target -> ((teams?.teamOf(target.scoreboardName)?.name ?: "") == expected) != inverted }
    }

    private fun readType(start: Int) {
        val inverted = shouldInvert()
        if (reader.canRead() && reader.peek() == '#') {
            reader.skip()
            val tag = readIdentifier(start)
            if (!typeOption.canParseTag(tag)) throw inapplicable(start, "type")
            typeOption.markTag(tag)
            val members = environment.entityTagMembers(tag).orEmpty()
            predicates += { (it.type in members) != inverted }
            return
        }
        if (!typeOption.canParse(inverted)) throw inapplicable(start, "type")
        val id = readIdentifier(start)
        if (EntityTypes[id] == null) {
            reader.cursor = start
            throw error("argument.entity.options.type.invalid", id.toString())
        }
        if (id == EntityTypes.PLAYER && !inverted) includesEntities = false
        if (!inverted) typeLimit = id
        typeOption.mark(inverted)
        predicates += { (it.type == id) != inverted }
    }

    private fun readTag() {
        val inverted = shouldInvert()
        val tag = reader.readUnquotedString()
        predicates += { target ->
            val has = if (tag.isEmpty()) target.tags.isEmpty() else tag in target.tags
            has != inverted
        }
    }

    private fun readNbt() {
        val inverted = shouldInvert()
        val start = reader.cursor
        val (tag, end) =
            runCatching { Snbt.parseFrom(reader.string, start) }.getOrElse {
                reader.cursor = start
                throw reader.syntaxError(it.message ?: "Invalid NBT")
            }
        reader.cursor = end
        val expected = tag as? NbtCompound ?: throw reader.syntaxError("Expected a compound")
        predicates += { containsNbt(targetNbt(it), expected) != inverted }
    }

    private fun readScores(start: Int) {
        if (scoresSet) throw inapplicable(start, "scores")
        val expected = mutableMapOf<String, SelectorRange<Int>>()
        reader.expect('{')
        reader.skipWhitespace()
        while (reader.canRead() && reader.peek() != '}') {
            reader.skipWhitespace()
            val objective = reader.readUnquotedString()
            reader.skipWhitespace()
            reader.expect('=')
            reader.skipWhitespace()
            expected[objective] = readIntRange()
            reader.skipWhitespace()
            if (reader.canRead() && reader.peek() == ',') reader.skip()
        }
        reader.expect('}')
        scoresSet = true
        if (expected.isEmpty()) return
        val scoreboard = environment.scoreboard
        predicates += { target ->
            expected.all { (objective, range) ->
                val value = scoreboard?.score(target.scoreboardName, objective)
                value != null && range.matches(value)
            }
        }
    }

    private fun readAdvancements(start: Int) {
        if (advancementsSet) throw inapplicable(start, "advancements")
        val expected = mutableMapOf<Identifier, Map<String, Boolean>?>()
        val whole = mutableMapOf<Identifier, Boolean>()
        reader.expect('{')
        reader.skipWhitespace()
        while (reader.canRead() && reader.peek() != '}') {
            reader.skipWhitespace()
            val id = readIdentifier(reader.cursor)
            reader.skipWhitespace()
            reader.expect('=')
            reader.skipWhitespace()
            if (reader.canRead() && reader.peek() == '{') {
                reader.skip()
                val criteria = mutableMapOf<String, Boolean>()
                reader.skipWhitespace()
                while (reader.canRead() && reader.peek() != '}') {
                    reader.skipWhitespace()
                    val criterion = reader.readUnquotedString()
                    reader.skipWhitespace()
                    reader.expect('=')
                    reader.skipWhitespace()
                    criteria[criterion] = readBoolean()
                    reader.skipWhitespace()
                    if (reader.canRead() && reader.peek() == ',') reader.skip()
                }
                reader.skipWhitespace()
                reader.expect('}')
                expected[id] = criteria
            } else {
                whole[id] = readBoolean()
                expected[id] = null
            }
            reader.skipWhitespace()
            if (reader.canRead() && reader.peek() == ',') reader.skip()
        }
        reader.expect('}')
        advancementsSet = true
        if (expected.isEmpty()) return
        includesEntities = false
        val advancements = environment.advancements
        val progress = environment.progress
        predicates += { target ->
            target is CommandTarget.Player &&
                expected.all { (id, criteria) ->
                    val advancement = advancements?.get(id)
                    val granted = progress?.grantedCriteria(target.uuid, id).orEmpty()
                    when {
                        advancement == null -> false
                        criteria != null -> criteria.all { (name, done) -> (name in granted) == done }
                        else -> advancement.isComplete(granted) == (whole[id] == true)
                    }
                }
        }
    }

    /**
     * `predicate=`, which resolves against the `minecraft:predicate` registry. Vanilla ships none
     * of its own (they are datapack content), and an unknown predicate is false there even when
     * negated, so with no datapack loaded nothing matches either way.
     */
    private fun readPredicate() {
        shouldInvert()
        readIdentifier(reader.cursor)
        predicates += { false }
    }

    private fun finalizePredicates() {
        rotX?.let { range -> predicates += { rotationMatches(range, it.pitch) } }
        rotY?.let { range -> predicates += { rotationMatches(range, it.yaw) } }
        level?.let { range ->
            predicates += { target ->
                target is CommandTarget.Player && range.matches(target.session.experience.level)
            }
        }
    }

    /** parity: a wrapped range, so `y_rotation=170..-170` is the arc that crosses south. */
    private fun rotationMatches(
        range: SelectorRange<Double>,
        value: Float,
    ): Boolean {
        val min = wrapDegrees(range.min ?: 0.0)
        val max = wrapDegrees(range.max ?: DEFAULT_ROTATION_MAX)
        val rotation = wrapDegrees(value.toDouble())
        return if (min > max) rotation >= min || rotation <= max else rotation in min..max
    }

    private fun wrapDegrees(value: Double): Double {
        var wrapped = value % FULL_TURN
        if (wrapped >= HALF_TURN) wrapped -= FULL_TURN
        if (wrapped < -HALF_TURN) wrapped += FULL_TURN
        return wrapped
    }

    private fun shouldInvert(): Boolean {
        reader.skipWhitespace()
        if (reader.canRead() && reader.peek() == '!') {
            reader.skip()
            reader.skipWhitespace()
            return true
        }
        return false
    }

    private fun readBoolean(): Boolean =
        when (val word = reader.readUnquotedString()) {
            "true" -> true
            "false" -> false
            else -> throw reader.syntaxError("Expected true or false, got '$word'")
        }

    private fun readIdentifier(start: Int): Identifier =
        runCatching { Identifier.parse(reader.readIdentifier()) }.getOrElse {
            reader.cursor = start
            throw reader.syntaxError((it as? IdentifierFormatException)?.message ?: "Invalid identifier")
        }

    private fun readDoubleRange(): SelectorRange<Double> = readRange(rejectSwapped = true) { it.toDoubleOrNull() }

    private fun readIntRange(): SelectorRange<Int> = readRange(rejectSwapped = true) { it.toIntOrNull() }

    /**
     * parity: `MinMaxBounds.FloatDegrees.fromReader`, which unlike the other two does **not** reject
     * a swapped range: `y_rotation=160..-160` is the arc through south, not a mistake.
     */
    private fun readRotationRange(): SelectorRange<Double> = readRange(rejectSwapped = false) { it.toDoubleOrNull() }

    /** parity: `MinMaxBounds.Bounds.fromReader`, including `..` as the only separator and both ends optional. */
    private fun <T : Comparable<T>> readRange(
        rejectSwapped: Boolean,
        convert: (String) -> T?,
    ): SelectorRange<T> {
        if (!reader.canRead()) throw error("argument.range.empty")
        val start = reader.cursor
        val minText = readRangeNumber()
        val min = if (minText.isEmpty()) null else convert(minText) ?: rangeNumberError(start, minText)
        val max: T?
        if (reader.canRead(2) && reader.peek() == '.' && reader.peek(1) == '.') {
            reader.skip()
            reader.skip()
            val maxText = readRangeNumber()
            max = if (maxText.isEmpty()) null else convert(maxText) ?: rangeNumberError(start, maxText)
        } else {
            max = min
        }
        if (min == null && max == null) {
            reader.cursor = start
            throw error("argument.range.empty")
        }
        if (rejectSwapped && min != null && max != null && min > max) {
            reader.cursor = start
            throw error("argument.range.swapped")
        }
        return SelectorRange(min, max)
    }

    private fun rangeNumberError(
        start: Int,
        text: String,
    ): Nothing {
        reader.cursor = start
        throw reader.syntaxError("Invalid number '$text'")
    }

    /** Digits, a sign and a decimal point, but never the `.` that opens the `..` separator. */
    private fun readRangeNumber(): String {
        val start = reader.cursor
        while (reader.canRead()) {
            val next = reader.peek()
            val allowed =
                when {
                    next in '0'..'9' || next == '-' -> true
                    next == '.' -> !reader.canRead(2) || reader.peek(1) != '.'
                    else -> false
                }
            if (!allowed) break
            reader.skip()
        }
        return reader.string.substring(start, reader.cursor)
    }

    private fun inapplicable(
        start: Int,
        key: String,
    ): CommandSyntaxException {
        reader.cursor = start
        return error("argument.entity.options.inapplicable", key)
    }

    private fun error(
        key: String,
        argument: String? = null,
    ): CommandSyntaxException {
        val message =
            if (argument == null) {
                Component.translatable(key)
            } else {
                Component.translatable(key, Component.text(argument))
            }
        return CommandSyntaxException(message, reader.string, reader.cursor)
    }

    private companion object {
        const val UUID_LENGTH = 36
        const val UUID_DASHES = 4
        const val MAX_PLAYER_NAME = 16
        const val FULL_TURN = 360.0
        const val HALF_TURN = 180.0
        const val DEFAULT_ROTATION_MAX = 359.0
    }
}

/**
 * How often an option may appear, and with which sign. parity: `InvertableSetOptionState`.
 *
 * One positive value settles the question, so nothing else may follow it; negated values only
 * remove candidates, so any number of them is fine.
 */
private class InvertableOption {
    private var single = false
    private var multiple = false
    private val tags = mutableSetOf<Identifier>()

    fun canParse(inverted: Boolean): Boolean = if (inverted) !single else !single && !multiple

    fun canParseTag(tag: Identifier): Boolean = !single && tag !in tags

    fun mark(inverted: Boolean) {
        if (inverted) multiple = true else single = true
    }

    fun markTag(tag: Identifier) {
        multiple = true
        tags += tag
    }
}

/** Whether [actual] carries everything [expected] asks for. parity: `NbtUtils.compareNbt` with lists ordered. */
private fun containsNbt(
    actual: NbtCompound,
    expected: NbtCompound,
): Boolean =
    expected.keys.all { key ->
        val wanted = expected[key] ?: return@all false
        val present = actual[key] ?: return@all false
        if (wanted is NbtCompound && present is NbtCompound) containsNbt(present, wanted) else wanted == present
    }

/** What `nbt=` compares against: the same partial entity view `/data get entity` answers with. */
private fun targetNbt(target: CommandTarget): NbtCompound =
    when (target) {
        is CommandTarget.Player -> EntityNbt.of(target.session)
        is CommandTarget.Entity -> EntityNbt.of(target.entity)
    }
