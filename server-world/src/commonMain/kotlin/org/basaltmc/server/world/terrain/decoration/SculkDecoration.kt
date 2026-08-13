package org.basaltmc.server.world.terrain.decoration

import org.basaltmc.server.random.BasaltRandom
import org.basaltmc.server.random.WorldgenRandom
import org.basaltmc.server.random.XoroshiroRandom
import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.world.Chunk
import org.basaltmc.server.world.WorldHeight
import org.basaltmc.server.world.terrain.surface.SurfaceRuleEnvironment
import org.basaltmc.wire.common.Identifier

/** // parity: the deep-dark sculk entries at indexes 0 and 1 of `UNDERGROUND_DECORATION`. */
public object SculkDecoration {
    public fun decorate(
        chunk: Chunk,
        seed: Long,
        environment: SurfaceRuleEnvironment,
        externalBlockState: ((x: Int, y: Int, z: Int) -> Int?)? = null,
    ) {
        decorateFeatures(chunk, seed, environment, listOf(SCULK_VEIN_INDEX, SCULK_PATCH_INDEX), externalBlockState)
    }

    internal fun decorateFeatures(
        chunk: Chunk,
        seed: Long,
        environment: SurfaceRuleEnvironment,
        globalIndices: Iterable<Int>,
        externalBlockState: ((x: Int, y: Int, z: Int) -> Int?)? = null,
        blockWriteObserver: ((x: Int, y: Int, z: Int, stateId: Int) -> Unit)? = null,
    ) {
        val region = SculkRegion(chunk, externalBlockState, blockWriteObserver)
        val random = WorldgenRandom(XoroshiroRandom(seed))
        val decorationSeed = random.setDecorationSeed(seed, chunk.chunkX * 16, chunk.chunkZ * 16)
        for (globalIndex in globalIndices) {
            random.setFeatureSeed(decorationSeed, globalIndex, UNDERGROUND_DECORATION_STEP)
            when (globalIndex) {
                SCULK_VEIN_INDEX -> placeSculkVeins(region, random, environment)
                SCULK_PATCH_INDEX -> placeSculkPatches(region, random, environment)
            }
        }
    }

    private fun placeSculkPatches(
        region: SculkRegion,
        random: WorldgenRandom,
        environment: SurfaceRuleEnvironment,
    ) {
        repeat(256) {
            val x = region.chunk.chunkX * 16 + random.nextInt(16)
            val z = region.chunk.chunkZ * 16 + random.nextInt(16)
            val origin = DecorationPos(x, WorldHeight.MIN_Y + random.nextInt(321), z)
            if (environment.biomeAt(x, origin.y, z).substringAfter(':') != "deep_dark") return@repeat
            if (!region.canSculkSpreadFrom(origin)) return@repeat

            val cursors = MutableList(10) { ChargeCursor(origin, 32) }
            repeat(64) {
                for (cursor in cursors) cursor.update(region, origin, random)
            }

            if (random.nextFloat() <= 0.5f && region.hasFullCollision(origin.offset(0, -1, 0))) {
                region.set(origin, SCULK_CATALYST_STATE_ID)
            }
        }
    }

    private fun placeSculkVeins(
        region: SculkRegion,
        random: WorldgenRandom,
        environment: SurfaceRuleEnvironment,
    ) {
        val count = 204 + random.nextInt(47)
        repeat(count) {
            val x = region.chunk.chunkX * 16 + random.nextInt(16)
            val z = region.chunk.chunkZ * 16 + random.nextInt(16)
            val origin = DecorationPos(x, WorldHeight.MIN_Y + random.nextInt(321), z)
            if (environment.biomeAt(x, origin.y, z).substringAfter(':') != "deep_dark") return@repeat
            placeMultifaceGrowth(region, random, origin)
        }
    }

