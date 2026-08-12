package org.basaltmc.server.world.structure

import org.basaltmc.server.random.WorldgenRandom
import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.world.Chunk
import org.basaltmc.server.world.ChunkBlockEntity
import org.basaltmc.server.world.ChunkGeneratedEntity
import org.basaltmc.server.world.WorldHeight
import org.basaltmc.wire.common.Identifier
import org.basaltmc.wire.nbt.NbtCompound
import org.basaltmc.wire.nbt.NbtDouble
import org.basaltmc.wire.nbt.NbtFloat
import org.basaltmc.wire.nbt.NbtInt
import org.basaltmc.wire.nbt.NbtList
import org.basaltmc.wire.nbt.NbtString
import org.basaltmc.wire.nbt.NbtType
import org.basaltmc.wire.nbt.deepCopy

/**
 * // parity: net.minecraft.world.level.levelgen.structure.templatesystem.StructureTemplate,
 * scoped to what this project can currently represent. The NBT `Properties` compound is retained
 * and resolved through [Blocks]; facing, axis, cardinal connections, numeric rotations and rail
 * shapes are mirrored/rotated at placement. Multiple alternative palettes (real vanilla's
 * `palettes` list, used by a handful of non-village structures for random variation) are not
 * modeled. Block entity NBT is preserved for blocks Basalt can store, including vanilla loot seeds
 * for random containers, and template entities are emitted as persistent generated descriptors.
 */
public class StructureTemplate {
    public var size: StructurePos = StructurePos(0, 0, 0)
        private set

    private val blocksBacking: MutableList<StructureBlockInfo> = mutableListOf()
    private val entitiesBacking: MutableList<StructureEntityInfo> = mutableListOf()

    public val blocks: List<StructureBlockInfo> get() = blocksBacking

    public val entities: List<StructureEntityInfo> get() = entitiesBacking

    public fun load(nbt: NbtCompound) {
        blocksBacking.clear()
        entitiesBacking.clear()
        val sizeList = nbt.getList("size")
        size =
            StructurePos(
                (sizeList?.get(0) as? NbtInt)?.value ?: 0,
                (sizeList?.get(1) as? NbtInt)?.value ?: 0,
                (sizeList?.get(2) as? NbtInt)?.value ?: 0,
            )

        val paletteList = nbt.getList("palette") ?: return
        val blockList = nbt.getList("blocks") ?: return
        val paletteProperties =
            Array(paletteList.size) { i ->
                val entry = paletteList[i] as NbtCompound
                val properties = entry["Properties"] as? NbtCompound ?: return@Array emptyMap()
                properties.mapNotNull { (name, value) -> (value as? NbtString)?.value?.let { name to it } }.toMap()
            }
        val paletteStateIds =
            IntArray(paletteList.size) { i ->
                val entry = paletteList[i] as NbtCompound
                val name = entry.getString("Name")
                val identifier = Identifier.parse(name)
                Blocks.stateIdOf(identifier, paletteProperties[i])
                    ?: Blocks.REGISTRY.byIdentifier(identifier)?.stateId
                    ?: Blocks.AIR.stateId
            }
        // parity: JigsawBlock's `orientation` blockstate property (`front_top`, front first):
        // the one block property this project reads, since jigsaw assembly cannot work without it.
        val paletteFrontFacing =
            Array(paletteList.size) { i ->
                val entry = paletteList[i] as NbtCompound
                if (entry.getString("Name") != "minecraft:jigsaw") return@Array null
                val orientation = (entry["Properties"] as? NbtCompound)?.getString("orientation") ?: return@Array null
                StructureDirection.fromName(orientation.substringBefore('_'))
            }
        val paletteTopFacing =
            Array(paletteList.size) { i ->
                val entry = paletteList[i] as NbtCompound
                if (entry.getString("Name") != "minecraft:jigsaw") return@Array null
                val orientation = (entry["Properties"] as? NbtCompound)?.getString("orientation") ?: return@Array null
                StructureDirection.fromName(orientation.substringAfter('_'))
            }

        for (i in 0 until blockList.size) {
            val blockTag = blockList[i] as NbtCompound
            val posList = blockTag.getList("pos") ?: continue
            val pos =
                StructurePos(
                    (posList[0] as NbtInt).value,
                    (posList[1] as NbtInt).value,
                    (posList[2] as NbtInt).value,
                )
            val paletteIndex = blockTag.getInt("state")
            val stateId = paletteStateIds.getOrElse(paletteIndex) { Blocks.AIR.stateId }
            val blockEntityNbt = blockTag["nbt"] as? NbtCompound
            val frontFacing = paletteFrontFacing.getOrNull(paletteIndex)
            val topFacing = paletteTopFacing.getOrNull(paletteIndex)
            blocksBacking.add(
                StructureBlockInfo(
                    pos,
                    stateId,
                    blockEntityNbt,
                    frontFacing,
                    topFacing,
                    paletteProperties.getOrElse(paletteIndex) { emptyMap() },
                ),
            )
        }
        val entityList = nbt.getList("entities")
        if (entityList != null) {
            for (entry in entityList) {
                val entityTag = entry as? NbtCompound ?: continue
                val pos = entityTag.getList("pos") ?: continue
                val blockPos = entityTag.getList("blockPos") ?: continue
                if (pos.size < 3 || blockPos.size < 3) continue
                val data = entityTag["nbt"] as? NbtCompound ?: continue
                entitiesBacking +=
                    StructureEntityInfo(
                        (pos[0] as? NbtDouble)?.value ?: continue,
                        (pos[1] as? NbtDouble)?.value ?: continue,
                        (pos[2] as? NbtDouble)?.value ?: continue,
                        StructurePos(
                            (blockPos[0] as? NbtInt)?.value ?: continue,
                            (blockPos[1] as? NbtInt)?.value ?: continue,
                            (blockPos[2] as? NbtInt)?.value ?: continue,
                        ),
                        data,
                    )
            }
        }
    }

