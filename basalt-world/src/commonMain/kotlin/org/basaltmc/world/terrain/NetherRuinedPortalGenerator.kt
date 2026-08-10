package org.basaltmc.world.terrain

import org.basaltmc.nbt.NbtCompound
import org.basaltmc.nbt.deepCopy
import org.basaltmc.random.LegacyRandom
import org.basaltmc.random.PositionalHashing
import org.basaltmc.random.WorldgenRandom
import org.basaltmc.random.XoroshiroRandom
import org.basaltmc.registry.Blocks
import org.basaltmc.registry.Identifier
import org.basaltmc.world.Chunk
import org.basaltmc.world.ChunkBlockEntity
import org.basaltmc.world.ChunkHeight
import org.basaltmc.world.structure.StructureBoundingBox
import org.basaltmc.world.structure.StructureDirection
import org.basaltmc.world.structure.StructureMirror
import org.basaltmc.world.structure.StructurePlacementGrid
import org.basaltmc.world.structure.StructurePos
import org.basaltmc.world.structure.StructureRotation
import org.basaltmc.world.structure.StructureTemplate
import org.basaltmc.world.structure.TemplateLoader
import org.basaltmc.world.terrain.carver.NetherCaveCarver
import kotlin.concurrent.AtomicReference
import kotlin.math.abs
import kotlin.math.min

/**
 * // parity: RuinedPortalStructure and RuinedPortalPiece for the 26.2 `ruined_portal_nether`
 * setup. The official piece runs once in its template-center chunk and writes through a
 * WorldGenRegion into neighboring chunks. Basalt owns one chunk at a time, so this replays that
 * one global operation from the center chunk's RNG and clips only the final writes to the target.
 */