    private fun placeMultifaceGrowth(
        region: SculkRegion,
        random: BasaltRandom,
        origin: DecorationPos,
    ): Boolean {
        if (!region.isAirOrWater(origin)) return false
        val searchDirections = VALID_DIRECTIONS.toMutableList().also { shuffle(it, random) }
        if (placeGrowthIfPossible(region, random, origin, searchDirections)) return true

        for (searchDirection in searchDirections) {
            val placementDirections =
                VALID_DIRECTIONS
                    .filterTo(mutableListOf()) { it != searchDirection.opposite }
                    .also { shuffle(it, random) }
            // parity: 26.2's MultifaceGrowthFeature resets to origin + direction on every one of
            // its 20 iterations, so a failed immutable attempt has the same result twenty times.
            val position = origin.offset(searchDirection.x, searchDirection.y, searchDirection.z)
            val stateId = region.get(position) ?: continue
            if (!region.isAirOrWater(stateId) && !region.isSculkVein(stateId)) continue
            if (placeGrowthIfPossible(region, random, position, placementDirections)) return true
        }
        return false
    }

    private fun placeGrowthIfPossible(
        region: SculkRegion,
        random: BasaltRandom,
        position: DecorationPos,
        placementDirections: List<Direction>,
    ): Boolean {
        val oldStateId = region.get(position) ?: return false
        for (placementDirection in placementDirections) {
            val neighbour = position.offset(placementDirection.x, placementDirection.y, placementDirection.z)
            if (region.blockPath(neighbour) !in SCULK_VEIN_SUPPORTS) continue
            val newStateId = region.withFace(oldStateId, position, placementDirection) ?: return false
            region.set(position, newStateId)
            if (random.nextFloat() < 1.0f) {
                spreadFromFaceTowardRandomDirection(region, random, position, placementDirection, newStateId)
            }
            return true
        }
        return false
    }

    private fun spreadFromFaceTowardRandomDirection(
        region: SculkRegion,
        random: BasaltRandom,
        source: DecorationPos,
        startingFace: Direction,
        sourceStateId: Int,
    ) {
        val directions = DIRECTIONS.toMutableList().also { shuffle(it, random) }
        for (spreadDirection in directions) {
            if (spreadDirection.axis == startingFace.axis) continue
            if (!region.hasFace(sourceStateId, startingFace) || region.hasFace(sourceStateId, spreadDirection)) continue
            for (type in SpreadType.entries) {
                val spread = spreadPosition(source, spreadDirection, startingFace, type)
                if (!canSpreadInto(region, source, spread)) continue
                val oldStateId = region.get(spread.position) ?: continue
                val newStateId = region.withFace(oldStateId, spread.position, spread.face) ?: continue
                region.set(spread.position, newStateId)
                return
            }
        }
    }

    private fun spreadPosition(
        source: DecorationPos,
        spreadDirection: Direction,
        startingFace: Direction,
        type: SpreadType,
    ): SpreadPosition =
        when (type) {
            SpreadType.SAME_POSITION -> SpreadPosition(source, spreadDirection)
            SpreadType.SAME_PLANE ->
                SpreadPosition(source.offset(spreadDirection.x, spreadDirection.y, spreadDirection.z), startingFace)
            SpreadType.WRAP_AROUND ->
                SpreadPosition(
                    source
                        .offset(spreadDirection.x, spreadDirection.y, spreadDirection.z)
                        .offset(startingFace.x, startingFace.y, startingFace.z),
                    spreadDirection.opposite,
                )
        }

    private fun canSpreadInto(
        region: SculkRegion,
        source: DecorationPos,
        spread: SpreadPosition,
    ): Boolean {
        val existingStateId = region.get(spread.position) ?: return false
        val against = spread.position.offset(spread.face.x, spread.face.y, spread.face.z)
        if (region.blockPath(against) in SCULK_SPREAD_FORBIDDEN_SUPPORTS) return false
        if (source.manhattanDistance(spread.position) == 2) {
            val neighbour = source.offset(-spread.face.x, -spread.face.y, -spread.face.z)
            if (region.hasSturdyFace(neighbour, spread.face)) return false
        }
        if (Blocks.hasFluid(existingStateId) && !Blocks.hasWaterFluid(existingStateId)) return false
        if (!region.isReplaceableForVein(existingStateId)) return false
        if (region.isSculkVein(existingStateId) && region.hasFace(existingStateId, spread.face)) return false
        return region.hasSturdyFace(against, spread.face.opposite)
    }