    /** // parity: StructureTemplate.getJigsaws, world-space and rotated/mirrored. */
    public fun jigsawBlocks(
        position: StructurePos,
        rotation: StructureRotation,
        mirror: StructureMirror,
        pivot: StructurePos,
    ): List<PlacedJigsaw> =
        blocksBacking.mapNotNull { blockInfo ->
            val front = blockInfo.frontFacing ?: return@mapNotNull null
            val nbt = blockInfo.blockEntityNbt ?: return@mapNotNull null
            val worldPos = StructureTransform.transform(blockInfo.pos, mirror, rotation, pivot) + position
            val top = blockInfo.topFacing ?: return@mapNotNull null
            val joint = nbt.getString("joint")
            PlacedJigsaw(
                pos = worldPos,
                front = front.mirror(mirror).rotate(rotation),
                top = top.mirror(mirror).rotate(rotation),
                rollable = joint == "rollable" || joint.isEmpty() && !front.isHorizontal,
                name = nbt.getString("name", "minecraft:empty"),
                pool = nbt.getString("pool", "minecraft:empty"),
                target = nbt.getString("target", "minecraft:empty"),
                finalStateName = nbt.getString("final_state", "minecraft:air"),
                placementPriority = nbt.getInt("placement_priority", 0),
                selectionPriority = nbt.getInt("selection_priority", 0),
            )
        }

    /** // parity: TemplateStructurePiece's transformed DATA-mode structure-block markers. */
    public fun dataMarkers(
        position: StructurePos,
        rotation: StructureRotation,
        mirror: StructureMirror,
        pivot: StructurePos,
    ): List<StructureDataMarker> =
        blocksBacking.mapNotNull { blockInfo ->
            val block = Blocks.byStateId(blockInfo.stateId)?.block ?: return@mapNotNull null
            if (block.path != "structure_block") return@mapNotNull null
            val nbt = blockInfo.blockEntityNbt ?: return@mapNotNull null
            if (nbt.getString("mode").uppercase() != "DATA") return@mapNotNull null
            StructureDataMarker(
                StructureTransform.transform(blockInfo.pos, mirror, rotation, pivot) + position,
                nbt.getString("metadata"),
            )
        }

    public fun getBoundingBox(
        position: StructurePos,
        rotation: StructureRotation,
        pivot: StructurePos,
        mirror: StructureMirror,
    ): StructureBoundingBox {
        val delta = StructurePos(size.x - 1, size.y - 1, size.z - 1)
        val corner1 = StructureTransform.transform(StructurePos(0, 0, 0), mirror, rotation, pivot)
        val corner2 = StructureTransform.transform(delta, mirror, rotation, pivot)
        return StructureBoundingBox.fromCorners(corner1, corner2).move(position)
    }

