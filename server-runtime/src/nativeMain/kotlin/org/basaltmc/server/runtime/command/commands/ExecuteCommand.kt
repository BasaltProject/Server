package org.basaltmc.server.runtime.command.commands

import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.registry.EntityTypes
import org.basaltmc.server.runtime.command.BlockPosArgument
import org.basaltmc.server.runtime.command.BlockPredicateArgument
import org.basaltmc.server.runtime.command.BlockPredicateInput
import org.basaltmc.server.runtime.command.Command
import org.basaltmc.server.runtime.command.CommandContext
import org.basaltmc.server.runtime.command.CommandNode
import org.basaltmc.server.runtime.command.CommandNodeBuilder
import org.basaltmc.server.runtime.command.CommandSource
import org.basaltmc.server.runtime.command.CommandTarget
import org.basaltmc.server.runtime.command.Coordinates
import org.basaltmc.server.runtime.command.DoubleArgument
import org.basaltmc.server.runtime.command.EntitiesArgument
import org.basaltmc.server.runtime.command.EntityAnchor
import org.basaltmc.server.runtime.command.EntityAnchorArgument
import org.basaltmc.server.runtime.command.ExecutionSource
import org.basaltmc.server.runtime.command.FloatRangeArgument
import org.basaltmc.server.runtime.command.HeightmapArgument
import org.basaltmc.server.runtime.command.IntRangeArgument
import org.basaltmc.server.runtime.command.ItemPredicateArgument
import org.basaltmc.server.runtime.command.ItemSlotsArgument
import org.basaltmc.server.runtime.command.LootPredicateArgument
import org.basaltmc.server.runtime.command.NbtPathArgument
import org.basaltmc.server.runtime.command.ObjectiveArgument
import org.basaltmc.server.runtime.command.PermissionLevel
import org.basaltmc.server.runtime.command.RegistryArgument
import org.basaltmc.server.runtime.command.ResourceLocationArgument
import org.basaltmc.server.runtime.command.RotationArgument
import org.basaltmc.server.runtime.command.ScoreHolderArgument
import org.basaltmc.server.runtime.command.SuggestionProviders
import org.basaltmc.server.runtime.command.SwizzleArgument
import org.basaltmc.server.runtime.command.Vec3Argument
import org.basaltmc.server.runtime.command.WorldArgument
import org.basaltmc.server.runtime.command.worldIn
import org.basaltmc.server.runtime.entity.ServerEntity
import org.basaltmc.server.runtime.player.PlayerRegistry
import org.basaltmc.server.runtime.scoreboard.Scoreboard
import org.basaltmc.server.runtime.world.LoadedWorld
import org.basaltmc.server.world.Chunk
import org.basaltmc.wire.common.Identifier
import org.basaltmc.wire.text.Component
import kotlin.math.atan2
import kotlin.math.floor
import kotlin.math.sqrt
import kotlin.uuid.ExperimentalUuidApi
import kotlin.uuid.Uuid

/**
 * parity: `ExecuteCommand`, declared as the tree vanilla declares rather than as one greedy string.
 *
 * Each link redirects back onto `/execute` with a modifier, so the client parses and completes the
 * whole chain and every link keeps its own arguments. `run` redirects onto the dispatcher root,
 * which is what makes the rest of the line a command in its own right.
 *
 * Declared but inert, each for want of the system underneath: `on` (no entity relations), `if
 * predicate`, `if items` and `if stopwatch`. They parse and fail rather than being missing from the
 * tree, so a line that uses one reads as false instead of as a syntax error.
 */