    private fun spreadAll(
        region: SculkRegion,
        sourceStateId: Int,
        source: DecorationPos,
        sameSpaceOnly: Boolean,
    ): Int {
        var placements = 0
        val otherBlockSource = !region.isSculkVein(sourceStateId)
        for (startingFace in DIRECTIONS) {
            if (!otherBlockSource && !region.hasFace(sourceStateId, startingFace)) continue
            for (spreadDirection in DIRECTIONS) {
                if (spreadDirection.axis == startingFace.axis) continue
                if (!otherBlockSource && region.hasFace(sourceStateId, spreadDirection)) continue
                val spreadTypes = if (sameSpaceOnly) SAME_POSITION_ONLY else SpreadType.entries
                for (type in spreadTypes) {
                    val spread = spreadPosition(source, spreadDirection, startingFace, type)
                    if (!canSpreadInto(region, source, spread)) continue
                    val oldStateId = region.get(spread.position) ?: continue
                    val newStateId = region.withFace(oldStateId, spread.position, spread.face) ?: continue
                    region.set(spread.position, newStateId)
                    placements++
                    break
                }
            }
        }
        return placements
    }

    private fun attemptSpreadVein(
        region: SculkRegion,
        position: DecorationPos,
        stateId: Int,
        facings: List<Direction>?,
        behaviour: SculkBehaviour,
    ): Boolean =
        if (behaviour != SculkBehaviour.DEFAULT) {
            spreadAll(region, stateId, position, sameSpaceOnly = false) > 0
        } else if (facings == null) {
            spreadAll(region, stateId, position, sameSpaceOnly = true) > 0
        } else if (facings.isNotEmpty()) {
            region.isAirOrWater(stateId) && regrowVein(region, position, stateId, facings)
        } else {
            spreadAll(region, stateId, position, sameSpaceOnly = false) > 0
        }

    private fun regrowVein(
        region: SculkRegion,
        position: DecorationPos,
        existingStateId: Int,
        facings: List<Direction>,
    ): Boolean {
        var newStateId =
            if (existingStateId ==
                Blocks.WATER.stateId
            ) {
                SCULK_VEIN_WATERLOGGED_STATE_ID
            } else {
                SCULK_VEIN_STATE_ID
            }
        var placedFace = false
        for (face in facings) {
            if (!region.hasSturdyFace(position.offset(face.x, face.y, face.z), face.opposite)) continue
            newStateId = region.withFace(newStateId, position, face) ?: continue
            placedFace = true
        }
        if (placedFace) region.set(position, newStateId)
        return placedFace
    }

    private fun attemptUseCharge(
        cursor: ChargeCursor,
        region: SculkRegion,
        origin: DecorationPos,
        random: BasaltRandom,
        behaviour: SculkBehaviour,
    ): Int =
        when (behaviour) {
            SculkBehaviour.DEFAULT -> if (cursor.decayDelay > 0) cursor.charge else 0
            SculkBehaviour.VEIN -> {
                if (attemptPlaceSculk(region, cursor.position, random)) {
                    cursor.charge - 1
                } else if (random.nextInt(5) == 0) {
                    (cursor.charge * 0.5f).toInt()
                } else {
                    cursor.charge
                }
            }
            SculkBehaviour.SCULK -> useChargeFromSculk(cursor, region, origin, random)
        }

    private fun attemptPlaceSculk(
        region: SculkRegion,
        position: DecorationPos,
        random: BasaltRandom,
    ): Boolean {
        val stateId = region.get(position) ?: return false
        val supports = DIRECTIONS.toMutableList().also { shuffle(it, random) }
        for (support in supports) {
            if (!region.hasFace(stateId, support)) continue
            val supportPosition = position.offset(support.x, support.y, support.z)
            if (region.blockPath(supportPosition) !in SCULK_REPLACEABLE_WORLD_GEN) continue
            region.set(supportPosition, SCULK_STATE_ID)
            spreadAll(region, SCULK_STATE_ID, supportPosition, sameSpaceOnly = false)

            val skip = support.opposite
            for (direction in DIRECTIONS) {
                if (direction == skip) continue
                val veinPosition = supportPosition.offset(direction.x, direction.y, direction.z)
                val veinStateId = region.get(veinPosition) ?: continue
                if (region.isSculkVein(veinStateId)) dischargeVein(region, veinPosition, veinStateId)
            }
            return true
        }
        return false
    }