    /**
     * // parity: StructureTemplate.placeInWorld, narrowed only by the missing fluid spread.
     * [clipTo] mirrors real
     * vanilla's own per-chunk `chunkBb` clipping (`StructureTemplate.processBlockInfos`): a piece
     * spanning several chunks is placed once per intersecting chunk, each time keeping only the
     * blocks that land inside that one chunk.
     */
    public fun placeInto(
        chunk: Chunk,
        position: StructurePos,
        rotation: StructureRotation,
        mirror: StructureMirror,
        pivot: StructurePos,
        clipTo: StructureBoundingBox,
        processors: StructureProcessorList = StructureProcessorList.EMPTY,
        random: WorldgenRandom? = null,
        ignoreStructureBlocks: Boolean = false,
        ignoreAir: Boolean = false,
        gravityOffset: Int? = null,
    ) {
        val processedBlocks =
            blocksBacking.mapNotNull { blockInfo ->
                if (
                    (ignoreAir && Blocks.isAir(blockInfo.stateId)) ||
                    (ignoreStructureBlocks && Blocks.byStateId(blockInfo.stateId)?.block?.path == "structure_block")
                ) {
                    return@mapNotNull null
                }
                val originalWorldPos = StructureTransform.transform(blockInfo.pos, mirror, rotation, pivot) + position
                // parity: processBlockInfos first rejects blocks outside chunkBb, before any processor
                // (including GravityProcessor) gets to move them.
                if (!clipTo.isInside(originalWorldPos)) return@mapNotNull null
                val localX = originalWorldPos.x and 15
                val localZ = originalWorldPos.z and 15
                val worldPos =
                    if (gravityOffset != null) {
                        // parity: Projection.TERRAIN_MATCHING appends
                        // GravityProcessor(WORLD_SURFACE_WG, -1). It uses the unrotated template Y,
                        // not the piece origin or transformed Y.
                        originalWorldPos.copy(
                            y =
                                chunk.heightAt(localX, localZ, Chunk.HeightmapType.WORLD_SURFACE) +
                                    gravityOffset + blockInfo.pos.y,
                        )
                    } else {
                        originalWorldPos
                    }
                if (!clipTo.isInside(worldPos) || worldPos.y !in WorldHeight.MIN_Y..WorldHeight.MAX_Y) {
                    return@mapNotNull null
                }
                ProcessedBlock(blockInfo, originalWorldPos, worldPos)
            }

        // `processBlockInfos` completes the whole list before `placeInWorld` writes the first block.
        // In particular, one projected block cannot raise the height sampled by another block in
        // the same template.
        for ((blockInfo, processorPos, worldPos) in processedBlocks) {
            val processorLocalX = processorPos.x and 15
            val processorLocalZ = processorPos.z and 15
            // parity: JigsawReplacementProcessor, applied to every jigsaw block unconditionally.
            val stateId =
                if (blockInfo.frontFacing != null) {
                    val finalStateName =
                        blockInfo.blockEntityNbt?.getString("final_state", "minecraft:air") ?: "minecraft:air"
                    if (Identifier.parse(finalStateName.substringBefore('[')).path == "structure_void") continue
                    parsedAndTransformedStateId(finalStateName, mirror, rotation)
                } else if (processors.rules.isNotEmpty()) {
                    val worldStateId = chunk.getBlockStateId(processorLocalX, processorPos.y, processorLocalZ)
                    val processedStateId = processors.processAt(blockInfo.stateId, worldStateId, processorPos)
                    val properties =
                        if (processedStateId == blockInfo.stateId) {
                            blockInfo.properties
                        } else {
                            Blocks.byStateId(processedStateId)?.properties.orEmpty()
                        }
                    transformedStateId(processedStateId, properties, mirror, rotation)
                } else {
                    transformedStateId(blockInfo.stateId, blockInfo.properties, mirror, rotation)
                }
            val localX = worldPos.x and 15
            val localZ = worldPos.z and 15
            chunk.setBlockStateId(localX, worldPos.y, localZ, stateId)
            placeBlockEntity(chunk, blockInfo, stateId, worldPos, random)
        }
        for (entityInfo in entitiesBacking) {
            val ownerPos = StructureTransform.transform(entityInfo.blockPos, mirror, rotation, pivot) + position
            if (!clipTo.isInside(ownerPos)) continue
            val transformed = transformEntityPosition(entityInfo.x, entityInfo.y, entityInfo.z, mirror, rotation, pivot)
            val worldX = transformed.first + position.x
            val worldY = transformed.second + position.y
            val worldZ = transformed.third + position.z
            val data = entityInfo.data.deepCopy()
            data.remove("UUID")
            data["Pos"] =
                NbtList(
                    NbtType.DOUBLE,
                    listOf(NbtDouble(worldX), NbtDouble(worldY), NbtDouble(worldZ)),
                )
            val sourceRotation = data.getList("Rotation")
            val sourceYaw =
                if (sourceRotation != null &&
                    sourceRotation.size > 0
                ) {
                    (sourceRotation[0] as? NbtFloat)?.value ?: 0f
                } else {
                    0f
                }
            val pitch =
                if (sourceRotation != null &&
                    sourceRotation.size > 1
                ) {
                    (sourceRotation[1] as? NbtFloat)?.value ?: 0f
                } else {
                    0f
                }
            val yaw = transformEntityYaw(sourceYaw, mirror, rotation)
            data["Rotation"] = NbtList(NbtType.FLOAT, listOf(NbtFloat(yaw), NbtFloat(pitch)))
            val typeName = data.getString("id")
            if (typeName.isEmpty()) continue
            chunk.addGeneratedEntity(
                ChunkGeneratedEntity(
                    Identifier.parse(typeName),
                    worldX,
                    worldY,
                    worldZ,
                    yaw,
                    pitch,
                    data,
                ),
            )
        }
    }