public class ExecuteCommand(
    private val scoreboard: Scoreboard,
    /** Where `store ... bossbar` writes, or null in a fixture with no bars. */
    private val bossBars: BossBarRegistry? = null,
    /** Where `store ... storage` writes and `if data` reads, or null in a fixture without it. */
    private val data: DataAccess? = null,
    /** What `if function` runs, or null where no functions are loaded. */
    private val functions: FunctionRunner? = null,
    /** Where `summon` gets an entity id, or null in a fixture that cannot spawn. */
    private val players: PlayerRegistry? = null,
) : Command(name = "execute", permissionLevel = PermissionLevel.GAMEMASTERS) {
    override fun build(root: CommandNodeBuilder) {
        val execute = root.node

        root.literal("run") { redirectTo(root.root) }
        buildContextClauses(root, execute)
        for (positive in listOf(true, false)) {
            root.literal(if (positive) "if" else "unless") { buildConditions(this, execute, positive) }
        }
        root.literal("store") {
            for (fromSuccess in listOf(false, true)) {
                literal(if (fromSuccess) "success" else "result") { buildStores(this, execute, fromSuccess) }
            }
        }
    }

    /** The clauses that only change the source: who, where, facing what, in which world. */
    private fun buildContextClauses(
        root: CommandNodeBuilder,
        execute: CommandNode,
    ) {
        root.literal("as") {
            argument("targets", EntitiesArgument()) {
                redirectTo(execute, forks = true) { context ->
                    context.targets("targets").map { target ->
                        ExecutionSource(context.source, player = target.playerOrNull(), entity = target)
                    }
                }
            }
        }
        root.literal("at") {
            argument("targets", EntitiesArgument()) {
                redirectTo(execute, forks = true) { context ->
                    context.targets("targets").map { target -> context.source.at(target) }
                }
            }
        }
        root.literal("positioned") {
            argument("pos", Vec3Argument) {
                redirectTo(execute) { context ->
                    val position = context.get<Coordinates>("pos").resolve(context.source.position)
                    listOf(ExecutionSource(context.source, position = position))
                }
            }
            literal("as") {
                argument("targets", EntitiesArgument()) {
                    redirectTo(execute, forks = true) { context ->
                        context.targets("targets").map { target ->
                            ExecutionSource(context.source, position = Triple(target.x, target.y, target.z))
                        }
                    }
                }
            }
            literal("over") {
                argument("heightmap", HeightmapArgument) {
                    redirectTo(execute) { context -> listOf(context.over(context.get("heightmap"))) }
                }
            }
        }
        root.literal("rotated") {
            argument("rot", RotationArgument) {
                redirectTo(execute) { context ->
                    listOf(ExecutionSource(context.source, rotation = context.get("rot")))
                }
            }
            literal("as") {
                argument("targets", EntitiesArgument()) {
                    redirectTo(execute, forks = true) { context ->
                        context.targets("targets").map { target ->
                            ExecutionSource(context.source, rotation = target.yaw to target.pitch)
                        }
                    }
                }
            }
        }
        root.literal("facing") {
            argument("pos", Vec3Argument) {
                redirectTo(execute) { context ->
                    val (x, y, z) = context.get<Coordinates>("pos").resolve(context.source.position)
                    listOf(context.source.facing(x, y, z))
                }
            }
            literal("entity") {
                argument("targets", EntitiesArgument()) {
                    argument("anchor", EntityAnchorArgument) {
                        redirectTo(execute, forks = true) { context ->
                            val eyes = context.get<EntityAnchor>("anchor") == EntityAnchor.EYES
                            context.targets("targets").map { target ->
                                val y = target.y + if (eyes) EYE_HEIGHT else 0.0
                                context.source.facing(target.x, y, target.z)
                            }
                        }
                    }
                }
            }
        }
        root.literal("align") {
            argument("axes", SwizzleArgument) {
                redirectTo(execute) { context ->
                    val axes = context.get<Set<Char>>("axes")
                    val (x, y, z) = context.source.position
                    val aligned =
                        Triple(
                            if ('x' in axes) floor(x) else x,
                            if ('y' in axes) floor(y) else y,
                            if ('z' in axes) floor(z) else z,
                        )
                    listOf(ExecutionSource(context.source, position = aligned))
                }
            }
        }
        root.literal("anchored") {
            argument("anchor", EntityAnchorArgument) {
                redirectTo(execute) { context ->
                    val eyes = context.get<EntityAnchor>("anchor") == EntityAnchor.EYES
                    listOf(ExecutionSource(context.source, anchorEyes = eyes))
                }
            }
        }
        root.literal("in") {
            argument("dimension", WorldArgument) {
                redirectTo(execute) { context ->
                    listOf(ExecutionSource(context.source, world = context.get<LoadedWorld>("dimension")))
                }
            }
        }
        root.literal("summon") {
            argument("entity", RegistryArgument.resource("minecraft:entity_type")) {
                suggestsFromClient(SuggestionProviders.SUMMONABLE_ENTITIES)
                redirectTo(execute) { context -> listOfNotNull(summon(context)) }
            }
        }
        root.literal("on") {
            // parity: the eight relations `/execute on` accepts. None of them exist in this build,
            // so each yields nobody and the rest of the line does not run.
            for (relation in RELATIONS) {
                literal(relation) { redirectTo(execute, forks = true) { emptyList() } }
            }
        }
    }

    /** parity: `execute summon`, which spawns the entity and runs the rest of the line as it. */
    @OptIn(ExperimentalUuidApi::class)
    private fun summon(context: CommandContext): CommandSource? {
        val registry = players ?: return null
        val id = context.get<Identifier>("entity")
        if (id == EntityTypes.PLAYER) return null
        val type = EntityTypes[id] ?: return null
        val world = context.source.worldIn(context.environment) ?: return null
        val (x, y, z) = context.source.position
        val entity =
            ServerEntity(
                entityId = registry.allocateEntityId(),
                uuid = Uuid.random(),
                type = id,
                protocolType = type.protocolId,
                x = x,
                y = y,
                z = z,
            )
        world.state.spawnEntity(entity)
        return ExecutionSource(context.source, entity = CommandTarget.Entity(entity, world.name))
    }

    private fun buildConditions(
        builder: CommandNodeBuilder,
        execute: CommandNode,
        positive: Boolean,
    ) {
        builder.literal("block") {
            argument("pos", BlockPosArgument) {
                argument("block", BlockPredicateArgument) {
                    conditional(this, execute, positive) { context ->
                        val (x, y, z) = context.blockPos("pos")
                        val state =
                            context.source
                                .worldIn(context.environment)
                                ?.state
                                ?.getBlockState(x, y, z)
                        if (state != null && context.get<BlockPredicateInput>("block").matches(state)) 1 else null
                    }
                }
            }
        }
        builder.literal("blocks") {
            argument("start", BlockPosArgument) {
                argument("end", BlockPosArgument) {
                    argument("destination", BlockPosArgument) {
                        for (masked in listOf(false, true)) {
                            literal(if (masked) "masked" else "all") {
                                conditional(this, execute, positive) { context -> compareRegions(context, masked) }
                            }
                        }
                    }
                }
            }
        }
        builder.literal("data") {
            literal("block") {
                argument("sourcePos", BlockPosArgument) {
                    argument("path", NbtPathArgument) {
                        conditional(this, execute, positive) { context ->
                            val holder = DataHolderRef.Block(context.blockPos("sourcePos"))
                            existing(context, holder)
                        }
                    }
                }
            }
            literal("entity") {
                argument("source", EntitiesArgument(single = true)) {
                    argument("path", NbtPathArgument) {
                        conditional(this, execute, positive) { context ->
                            val player =
                                context.targets("source").firstOrNull()?.playerOrNull() ?: return@conditional null
                            existing(context, DataHolderRef.Entity(player))
                        }
                    }
                }
            }
            literal("storage") {
                argument("source", ResourceLocationArgument) {
                    argument("path", NbtPathArgument) {
                        conditional(this, execute, positive) { context ->
                            existing(context, DataHolderRef.Storage(context.get("source")))
                        }
                    }
                }
            }
        }
        builder.literal("dimension") {
            argument("dimension", WorldArgument) {
                conditional(this, execute, positive) { context ->
                    val world = context.get<LoadedWorld>("dimension")
                    if (context.source.worldIn(context.environment)?.name == world.name) 1 else null
                }
            }
        }
        builder.literal("entity") {
            argument("entities", EntitiesArgument()) {
                conditional(this, execute, positive) { context -> context.targets("entities").size.takeIf { it > 0 } }
            }
        }
        builder.literal("loaded") {
            argument("pos", BlockPosArgument) {
                conditional(this, execute, positive) { context ->
                    val (x, _, z) = context.blockPos("pos")
                    val loaded =
                        context.source
                            .worldIn(context.environment)
                            ?.state
                            ?.isLoaded(x shr 4, z shr 4)
                    if (loaded == true) 1 else null
                }
            }
        }
        builder.literal("biome") {
            argument("pos", BlockPosArgument) {
                argument("biome", RegistryArgument.orTag("worldgen/biome")) {
                    conditional(this, execute, positive) { context -> matchesBiome(context) }
                }
            }
        }
        builder.literal("function") {
            argument("name", RegistryArgument.FUNCTION) {
                conditional(this, execute, positive) { context ->
                    val runner = functions ?: return@conditional null
                    val id = context.get<Identifier>("name")
                    runCatching { runner.run(context.dispatcher, context.source, id) }.getOrDefault(0).takeIf { it > 0 }
                }
            }
        }
        builder.literal("score") { buildScoreConditions(this, execute, positive) }
        // Declared so the tree matches vanilla's; each needs a system this build has not got.
        builder.literal("predicate") {
            argument("predicate", LootPredicateArgument) { conditional(this, execute, positive) { null } }
        }
        builder.literal("items") {
            literal("block") {
                argument("pos", BlockPosArgument) {
                    argument("slots", ItemSlotsArgument) {
                        argument("item_predicate", ItemPredicateArgument) {
                            conditional(this, execute, positive) { null }
                        }
                    }
                }
            }
            literal("entity") {
                argument("entities", EntitiesArgument()) {
                    argument("slots", ItemSlotsArgument) {
                        argument("item_predicate", ItemPredicateArgument) {
                            conditional(this, execute, positive) { null }
                        }
                    }
                }
            }
        }
        builder.literal("stopwatch") {
            argument("id", ResourceLocationArgument) {
                argument("range", FloatRangeArgument) { conditional(this, execute, positive) { null } }
            }
        }
    }

    private fun buildScoreConditions(
        builder: CommandNodeBuilder,
        execute: CommandNode,
        positive: Boolean,
    ) {
        builder.argument("target", ScoreHolderArgument) {
            argument("targetObjective", ObjectiveArgument) {
                for (operator in COMPARISONS) {
                    literal(operator) {
                        argument("source", ScoreHolderArgument) {
                            argument("sourceObjective", ObjectiveArgument) {
                                conditional(this, execute, positive) { context ->
                                    compareScores(context, operator)
                                }
                            }
                        }
                    }
                }
                literal("matches") {
                    argument("range", IntRangeArgument) {
                        conditional(this, execute, positive) { context ->
                            val value = context.score("target", "targetObjective") ?: return@conditional null
                            if (matchesRange(value, context.get("range"))) 1 else null
                        }
                    }
                }
            }
        }
    }

    private fun buildStores(
        builder: CommandNodeBuilder,
        execute: CommandNode,
        fromSuccess: Boolean,
    ) {
        builder.literal("score") {
            argument("targets", ScoreHolderArgument) {
                argument("objective", ObjectiveArgument) {
                    storing(this, execute, fromSuccess) { context, value ->
                        val objective = context.get<String>("objective")
                        for (holder in context.get<List<String>>("targets")) {
                            scoreboard.setScore(holder, objective, value)
                        }
                    }
                }
            }
        }
        builder.literal("bossbar") {
            argument("id", ResourceLocationArgument) {
                for (max in listOf(false, true)) {
                    literal(if (max) "max" else "value") {
                        storing(this, execute, fromSuccess) { context, value ->
                            val bar = bossBars?.get(context.get<Identifier>("id")) ?: return@storing
                            if (max) bar.max = maxOf(1, value) else bar.value = value
                        }
                    }
                }
            }
        }
        builder.literal("storage") {
            argument("target", ResourceLocationArgument) {
                argument("path", NbtPathArgument) {
                    typedStore(this, execute, fromSuccess) { context, value, type ->
                        data?.store(context.get("target"), context.get("path"), value, type)
                    }
                }
            }
        }
        builder.literal("block") {
            argument("targetPos", BlockPosArgument) {
                argument("path", NbtPathArgument) {
                    typedStore(this, execute, fromSuccess) { context, value, type ->
                        data?.storeInBlock(context, context.blockPos("targetPos"), context.get("path"), value, type)
                    }
                }
            }
        }
        // Declared for the tree's sake: a player's data is not a compound this build can write to.
        builder.literal("entity") {
            argument("target", EntitiesArgument(single = true)) {
                argument("path", NbtPathArgument) { typedStore(this, execute, fromSuccess) { _, _, _ -> } }
            }
        }
    }

    /** The `<type> <scale>` tail every data store shares. parity: the six number types vanilla writes. */
    private fun typedStore(
        builder: CommandNodeBuilder,
        execute: CommandNode,
        fromSuccess: Boolean,
        write: (CommandContext, Double, String) -> Unit,
    ) {
        for (type in NUMBER_TYPES) {
            builder.literal(type) {
                argument("scale", DoubleArgument()) {
                    storing(this, execute, fromSuccess) { context, value ->
                        write(context, value * context.get<Double>("scale"), type)
                    }
                }
            }
        }
    }

    /**
     * A condition node: it gates the rest of the line, and reports on its own when the line ends
     * there. parity: `ExecuteCommand.addConditional`, which declares both on the same node.
     */
    private fun conditional(
        builder: CommandNodeBuilder,
        execute: CommandNode,
        positive: Boolean,
        test: (CommandContext) -> Int?,
    ) {
        builder.executes { context ->
            val count = count(test, context, positive)
            context.success(
                if (count > 0) {
                    Component.translatable("commands.execute.conditional.pass_count", Component.text(count.toString()))
                } else {
                    Component.translatable("commands.execute.conditional.fail")
                },
            )
            count
        }
        builder.redirectTo(execute, forks = true) { context ->
            if (count(test, context, positive) > 0) listOf(context.source) else emptyList()
        }
    }

    /** What a condition reports: how many it matched, or zero when it did not hold. */
    private fun count(
        test: (CommandContext) -> Int?,
        context: CommandContext,
        positive: Boolean,
    ): Int {
        val matched = runCatching { test(context) }.getOrNull()
        if ((matched != null) != positive) return 0
        return matched ?: 1
    }

    /** A store node: wraps the source so whatever the chain ends up running reports back here. */
    private fun storing(
        builder: CommandNodeBuilder,
        execute: CommandNode,
        fromSuccess: Boolean,
        write: (CommandContext, Int) -> Unit,
    ) {
        builder.redirectTo(execute) { context ->
            val base = context.source as? ExecutionSource ?: ExecutionSource(context.source)
            listOf(
                base.withCallback { success, result ->
                    write(context, if (fromSuccess) (if (success) 1 else 0) else result)
                },
            )
        }
    }

    private fun existing(
        context: CommandContext,
        holder: DataHolderRef,
    ): Int? {
        val path = context.get<String>("path")
        return if (data?.pathExists(context, holder, path) == true) 1 else null
    }

    /** parity: `if blocks`, whose result is how many matched, and `masked` ignores air in the source. */
    private fun compareRegions(
        context: CommandContext,
        masked: Boolean,
    ): Int? {
        val world = context.source.worldIn(context.environment) ?: return null
        val start = context.blockPos("start")
        val end = context.blockPos("end")
        val destination = context.blockPos("destination")
        val minX = minOf(start.first, end.first)
        val minY = minOf(start.second, end.second)
        val minZ = minOf(start.third, end.third)

        var matched = 0
        for (x in minX..maxOf(start.first, end.first)) {
            for (y in minY..maxOf(start.second, end.second)) {
                for (z in minZ..maxOf(start.third, end.third)) {
                    val sourceState = world.state.getBlockState(x, y, z) ?: return null
                    if (masked && sourceState == Blocks.AIR.stateId) continue
                    val targetState =
                        world.state.getBlockState(
                            destination.first + (x - minX),
                            destination.second + (y - minY),
                            destination.third + (z - minZ),
                        ) ?: return null
                    if (sourceState != targetState) return null
                    matched++
                }
            }
        }
        return matched
    }

    private fun matchesBiome(context: CommandContext): Int? {
        val world = context.source.worldIn(context.environment) ?: return null
        val (x, y, z) = context.blockPos("pos")
        val chunk = world.state.chunkIfLoaded(x shr 4, z shr 4) ?: return null
        val biomeId =
            chunk.section((y - chunk.verticalBounds.minY) / 16).getBiomeId(
                (x and 15) / 4,
                ((y - chunk.verticalBounds.minY) % 16) / 4,
                (z and 15) / 4,
            )
        val wanted = context.get<Identifier>("biome")
        return if (org.basaltmc.server.registry.Biomes.REGISTRY
                .byId(biomeId)
                ?.id == wanted
        ) {
            1
        } else {
            null
        }
    }

    private fun compareScores(
        context: CommandContext,
        operator: String,
    ): Int? {
        val value = context.score("target", "targetObjective") ?: return null
        val other = context.score("source", "sourceObjective") ?: return null
        val holds =
            when (operator) {
                "<" -> value < other
                "<=" -> value <= other
                "=" -> value == other
                ">=" -> value >= other
                else -> value > other
            }
        return if (holds) 1 else null
    }

    /** parity: `MinMaxBounds.Ints`, whose forms are `n`, `a..b`, `a..` and `..b`. */
    private fun matchesRange(
        value: Int,
        range: String,
    ): Boolean {
        if (".." !in range) return value == range.toIntOrNull()
        val min = range.substringBefore("..").takeIf { it.isNotEmpty() }?.toIntOrNull()
        val max = range.substringAfter("..").takeIf { it.isNotEmpty() }?.toIntOrNull()
        return (min == null || value >= min) && (max == null || value <= max)
    }

    private fun CommandContext.score(
        holderArgument: String,
        objectiveArgument: String,
    ): Int? {
        val holder = get<List<String>>(holderArgument).firstOrNull() ?: return null
        return scoreboard.score(holder, get(objectiveArgument))
    }

    private fun CommandContext.targets(name: String): List<CommandTarget> = get(name)

    private fun CommandContext.blockPos(name: String): Triple<Int, Int, Int> =
        get<org.basaltmc.server.runtime.command.BlockCoordinates>(name).resolve(source.position)

    private fun CommandTarget.playerOrNull() = (this as? CommandTarget.Player)?.session

    /** parity: `at`, which takes the target's position, rotation and world, but not its identity. */
    private fun CommandSource.at(target: CommandTarget): CommandSource =
        ExecutionSource(
            this,
            position = Triple(target.x, target.y, target.z),
            rotation = target.yaw to target.pitch,
        )

    /** parity: `Entity.lookAt`: yaw grows clockwise from +Z, pitch is positive downwards. */
    private fun CommandSource.facing(
        x: Double,
        y: Double,
        z: Double,
    ): CommandSource {
        val (sourceX, sourceY, sourceZ) = position
        val eyeY = sourceY + if ((this as? ExecutionSource)?.anchorEyes == true) EYE_HEIGHT else 0.0
        val dx = x - sourceX
        val dy = y - eyeY
        val dz = z - sourceZ
        val yaw = (-atan2(dx, dz) * DEGREES_PER_RADIAN).toFloat()
        val pitch = (-atan2(dy, sqrt(dx * dx + dz * dz)) * DEGREES_PER_RADIAN).toFloat()
        return ExecutionSource(this, rotation = yaw to pitch)
    }

    /** parity: `positioned over`, which drops the position onto the named heightmap. */
    private fun CommandContext.over(heightmap: String): CommandSource {
        val world = source.worldIn(environment) ?: return source
        val (x, _, z) = source.position
        val chunk = world.state.chunkIfLoaded(floor(x).toInt() shr 4, floor(z).toInt() shr 4) ?: return source
        val type =
            when (heightmap) {
                "world_surface" -> Chunk.HeightmapType.WORLD_SURFACE
                "ocean_floor" -> Chunk.HeightmapType.OCEAN_FLOOR
                "motion_blocking_no_leaves" -> Chunk.HeightmapType.MOTION_BLOCKING_NO_LEAVES
                else -> Chunk.HeightmapType.MOTION_BLOCKING
            }
        val height = chunk.heightAt(floor(x).toInt() and 15, floor(z).toInt() and 15, type)
        return ExecutionSource(source, position = Triple(x, height.toDouble(), z))
    }

    private companion object {
        const val DEGREES_PER_RADIAN = 57.2957795130823

        /** parity: a player's eye height, which is what `anchored eyes` aims from. */
        const val EYE_HEIGHT = 1.62

        val COMPARISONS = listOf("<", "<=", "=", ">=", ">")
        val NUMBER_TYPES = listOf("byte", "double", "float", "int", "long", "short")

        val RELATIONS =
            listOf("attacker", "controller", "leasher", "origin", "owner", "passengers", "target", "vehicle")
    }
}