    private fun useChargeFromSculk(
        cursor: ChargeCursor,
        region: SculkRegion,
        origin: DecorationPos,
        random: BasaltRandom,
    ): Int {
        val charge = cursor.charge
        if (charge == 0 || random.nextInt(5) != 0) return charge
        val isCloseToOrigin = cursor.position == origin
        if (!isCloseToOrigin && canPlaceGrowth(region, cursor.position)) {
            if (random.nextInt(50) < charge) {
                val growthPosition = cursor.position.offset(0, 1, 0)
                val waterlogged = region.get(growthPosition) == Blocks.WATER.stateId
                val stateId =
                    if (random.nextInt(11) == 0) {
                        if (waterlogged) SCULK_SHRIEKER_WATERLOGGED_STATE_ID else SCULK_SHRIEKER_STATE_ID
                    } else {
                        if (waterlogged) SCULK_SENSOR_WATERLOGGED_STATE_ID else SCULK_SENSOR_STATE_ID
                    }
                region.set(growthPosition, stateId)
            }
            return maxOf(0, charge - 50)
        }

        if (random.nextInt(10) != 0) return charge
        return charge - if (isCloseToOrigin) 1 else decayPenalty(cursor.position, origin, charge)
    }

    private fun canPlaceGrowth(
        region: SculkRegion,
        position: DecorationPos,
    ): Boolean {
        val aboveStateId = region.get(position.offset(0, 1, 0)) ?: return false
        if (!Blocks.isAir(aboveStateId) && aboveStateId != Blocks.WATER.stateId) return false
        var growthCount = 0
        for (z in position.z - 4..position.z + 4) {
            for (y in position.y..position.y + 2) {
                for (x in position.x - 4..position.x + 4) {
                    when (region.blockPath(DecorationPos(x, y, z))) {
                        "sculk_sensor", "sculk_shrieker" -> if (++growthCount > 2) return false
                    }
                }
            }
        }
        return true
    }

    private fun decayPenalty(
        position: DecorationPos,
        origin: DecorationPos,
        charge: Int,
    ): Int {
        val dx = position.x - origin.x
        val dy = position.y - origin.y
        val dz = position.z - origin.z
        val distance = kotlin.math.sqrt((dx * dx + dy * dy + dz * dz).toDouble()).toFloat()
        val outsideRadius = distance - 1.0f
        val distanceFactor = minOf(1.0f, outsideRadius * outsideRadius / 529.0f)
        return maxOf(1, (charge * distanceFactor * 0.5f).toInt())
    }

    private fun dischargeVein(
        region: SculkRegion,
        position: DecorationPos,
        initialStateId: Int,
    ) {
        var stateId = initialStateId
        for (direction in DIRECTIONS) {
            if (!region.hasFace(stateId, direction)) continue
            val neighbour = position.offset(direction.x, direction.y, direction.z)
            if (region.blockPath(neighbour) != "sculk") continue
            val properties = Blocks.byStateId(stateId)?.properties?.toMutableMap() ?: continue
            properties[direction.property] = "false"
            stateId = Blocks.stateIdOf(SCULK_VEIN_ID, properties) ?: stateId
        }
        if (DIRECTIONS.none { region.hasFace(stateId, it) }) {
            stateId = if (Blocks.hasWaterFluid(initialStateId)) Blocks.WATER.stateId else Blocks.AIR.stateId
        }
        region.set(position, stateId)
    }

    private fun findMovementPosition(
        region: SculkRegion,
        position: DecorationPos,
        random: BasaltRandom,
    ): DecorationPos? {
        var selected: DecorationPos? = null
        val offsets = NON_CORNER_NEIGHBOURS.toMutableList().also { shuffle(it, random) }
        for (offset in offsets) {
            val neighbour = position.offset(offset.x, offset.y, offset.z)
            val neighbourStateId = region.get(neighbour) ?: continue
            if (region.behaviour(neighbourStateId) == SculkBehaviour.DEFAULT) continue
            if (!isMovementUnobstructed(region, position, neighbour, offset)) continue
            selected = neighbour
            if (region.isSculkVein(neighbourStateId) && hasSubstrateAccess(region, neighbour, neighbourStateId)) break
        }
        return selected
    }