    private data class ProcessedBlock(
        val block: StructureBlockInfo,
        val processorPos: StructurePos,
        val placedPos: StructurePos,
    )

    private fun transformEntityPosition(
        sourceX: Double,
        sourceY: Double,
        sourceZ: Double,
        mirror: StructureMirror,
        rotation: StructureRotation,
        pivot: StructurePos,
    ): Triple<Double, Double, Double> {
        var x = sourceX
        var z = sourceZ
        when (mirror) {
            StructureMirror.LEFT_RIGHT -> z = 1.0 - z
            StructureMirror.FRONT_BACK -> x = 1.0 - x
            StructureMirror.NONE -> Unit
        }
        return when (rotation) {
            StructureRotation.COUNTERCLOCKWISE_90 ->
                Triple(pivot.x - pivot.z + z, sourceY, pivot.x + pivot.z + 1.0 - x)
            StructureRotation.CLOCKWISE_90 ->
                Triple(pivot.x + pivot.z + 1.0 - z, sourceY, pivot.z - pivot.x + x)
            StructureRotation.CLOCKWISE_180 ->
                Triple(pivot.x + pivot.x + 1.0 - x, sourceY, pivot.z + pivot.z + 1.0 - z)
            StructureRotation.NONE -> Triple(x, sourceY, z)
        }
    }

    private fun transformEntityYaw(
        sourceYaw: Float,
        mirror: StructureMirror,
        rotation: StructureRotation,
    ): Float {
        val mirrored =
            when (mirror) {
                StructureMirror.FRONT_BACK -> -sourceYaw
                StructureMirror.LEFT_RIGHT -> 180f - sourceYaw
                StructureMirror.NONE -> sourceYaw
            }
        val mirrorOffset = mirrored - sourceYaw
        val rotationOffset =
            when (rotation) {
                StructureRotation.NONE -> 0f
                StructureRotation.CLOCKWISE_90 -> 90f
                StructureRotation.CLOCKWISE_180 -> 180f
                StructureRotation.COUNTERCLOCKWISE_90 -> 270f
            }
        return sourceYaw + rotationOffset + mirrorOffset
    }

    private fun parsedAndTransformedStateId(
        stateName: String,
        mirror: StructureMirror,
        rotation: StructureRotation,
    ): Int {
        val identifier = Identifier.parse(stateName.substringBefore('['))
        val properties =
            stateName
                .substringAfter('[', "")
                .substringBefore(']')
                .split(',')
                .mapNotNull { entry ->
                    val parts = entry.split('=', limit = 2)
                    if (parts.size == 2) parts[0].trim() to parts[1].trim() else null
                }.toMap()
        val stateId =
            Blocks.stateIdOf(identifier, properties)
                ?: Blocks.REGISTRY.byIdentifier(identifier)?.stateId
                ?: Blocks.AIR.stateId
        return transformedStateId(stateId, properties, mirror, rotation)
    }