internal class NetherRuinedPortalGenerator(
    private val biomeSource: NetherBiomeSource,
    private val templateLoader: TemplateLoader,
    private val baseStateAt: (Int, Int, Int) -> Int,
    private val placeEarlierBastions: (Chunk, Long) -> Unit,
) {
    private val replayCache = AtomicReference<Map<PortalCacheKey, PortalReplay>>(emptyMap())

    fun generateInto(
        chunk: Chunk,
        seed: Long,
    ) {
        val targetCellX = chunk.chunkX.floorDiv(PORTAL_GRID.spacing)
        val targetCellZ = chunk.chunkZ.floorDiv(PORTAL_GRID.spacing)
        val clip = chunkClip(chunk)
        for (cellOffsetX in -1..1) {
            for (cellOffsetZ in -1..1) {
                val (candidateX, candidateZ) =
                    PORTAL_GRID.candidateChunk(
                        seed,
                        (targetCellX + cellOffsetX) * PORTAL_GRID.spacing,
                        (targetCellZ + cellOffsetZ) * PORTAL_GRID.spacing,
                    )
                val start = createStart(seed, candidateX, candidateZ) ?: continue
                if (!start.effectIntersects(clip)) continue
                applyReplay(chunk, replay(seed, candidateX, candidateZ, start))
            }
        }
    }

    internal fun startSnapshot(
        seed: Long,
        candidateChunkX: Int,
        candidateChunkZ: Int,
    ): NetherRuinedPortalStartSnapshot? =
        createStart(seed, candidateChunkX, candidateChunkZ)?.let { start ->
            NetherRuinedPortalStartSnapshot(
                start.templateName,
                start.position,
                start.rotation,
                start.mirror,
                start.boundingBox,
                start.airPocket,
            )
        }

    private fun createStart(
        seed: Long,
        candidateChunkX: Int,
        candidateChunkZ: Int,
    ): PortalStart? {
        val random = WorldgenRandom(LegacyRandom(0L))
        random.setLargeFeatureSeed(seed, candidateChunkX, candidateChunkZ)
        val airPocket = random.nextFloat() < AIR_POCKET_PROBABILITY
        val templateName =
            if (random.nextFloat() < GIANT_PORTAL_PROBABILITY) {
                GIANT_PORTALS[random.nextInt(GIANT_PORTALS.size)]
            } else {
                PORTALS[random.nextInt(PORTALS.size)]
            }
        val template = templateLoader(templateName) ?: return null
        val rotation = StructureRotation.entries[random.nextInt(StructureRotation.entries.size)]
        val mirror = if (random.nextFloat() < 0.5f) StructureMirror.NONE else StructureMirror.FRONT_BACK
        val pivot = StructurePos(template.size.x / 2, 0, template.size.z / 2)
        val basePosition = StructurePos(candidateChunkX * 16, 0, candidateChunkZ * 16)
        val unprojectedBox = template.getBoundingBox(basePosition, rotation, pivot, mirror)
        val projectedY = findSuitableY(random, airPocket, unprojectedBox)
        val position = StructurePos(basePosition.x, projectedY, basePosition.z)
        if (biomeSource.biomeAt(position.x, position.y, position.z).id.path !in NETHER_BIOMES) return null
        return PortalStart(
            templateName,
            template,
            position,
            rotation,
            mirror,
            pivot,
            unprojectedBox.move(StructurePos(0, projectedY, 0)),
            airPocket,
        )
    }

    private fun findSuitableY(
        random: WorldgenRandom,
        airPocket: Boolean,
        boundingBox: StructureBoundingBox,
    ): Int {
        val selectedY =
            if (airPocket) {
                32 + random.nextInt(69)
            } else if (random.nextFloat() < 0.5f) {
                27 + random.nextInt(3)
            } else {
                29 + random.nextInt(72)
            }
        val corners =
            listOf(
                boundingBox.minX to boundingBox.minZ,
                boundingBox.maxX to boundingBox.minZ,
                boundingBox.minX to boundingBox.maxZ,
                boundingBox.maxX to boundingBox.maxZ,
            )
        var y = selectedY
        while (y > MIN_PROJECTED_Y) {
            var solidCorners = 0
            for ((x, z) in corners) {
                if (!Blocks.isAir(baseStateAt(x, y, z)) && ++solidCorners == 3) return y
            }
            y--
        }
        return y
    }

    private fun replay(
        seed: Long,
        candidateChunkX: Int,
        candidateChunkZ: Int,
        start: PortalStart,
    ): PortalReplay {
        val key = PortalCacheKey(seed, candidateChunkX, candidateChunkZ)
        replayCache.value[key]?.let { return it }
        val created = createReplay(seed, start)
        while (true) {
            val current = replayCache.value
            current[key]?.let { return it }
            if (replayCache.compareAndSet(current, current + (key to created))) return created
        }
    }

    private fun createReplay(
        seed: Long,
        start: PortalStart,
    ): PortalReplay {
        val center = start.boundingBox.center()
        val minChunkX = (center.x - SPREAD_RADIUS).floorDiv(16)
        val maxChunkX = (center.x + SPREAD_RADIUS).floorDiv(16)
        val minChunkZ = (center.z - SPREAD_RADIUS).floorDiv(16)
        val maxChunkZ = (center.z + SPREAD_RADIUS).floorDiv(16)
        val chunks = mutableMapOf<Pair<Int, Int>, Chunk>()
        for (chunkX in minChunkX..maxChunkX) {
            for (chunkZ in minChunkZ..maxChunkZ) {
                val chunk = Chunk(chunkX, chunkZ, ChunkHeight.NETHER)
                fillBaseTerrain(chunk)
                NetherCaveCarver.carve(chunk, seed)
                placeEarlierBastions(chunk, seed)
                chunks[chunkX to chunkZ] = chunk
            }
        }
        val world = PortalWorld(chunks, baseStateAt)
        val centerChunkX = center.x.floorDiv(16)
        val centerChunkZ = center.z.floorDiv(16)
        val placementRandom = WorldgenRandom(XoroshiroRandom(seed))
        val decorationSeed = placementRandom.setDecorationSeed(seed, centerChunkX * 16, centerChunkZ * 16)
        placementRandom.setFeatureSeed(decorationSeed, RUINED_PORTAL_STRUCTURE_INDEX, SURFACE_STRUCTURES_STEP)

        placeTemplate(start, world, placementRandom)
        spreadNetherrack(start.boundingBox, world, placementRandom)
        addNetherrackDripColumnsBelowPortal(start.boundingBox, world, placementRandom)
        val survivingEntities =
            world.blockEntities.filter { entity ->
                Blocks.byStateId(world.stateAt(entity.position.x, entity.position.y, entity.position.z))?.block?.path ==
                    "chest"
            }
        return PortalReplay(world.writes.toMap(), survivingEntities)
    }

    private fun fillBaseTerrain(chunk: Chunk) {
        for (y in 0 until NETHER_GENERATION_DEPTH) {
            for (z in 0 until 16) {
                for (x in 0 until 16) {
                    val stateId = baseStateAt(chunk.chunkX * 16 + x, y, chunk.chunkZ * 16 + z)
                    if (!Blocks.isAir(stateId)) chunk.setBlockStateId(x, y, z, stateId)
                }
            }
        }
        for (z in 0 until 16) {
            for (x in 0 until 16) {
                chunk.setBlockStateId(x, 0, z, Blocks.BEDROCK.stateId)
                chunk.setBlockStateId(x, NETHER_GENERATION_DEPTH - 1, z, Blocks.BEDROCK.stateId)
            }
        }
    }

    private fun placeTemplate(
        start: PortalStart,
        world: PortalWorld,
        random: WorldgenRandom,
    ) {
        val processed = mutableListOf<ProcessedPortalBlock>()
        for (blockInfo in start.template.blocks) {
            val input = PortalState.from(blockInfo.stateId, blockInfo.properties)
            if (input.path == "structure_block") continue
            if (!start.airPocket && input.isAir) continue
            val worldPos =
                org.basaltmc.world.structure.StructureTransform.transform(
                    blockInfo.pos,
                    start.mirror,
                    start.rotation,
                    start.pivot,
                ) + start.position
            val oldStateId = world.stateAt(worldPos.x, worldPos.y, worldPos.z)
            if (Blocks.byStateId(oldStateId)?.block?.path in PROTECTED_BLOCKS) continue
            var state = applyRuleProcessor(input, worldPos)
            state = applyBlockAgeProcessor(state, worldPos)
            if (Blocks.byStateId(oldStateId)?.block?.path == "lava" && !state.hasFullCollisionShape) {
                state = PortalState.of("lava")
            }
            state = applyBlackstoneReplacement(state)
            processed +=
                ProcessedPortalBlock(
                    worldPos,
                    state.transform(start.mirror, start.rotation).stateId,
                    blockInfo.blockEntityNbt,
                )
        }

        for (block in processed) {
            world.setBlock(block.position, block.stateId)
            val statePath = Blocks.byStateId(block.stateId)?.block?.path
            if (statePath == "chest" && block.nbt != null) {
                val data = block.nbt.deepCopy()
                data.putLong("LootTableSeed", random.nextLong())
                if ("components" !in data) data.putCompound("components", NbtCompound())
                world.blockEntities += PortalBlockEntity(block.position, data)
            }
        }

        // parity: TemplateStructurePiece performs this second pass after placeInWorld, outside the
        // processor chain, so even a processor-rejected jigsaw position receives final_state.
        for (blockInfo in start.template.blocks) {
            if (PortalState.from(blockInfo.stateId, blockInfo.properties).path != "jigsaw") continue
            val position =
                org.basaltmc.world.structure.StructureTransform.transform(
                    blockInfo.pos,
                    start.mirror,
                    start.rotation,
                    start.pivot,
                ) + start.position
            val finalState = blockInfo.blockEntityNbt?.getString("final_state", "minecraft:air") ?: "minecraft:air"
            world.setBlock(position, stateId(finalState.substringBefore('[').substringAfter(':')))
        }
    }

    private fun applyRuleProcessor(
        input: PortalState,
        position: StructurePos,
    ): PortalState {
        val random = positionalRandom(position)
        return when (input.path) {
            "gold_block" -> if (random.nextFloat() < 0.3f) PortalState.of("air") else input
            "lava" -> if (random.nextFloat() < 0.2f) PortalState.of("magma_block") else input
            "netherrack" -> if (random.nextFloat() < MAGMA_PROBABILITY) PortalState.of("magma_block") else input
            else -> input
        }
    }

    private fun applyBlockAgeProcessor(
        input: PortalState,
        position: StructurePos,
    ): PortalState {
        val random = positionalRandom(position)
        return when (input.path) {
            "stone_bricks", "stone", "chiseled_stone_bricks" -> {
                if (random.nextFloat() >= 0.5f) return input
                val nonMossyStairs = randomStairs(random, "stone_brick_stairs")
                randomStairs(random, "mossy_stone_brick_stairs")
                random.nextFloat()
                if (random.nextInt(2) == 0) PortalState.of("cracked_stone_bricks") else nonMossyStairs
            }
            in STAIR_BLOCKS -> {
                if (random.nextFloat() >= 0.5f) return input
                random.nextFloat()
                if (random.nextInt(2) ==
                    0
                ) {
                    PortalState.bottomSlab("stone_slab")
                } else {
                    PortalState.bottomSlab("stone_brick_slab")
                }
            }
            in SLAB_BLOCKS, in WALL_BLOCKS -> {
                random.nextFloat()
                input
            }
            "obsidian" -> if (random.nextFloat() < 0.15f) PortalState.of("crying_obsidian") else input
            else -> input
        }
    }

    private fun randomStairs(
        random: WorldgenRandom,
        path: String,
    ): PortalState =
        PortalState(
            path,
            linkedMapOf(
                "facing" to listOf("north", "east", "south", "west")[random.nextInt(4)],
                "half" to listOf("top", "bottom")[random.nextInt(2)],
                "shape" to "straight",
                "waterlogged" to "false",
            ),
        )

    private fun applyBlackstoneReplacement(input: PortalState): PortalState {
        val replacement = BLACKSTONE_REPLACEMENTS[input.path] ?: return input
        val properties =
            when {
                "facing" in input.properties && "half" in input.properties ->
                    linkedMapOf(
                        "facing" to input.properties.getValue("facing"),
                        "half" to input.properties.getValue("half"),
                        "shape" to "straight",
                        "waterlogged" to "false",
                    )
                "type" in input.properties ->
                    linkedMapOf(
                        "type" to input.properties.getValue("type"),
                        "waterlogged" to "false",
                    )
                else -> emptyMap()
            }
        return PortalState(replacement, properties)
    }

    private fun spreadNetherrack(
        boundingBox: StructureBoundingBox,
        world: PortalWorld,
        random: WorldgenRandom,
    ) {
        val center = boundingBox.center()
        val averageWidth = (boundingBox.xSpan + boundingBox.zSpan) / 2
        val distanceAdjustment = random.nextInt(maxOf(1, 8 - averageWidth / 2))
        for (x in center.x - SPREAD_RADIUS..center.x + SPREAD_RADIUS) {
            for (z in center.z - SPREAD_RADIUS..center.z + SPREAD_RADIUS) {
                val adjustedDistance = abs(x - center.x) + abs(z - center.z) + distanceAdjustment
                if (adjustedDistance >= SPREAD_RADIUS) continue
                if (random.nextDouble() >= NETHERRACK_PROBABILITY_BY_DISTANCE[adjustedDistance].toDouble()) continue
                val y = min(boundingBox.minY, world.surfaceY(x, z))
                val position = StructurePos(x, y, z)
                if (abs(y - boundingBox.minY) > 3 || !canReplaceWithNetherrack(world.stateAt(x, y, z))) continue
                placeNetherrackOrMagma(random, world, position)
                addNetherrackDripColumn(random, world, StructurePos(x, y - 1, z))
            }
        }
    }

    private fun addNetherrackDripColumnsBelowPortal(
        boundingBox: StructureBoundingBox,
        world: PortalWorld,
        random: WorldgenRandom,
    ) {
        for (x in boundingBox.minX + 1 until boundingBox.maxX) {
            for (z in boundingBox.minZ + 1 until boundingBox.maxZ) {
                if (Blocks.byStateId(world.stateAt(x, boundingBox.minY, z))?.block?.path == "netherrack") {
                    addNetherrackDripColumn(random, world, StructurePos(x, boundingBox.minY - 1, z))
                }
            }
        }
    }

    private fun addNetherrackDripColumn(
        random: WorldgenRandom,
        world: PortalWorld,
        start: StructurePos,
    ) {
        var position = start
        placeNetherrackOrMagma(random, world, position)
        var remaining = 8
        while (remaining > 0 && random.nextFloat() < 0.5f) {
            position = StructurePos(position.x, position.y - 1, position.z)
            remaining--
            placeNetherrackOrMagma(random, world, position)
        }
    }

    private fun placeNetherrackOrMagma(
        random: WorldgenRandom,
        world: PortalWorld,
        position: StructurePos,
    ) {
        world.setBlock(position, if (random.nextFloat() < MAGMA_PROBABILITY) MAGMA else NETHERRACK)
    }

    private fun canReplaceWithNetherrack(stateId: Int): Boolean {
        val path = Blocks.byStateId(stateId)?.block?.path ?: return false
        return path != "air" && path != "obsidian" && path !in PROTECTED_BLOCKS
    }

    private fun applyReplay(
        chunk: Chunk,
        replay: PortalReplay,
    ) {
        for ((position, stateId) in replay.writes) {
            if (position.x.floorDiv(16) != chunk.chunkX || position.z.floorDiv(16) != chunk.chunkZ) continue
            if (position.y !in chunk.verticalBounds.minY..chunk.verticalBounds.maxY) continue
            chunk.setBlockStateId(position.x and 15, position.y, position.z and 15, stateId)
        }
        for (entity in replay.blockEntities) {
            val position = entity.position
            if (position.x.floorDiv(16) != chunk.chunkX || position.z.floorDiv(16) != chunk.chunkZ) continue
            if (position.y !in chunk.verticalBounds.minY..chunk.verticalBounds.maxY) continue
            chunk.setBlockEntity(
                ChunkBlockEntity(
                    position.x and 15,
                    position.y,
                    position.z and 15,
                    Identifier.vanilla("chest"),
                    entity.data,
                ),
            )
        }
    }

    private fun positionalRandom(position: StructurePos): WorldgenRandom =
        WorldgenRandom(LegacyRandom(PositionalHashing.getSeed(position.x, position.y, position.z)))

    private fun chunkClip(chunk: Chunk): StructureBoundingBox =
        StructureBoundingBox(
            chunk.chunkX * 16,
            chunk.verticalBounds.minY,
            chunk.chunkZ * 16,
            chunk.chunkX * 16 + 15,
            chunk.verticalBounds.maxY,
            chunk.chunkZ * 16 + 15,
        )

    private class PortalWorld(
        private val chunks: Map<Pair<Int, Int>, Chunk>,
        private val baseStateAt: (Int, Int, Int) -> Int,
    ) {
        val writes: LinkedHashMap<StructurePos, Int> = linkedMapOf()
        val blockEntities: MutableList<PortalBlockEntity> = mutableListOf()

        fun stateAt(
            x: Int,
            y: Int,
            z: Int,
        ): Int {
            val position = StructurePos(x, y, z)
            writes[position]?.let { return it }
            if (y !in 0 until NETHER_GENERATION_DEPTH) return Blocks.AIR.stateId
            val chunk = chunks[x.floorDiv(16) to z.floorDiv(16)]
            return chunk?.getBlockStateId(x and 15, y, z and 15) ?: baseStateAt(x, y, z)
        }

        fun setBlock(
            position: StructurePos,
            stateId: Int,
        ) {
            if (position.y in ChunkHeight.NETHER.minY..ChunkHeight.NETHER.maxY) writes[position] = stateId
        }

        fun surfaceY(
            x: Int,
            z: Int,
        ): Int {
            for (y in NETHER_GENERATION_DEPTH - 1 downTo 0) if (!Blocks.isAir(stateAt(x, y, z))) return y
            return -1
        }
    }

    private data class PortalStart(
        val templateName: String,
        val template: StructureTemplate,
        val position: StructurePos,
        val rotation: StructureRotation,
        val mirror: StructureMirror,
        val pivot: StructurePos,
        val boundingBox: StructureBoundingBox,
        val airPocket: Boolean,
    ) {
        fun effectIntersects(clip: StructureBoundingBox): Boolean {
            val center = boundingBox.center()
            return center.x - SPREAD_RADIUS <= clip.maxX &&
                center.x + SPREAD_RADIUS >= clip.minX &&
                center.z - SPREAD_RADIUS <= clip.maxZ &&
                center.z + SPREAD_RADIUS >= clip.minZ
        }
    }

    private data class PortalCacheKey(
        val seed: Long,
        val chunkX: Int,
        val chunkZ: Int,
    )

    private data class PortalReplay(
        val writes: Map<StructurePos, Int>,
        val blockEntities: List<PortalBlockEntity>,
    )

    private data class PortalBlockEntity(
        val position: StructurePos,
        val data: NbtCompound,
    )

    private data class ProcessedPortalBlock(
        val position: StructurePos,
        val stateId: Int,
        val nbt: NbtCompound?,
    )

    private data class PortalState(
        val path: String,
        val properties: Map<String, String> = emptyMap(),
    ) {
        val isAir: Boolean get() = path == "air" || path == "cave_air" || path == "void_air"

        val stateId: Int
            get() {
                val identifier = Identifier.vanilla(path)
                return Blocks.stateIdOf(identifier, properties)
                    ?: Blocks.REGISTRY.byIdentifier(identifier)?.stateId
                    ?: Blocks.AIR.stateId
            }

        val hasFullCollisionShape: Boolean
            get() =
                !isAir &&
                    path !in NON_FULL_BLOCKS &&
                    !(path.endsWith("_slab") && properties["type"] != "double") &&
                    !path.endsWith("_stairs") &&
                    !path.endsWith("_wall")

        fun transform(
            mirror: StructureMirror,
            rotation: StructureRotation,
        ): PortalState {
            val facing = properties["facing"] ?: return this
            val transformed =
                StructureDirection
                    .fromName(facing)
                    ?.mirror(mirror)
                    ?.rotate(rotation)
                    ?.name
                    ?.lowercase()
                    ?: return this
            return copy(properties = properties + ("facing" to transformed))
        }

        companion object {
            fun from(
                stateId: Int,
                templateProperties: Map<String, String>,
            ): PortalState {
                val state = Blocks.byStateId(stateId) ?: return of("air")
                return PortalState(state.block.path, templateProperties.ifEmpty { state.properties })
            }

            fun of(path: String): PortalState = PortalState(path)

            fun bottomSlab(path: String): PortalState =
                PortalState(path, linkedMapOf("type" to "bottom", "waterlogged" to "false"))
        }
    }

    private companion object {
        const val NETHER_GENERATION_DEPTH = 128
        const val MIN_PROJECTED_Y = 15
        const val SPREAD_RADIUS = 14
        const val AIR_POCKET_PROBABILITY = 0.5f
        const val GIANT_PORTAL_PROBABILITY = 0.05f
        const val MAGMA_PROBABILITY = 0.07f
        const val RUINED_PORTAL_STRUCTURE_INDEX = 14
        const val SURFACE_STRUCTURES_STEP = 4
        val PORTAL_GRID = StructurePlacementGrid(spacing = 40, separation = 15, salt = 34222645)
        val PORTALS = (1..10).map { "minecraft:ruined_portal/portal_$it" }
        val GIANT_PORTALS = (1..3).map { "minecraft:ruined_portal/giant_portal_$it" }
        val NETHER_BIOMES =
            setOf("crimson_forest", "nether_wastes", "soul_sand_valley", "warped_forest", "basalt_deltas")
        val NETHERRACK_PROBABILITY_BY_DISTANCE =
            floatArrayOf(1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 0.9f, 0.9f, 0.8f, 0.7f, 0.6f, 0.4f, 0.2f)
        val PROTECTED_BLOCKS =
            setOf("bedrock", "spawner", "chest", "end_portal_frame", "reinforced_deepslate", "trial_spawner", "vault")
        val STAIR_BLOCKS =
            setOf(
                "cobblestone_stairs",
                "mossy_cobblestone_stairs",
                "stone_stairs",
                "stone_brick_stairs",
                "mossy_stone_brick_stairs",
            )
        val SLAB_BLOCKS =
            setOf(
                "cobblestone_slab",
                "mossy_cobblestone_slab",
                "smooth_stone_slab",
                "stone_slab",
                "stone_brick_slab",
                "mossy_stone_brick_slab",
            )
        val WALL_BLOCKS =
            setOf("stone_brick_wall", "mossy_stone_brick_wall", "cobblestone_wall", "mossy_cobblestone_wall")
        val BLACKSTONE_REPLACEMENTS =
            mapOf(
                "cobblestone" to "blackstone",
                "mossy_cobblestone" to "blackstone",
                "stone" to "polished_blackstone",
                "stone_bricks" to "polished_blackstone_bricks",
                "mossy_stone_bricks" to "polished_blackstone_bricks",
                "cobblestone_stairs" to "blackstone_stairs",
                "mossy_cobblestone_stairs" to "blackstone_stairs",
                "stone_stairs" to "polished_blackstone_stairs",
                "stone_brick_stairs" to "polished_blackstone_brick_stairs",
                "mossy_stone_brick_stairs" to "polished_blackstone_brick_stairs",
                "cobblestone_slab" to "blackstone_slab",
                "mossy_cobblestone_slab" to "blackstone_slab",
                "smooth_stone_slab" to "polished_blackstone_slab",
                "stone_slab" to "polished_blackstone_slab",
                "stone_brick_slab" to "polished_blackstone_brick_slab",
                "mossy_stone_brick_slab" to "polished_blackstone_brick_slab",
                "stone_brick_wall" to "polished_blackstone_brick_wall",
                "mossy_stone_brick_wall" to "polished_blackstone_brick_wall",
                "cobblestone_wall" to "blackstone_wall",
                "mossy_cobblestone_wall" to "blackstone_wall",
                "chiseled_stone_bricks" to "chiseled_polished_blackstone",
                "cracked_stone_bricks" to "cracked_polished_blackstone_bricks",
                "iron_bars" to "iron_chain",
            )
        val NON_FULL_BLOCKS = setOf("lava", "water", "iron_chain", "iron_bars", "chest")
        val NETHERRACK = stateId("netherrack")
        val MAGMA = stateId("magma_block")

        fun StructureBoundingBox.center(): StructurePos =
            StructurePos(
                minX + (maxX - minX + 1) / 2,
                minY + (maxY - minY + 1) / 2,
                minZ + (maxZ - minZ + 1) / 2,
            )

        val StructureBoundingBox.xSpan: Int get() = maxX - minX + 1
        val StructureBoundingBox.zSpan: Int get() = maxZ - minZ + 1

        fun stateId(path: String): Int =
            Blocks.REGISTRY.byIdentifier(Identifier.vanilla(path))?.stateId
                ?: error("missing vanilla block minecraft:$path")
    }
}

internal data class NetherRuinedPortalStartSnapshot(
    val templateName: String,
    val position: StructurePos,
    val rotation: StructureRotation,
    val mirror: StructureMirror,
    val boundingBox: StructureBoundingBox,
    val airPocket: Boolean,
)