    private fun isMovementUnobstructed(
        region: SculkRegion,
        from: DecorationPos,
        to: DecorationPos,
        delta: Offset,
    ): Boolean {
        if (from.manhattanDistance(to) == 1) return true
        val directionX = if (delta.x < 0) DIRECTIONS[4] else DIRECTIONS[5]
        val directionY = if (delta.y < 0) DIRECTIONS[0] else DIRECTIONS[1]
        val directionZ = if (delta.z < 0) DIRECTIONS[2] else DIRECTIONS[3]
        return when {
            delta.x == 0 -> isUnobstructed(region, from, directionY) || isUnobstructed(region, from, directionZ)
            delta.y == 0 -> isUnobstructed(region, from, directionX) || isUnobstructed(region, from, directionZ)
            else -> isUnobstructed(region, from, directionX) || isUnobstructed(region, from, directionY)
        }
    }

    private fun isUnobstructed(
        region: SculkRegion,
        from: DecorationPos,
        direction: Direction,
    ): Boolean = !region.hasSturdyFace(from.offset(direction.x, direction.y, direction.z), direction.opposite)

    private fun hasSubstrateAccess(
        region: SculkRegion,
        position: DecorationPos,
        stateId: Int,
    ): Boolean =
        DIRECTIONS.any { direction ->
            region.hasFace(stateId, direction) &&
                region.blockPath(position.offset(direction.x, direction.y, direction.z)) in SCULK_REPLACEABLE
        }

    private class ChargeCursor(
        var position: DecorationPos,
        var charge: Int,
        var decayDelay: Int = 1,
        var updateDelay: Int = 0,
        var facings: List<Direction>? = null,
    ) {
        fun update(
            region: SculkRegion,
            origin: DecorationPos,
            random: BasaltRandom,
        ) {
            if (charge <= 0) return
            if (updateDelay > 0) {
                updateDelay--
                return
            }

            var currentStateId = region.get(position) ?: Blocks.AIR.stateId
            var behaviour = region.behaviour(currentStateId)
            if (attemptSpreadVein(region, position, currentStateId, facings, behaviour)) {
                if (behaviour != SculkBehaviour.SCULK) {
                    currentStateId = region.get(position) ?: Blocks.AIR.stateId
                    behaviour = region.behaviour(currentStateId)
                }
            }

            charge = attemptUseCharge(this, region, origin, random, behaviour)
            if (charge <= 0) {
                if (behaviour == SculkBehaviour.VEIN) dischargeVein(region, position, currentStateId)
                return
            }

            val transferPosition = findMovementPosition(region, position, random)
            if (transferPosition != null) {
                if (behaviour == SculkBehaviour.VEIN) dischargeVein(region, position, currentStateId)
                position = transferPosition
                val dx = position.x - origin.x
                val dz = position.z - origin.z
                if (dx * dx + dz * dz >= 225) {
                    charge = 0
                    return
                }
                currentStateId = region.get(position) ?: Blocks.AIR.stateId
            }

            if (region.behaviour(currentStateId) != SculkBehaviour.DEFAULT) {
                facings = DIRECTIONS.filter { region.hasFace(currentStateId, it) }
            }
            decayDelay = if (behaviour == SculkBehaviour.DEFAULT) maxOf(decayDelay - 1, 0) else 1
            updateDelay = 1
        }
    }

    private fun <T> shuffle(
        values: MutableList<T>,
        random: BasaltRandom,
    ) {
        for (size in values.size downTo 2) {
            val swapTo = random.nextInt(size)
            val oldLast = values[size - 1]
            values[size - 1] = values[swapTo]
            values[swapTo] = oldLast
        }
    }

    private data class Direction(
        val property: String,
        val x: Int,
        val y: Int,
        val z: Int,
        val axis: Int,
        val oppositeIndex: Int,
    ) {
        val opposite: Direction get() = DIRECTIONS[oppositeIndex]
    }

    private data class SpreadPosition(
        val position: DecorationPos,
        val face: Direction,
    )

    private data class Offset(
        val x: Int,
        val y: Int,
        val z: Int,
    )

    private enum class SpreadType {
        SAME_POSITION,
        SAME_PLANE,
        WRAP_AROUND,
    }

    private enum class SculkBehaviour {
        DEFAULT,
        SCULK,
        VEIN,
    }