    private fun transformedStateId(
        stateId: Int,
        properties: Map<String, String>,
        mirror: StructureMirror,
        rotation: StructureRotation,
    ): Int {
        val block = Blocks.byStateId(stateId)?.block ?: return stateId
        var transformed = properties
        properties["facing"]?.let { facing ->
            val transformedFacing =
                StructureDirection
                    .fromName(facing)
                    ?.mirror(mirror)
                    ?.rotate(rotation)
                    ?.name
                    ?.lowercase()
            if (transformedFacing != null) transformed = transformed + ("facing" to transformedFacing)
        }
        val axis = properties["axis"]
        if ((axis == "x" || axis == "z") && rotation in ROTATIONS_SWAPPING_HORIZONTAL_AXES) {
            transformed = transformed + ("axis" to if (axis == "x") "z" else "x")
        }
        val directionalConnections = HORIZONTAL_DIRECTIONS.filter { it in properties }
        if (directionalConnections.isNotEmpty()) {
            val remapped = transformed.toMutableMap()
            directionalConnections.forEach(remapped::remove)
            for (directionName in directionalConnections) {
                val targetName =
                    StructureDirection
                        .fromName(directionName)!!
                        .mirror(mirror)
                        .rotate(rotation)
                        .name
                        .lowercase()
                remapped[targetName] = properties.getValue(directionName)
            }
            transformed = remapped
        }
        properties["rotation"]?.toIntOrNull()?.let { originalRotation ->
            val mirrored =
                when (mirror) {
                    StructureMirror.LEFT_RIGHT -> (8 - signedRotation(originalRotation) + 16) % 16
                    StructureMirror.FRONT_BACK -> (16 - signedRotation(originalRotation)) % 16
                    StructureMirror.NONE -> originalRotation
                }
            val rotated =
                when (rotation) {
                    StructureRotation.NONE -> mirrored
                    StructureRotation.CLOCKWISE_90 -> (mirrored + 4) % 16
                    StructureRotation.CLOCKWISE_180 -> (mirrored + 8) % 16
                    StructureRotation.COUNTERCLOCKWISE_90 -> (mirrored + 12) % 16
                }
            transformed = transformed + ("rotation" to rotated.toString())
        }
        properties["shape"]?.let { shape ->
            rotateRailShape(shape, mirror, rotation)?.let { transformedShape ->
                transformed = transformed + ("shape" to transformedShape)
            }
        }
        return Blocks.stateIdOf(block, transformed) ?: stateId
    }

    private fun signedRotation(rotation: Int): Int = if (rotation > 8) rotation - 16 else rotation

    private fun rotateRailShape(
        shape: String,
        mirror: StructureMirror,
        rotation: StructureRotation,
    ): String? {
        if (shape.startsWith("ascending_")) {
            val direction = transformHorizontal(shape.substringAfter("ascending_"), mirror, rotation) ?: return null
            return "ascending_$direction"
        }
        val directions =
            when (shape) {
                "north_south" -> listOf("north", "south")
                "east_west" -> listOf("east", "west")
                "south_east" -> listOf("south", "east")
                "south_west" -> listOf("south", "west")
                "north_west" -> listOf("north", "west")
                "north_east" -> listOf("north", "east")
                else -> return null // stair shapes do not rotate independently of facing
            }.mapNotNull { transformHorizontal(it, mirror, rotation) }
                .toSet()
        return when (directions) {
            setOf("north", "south") -> "north_south"
            setOf("east", "west") -> "east_west"
            setOf("south", "east") -> "south_east"
            setOf("south", "west") -> "south_west"
            setOf("north", "west") -> "north_west"
            setOf("north", "east") -> "north_east"
            else -> null
        }
    }

    private fun transformHorizontal(
        direction: String,
        mirror: StructureMirror,
        rotation: StructureRotation,
    ): String? =
        StructureDirection
            .fromName(direction)
            ?.mirror(mirror)
            ?.rotate(rotation)
            ?.name
            ?.lowercase()