    private class SculkRegion(
        val chunk: Chunk,
        private val externalBlockState: ((x: Int, y: Int, z: Int) -> Int?)?,
        private val blockWriteObserver: ((x: Int, y: Int, z: Int, stateId: Int) -> Unit)?,
    ) {
        private val externalWrites = HashMap<DecorationPos, Int>()

        fun get(position: DecorationPos): Int? =
            when {
                position.y !in WorldHeight.MIN_Y..WorldHeight.MAX_Y -> Blocks.AIR.stateId
                position.x shr 4 == chunk.chunkX && position.z shr 4 == chunk.chunkZ ->
                    chunk.getBlockStateId(position.x and 15, position.y, position.z and 15)
                else -> externalWrites[position] ?: externalBlockState?.invoke(position.x, position.y, position.z)
            }

        fun set(
            position: DecorationPos,
            stateId: Int,
        ) {
            if (position.y !in WorldHeight.MIN_Y..WorldHeight.MAX_Y) return
            if (position.x shr 4 == chunk.chunkX && position.z shr 4 == chunk.chunkZ) {
                chunk.setBlockStateId(position.x and 15, position.y, position.z and 15, stateId)
            } else {
                externalWrites[position] = stateId
            }
            blockWriteObserver?.invoke(position.x, position.y, position.z, stateId)
        }

        fun blockPath(position: DecorationPos): String? = get(position)?.let(Blocks::byStateId)?.block?.path

        fun isAirOrWater(position: DecorationPos): Boolean = get(position)?.let(::isAirOrWater) == true

        fun isAirOrWater(stateId: Int): Boolean = Blocks.isAir(stateId) || stateId == Blocks.WATER.stateId

        fun isSculkVein(stateId: Int): Boolean = Blocks.byStateId(stateId)?.block?.path == "sculk_vein"

        fun behaviour(stateId: Int): SculkBehaviour =
            when (Blocks.byStateId(stateId)?.block?.path) {
                "sculk" -> SculkBehaviour.SCULK
                "sculk_vein" -> SculkBehaviour.VEIN
                else -> SculkBehaviour.DEFAULT
            }

        fun canSculkSpreadFrom(position: DecorationPos): Boolean {
            val stateId = get(position) ?: return false
            if (behaviour(stateId) != SculkBehaviour.DEFAULT) return true
            if (!isAirOrWater(stateId)) return false
            return DIRECTIONS.any { direction ->
                hasFullCollision(position.offset(direction.x, direction.y, direction.z))
            }
        }

        fun isReplaceableForVein(stateId: Int): Boolean =
            Blocks.isAir(stateId) || stateId == Blocks.WATER.stateId || isSculkVein(stateId)

        fun hasSturdyFace(
            position: DecorationPos,
            face: Direction,
        ): Boolean =
            when (blockPath(position)) {
                "sculk_sensor", "sculk_shrieker" -> face == DIRECTIONS[0]
                in NON_STURDY_BLOCKS -> false
                else -> get(position)?.let(Chunk.HeightmapType.OCEAN_FLOOR::matches) == true
            }

        fun hasFullCollision(position: DecorationPos): Boolean =
            blockPath(position) !in NON_FULL_COLLISION_BLOCKS &&
                get(position)?.let(Chunk.HeightmapType.OCEAN_FLOOR::matches) == true

        fun hasFace(
            stateId: Int,
            direction: Direction,
        ): Boolean = Blocks.byStateId(stateId)?.properties?.get(direction.property) == "true"

        fun withFace(
            oldStateId: Int,
            position: DecorationPos,
            direction: Direction,
        ): Int? {
            if (!hasSturdyFace(position.offset(direction.x, direction.y, direction.z), direction.opposite)) return null
            val properties =
                if (isSculkVein(oldStateId)) {
                    Blocks.byStateId(oldStateId)?.properties?.toMutableMap() ?: return null
                } else {
                    DIRECTIONS.associateTo(linkedMapOf()) { it.property to "false" }.also {
                        it["waterlogged"] = (oldStateId == Blocks.WATER.stateId).toString()
                    }
                }
            if (properties[direction.property] == "true") return null
            properties[direction.property] = "true"
            return Blocks.stateIdOf(SCULK_VEIN_ID, properties)
        }
    }