    private fun placeBlockEntity(
        chunk: Chunk,
        blockInfo: StructureBlockInfo,
        stateId: Int,
        worldPos: StructurePos,
        random: WorldgenRandom?,
    ) {
        val source = blockInfo.blockEntityNbt ?: return
        val finalPath = Blocks.byStateId(stateId)?.block?.path ?: return
        if (finalPath !in BLOCK_ENTITY_BLOCKS) return
        val typeName = source.getString("id")
        if (typeName.isEmpty() || typeName == "minecraft:jigsaw") return
        val data = source.deepCopy()
        if ("components" !in data) data.putCompound("components", NbtCompound())
        if (random != null && finalPath in RANDOMIZABLE_CONTAINER_BLOCKS) {
            data.putLong("LootTableSeed", random.nextLong())
        }
        chunk.setBlockEntity(
            ChunkBlockEntity(
                worldPos.x and 15,
                worldPos.y,
                worldPos.z and 15,
                Identifier.parse(typeName),
                data,
            ),
        )
    }

    private companion object {
        val HORIZONTAL_DIRECTIONS = listOf("north", "east", "south", "west")
        val ROTATIONS_SWAPPING_HORIZONTAL_AXES =
            setOf(StructureRotation.CLOCKWISE_90, StructureRotation.COUNTERCLOCKWISE_90)
        val DYE_COLORS =
            listOf(
                "white",
                "orange",
                "magenta",
                "light_blue",
                "yellow",
                "lime",
                "pink",
                "gray",
                "light_gray",
                "cyan",
                "purple",
                "blue",
                "brown",
                "green",
                "red",
                "black",
            )
        val RANDOMIZABLE_CONTAINER_BLOCKS =
            setOf(
                "barrel",
                "chest",
                "trapped_chest",
                "dispenser",
                "dropper",
                "hopper",
                "crafter",
                "shulker_box",
            ) + ("shulker_box".let { suffix -> DYE_COLORS.map { color -> "${color}_$suffix" } })
        val BLOCK_ENTITY_BLOCKS =
            RANDOMIZABLE_CONTAINER_BLOCKS +
                setOf(
                    "spawner",
                    "furnace",
                    "blast_furnace",
                    "smoker",
                    "brewing_stand",
                    "beacon",
                    "lectern",
                    "jukebox",
                    "campfire",
                    "soul_campfire",
                    "beehive",
                    "bee_nest",
                    "decorated_pot",
                    "chiseled_bookshelf",
                    "trial_spawner",
                    "vault",
                    "bell",
                ).plus(DYE_COLORS.flatMap { color -> listOf("${color}_banner", "${color}_wall_banner") })
                    .plus(
                        listOf(
                            "oak",
                            "spruce",
                            "birch",
                            "jungle",
                            "acacia",
                            "cherry",
                            "dark_oak",
                            "pale_oak",
                            "mangrove",
                            "bamboo",
                        ).flatMap { wood ->
                            listOf(
                                "${wood}_sign",
                                "${wood}_wall_sign",
                                "${wood}_hanging_sign",
                                "${wood}_wall_hanging_sign",
                            )
                        },
                    )
    }
}

public data class StructureBlockInfo(
    val pos: StructurePos,
    val stateId: Int,
    val blockEntityNbt: NbtCompound?,
    val frontFacing: StructureDirection?,
    val topFacing: StructureDirection?,
    val properties: Map<String, String> = emptyMap(),
)

/** // parity: StructureTemplate.StructureEntityInfo, before piece transform and chunk clipping. */
public data class StructureEntityInfo(
    val x: Double,
    val y: Double,
    val z: Double,
    val blockPos: StructurePos,
    val data: NbtCompound,
)

/** // parity: StructureTemplate.JigsawBlockInfo, world-space. [finalStateName] is what real vanilla's `JigsawReplacementProcessor` swaps the jigsaw block itself for once placement is done. */
public data class PlacedJigsaw(
    val pos: StructurePos,
    val front: StructureDirection,
    val top: StructureDirection,
    val rollable: Boolean,
    val name: String,
    val pool: String,
    val target: String,
    val finalStateName: String,
    val placementPriority: Int,
    val selectionPriority: Int,
)

/** A transformed structure-block DATA marker consumed by a template structure piece callback. */
public data class StructureDataMarker(
    val pos: StructurePos,
    val metadata: String,
)