    private const val UNDERGROUND_DECORATION_STEP: Int = 7
    private const val SCULK_VEIN_INDEX: Int = 0
    private const val SCULK_PATCH_INDEX: Int = 1
    private val SCULK_VEIN_ID = Identifier.vanilla("sculk_vein")
    private val SCULK_STATE_ID = defaultStateId("sculk")
    private val SCULK_VEIN_STATE_ID = defaultStateId("sculk_vein")
    private val SCULK_VEIN_WATERLOGGED_STATE_ID = SCULK_VEIN_STATE_ID - 2
    private val SCULK_CATALYST_STATE_ID = defaultStateId("sculk_catalyst")
    private val SCULK_SENSOR_STATE_ID = defaultStateId("sculk_sensor")
    private val SCULK_SENSOR_WATERLOGGED_STATE_ID = SCULK_SENSOR_STATE_ID - 1
    private val SCULK_SHRIEKER_STATE_ID = defaultStateId("sculk_shrieker") - 4
    private val SCULK_SHRIEKER_WATERLOGGED_STATE_ID = SCULK_SHRIEKER_STATE_ID - 1
    private val DIRECTIONS =
        listOf(
            Direction("down", 0, -1, 0, 1, 1),
            Direction("up", 0, 1, 0, 1, 0),
            Direction("north", 0, 0, -1, 2, 3),
            Direction("south", 0, 0, 1, 2, 2),
            Direction("west", -1, 0, 0, 0, 5),
            Direction("east", 1, 0, 0, 0, 4),
        )
    private val VALID_DIRECTIONS =
        listOf(DIRECTIONS[1], DIRECTIONS[0], DIRECTIONS[2], DIRECTIONS[5], DIRECTIONS[3], DIRECTIONS[4])
    private val SAME_POSITION_ONLY = listOf(SpreadType.SAME_POSITION)
    private val NON_CORNER_NEIGHBOURS =
        buildList {
            for (z in -1..1) {
                for (y in -1..1) {
                    for (x in -1..1) {
                        if ((x == 0 || y == 0 || z == 0) && (x != 0 || y != 0 || z != 0)) add(Offset(x, y, z))
                    }
                }
            }
        }
    private val SCULK_VEIN_SUPPORTS =
        setOf("stone", "andesite", "diorite", "granite", "dripstone_block", "calcite", "tuff", "deepslate")
    private val SCULK_SPREAD_FORBIDDEN_SUPPORTS = setOf("sculk", "sculk_catalyst", "moving_piston")
    private val NON_STURDY_BLOCKS =
        setOf("sculk_vein", "sculk_sensor", "sculk_shrieker", "pointed_dripstone", "sulfur_spike", "snow")
    private val NON_FULL_COLLISION_BLOCKS = NON_STURDY_BLOCKS
    private val SCULK_REPLACEABLE =
        setOf(
            "stone",
            "granite",
            "diorite",
            "andesite",
            "tuff",
            "deepslate",
            "dirt",
            "coarse_dirt",
            "rooted_dirt",
            "mud",
            "muddy_mangrove_roots",
            "moss_block",
            "pale_moss_block",
            "grass_block",
            "podzol",
            "mycelium",
            "terracotta",
            "white_terracotta",
            "orange_terracotta",
            "magenta_terracotta",
            "light_blue_terracotta",
            "yellow_terracotta",
            "lime_terracotta",
            "pink_terracotta",
            "gray_terracotta",
            "light_gray_terracotta",
            "cyan_terracotta",
            "purple_terracotta",
            "blue_terracotta",
            "brown_terracotta",
            "green_terracotta",
            "red_terracotta",
            "black_terracotta",
            "crimson_nylium",
            "warped_nylium",
            "netherrack",
            "basalt",
            "blackstone",
            "sand",
            "red_sand",
            "gravel",
            "soul_sand",
            "soul_soil",
            "calcite",
            "smooth_basalt",
            "clay",
            "dripstone_block",
            "end_stone",
            "red_sandstone",
            "sandstone",
            "sulfur",
            "cinnabar",
        )
    private val SCULK_REPLACEABLE_WORLD_GEN =
        SCULK_REPLACEABLE +
            setOf(
                "deepslate_bricks",
                "deepslate_tiles",
                "cobbled_deepslate",
                "cracked_deepslate_bricks",
                "cracked_deepslate_tiles",
                "polished_deepslate",
            )

    private fun defaultStateId(path: String): Int =
        Blocks.REGISTRY.byIdentifier(Identifier.vanilla(path))?.stateId ?: error("unknown block '$path'")
}

private fun DecorationPos.manhattanDistance(other: DecorationPos): Int =
    kotlin.math.abs(x - other.x) + kotlin.math.abs(y - other.y) + kotlin.math.abs(z - other.z)
