package org.basaltmc.world.structure

import org.basaltmc.nbt.NbtCompound
import org.basaltmc.nbt.NbtInt
import org.basaltmc.nbt.NbtString
import org.basaltmc.nbt.deepCopy
import org.basaltmc.random.WorldgenRandom
import org.basaltmc.registry.Blocks
import org.basaltmc.registry.Identifier
import org.basaltmc.world.Chunk
import org.basaltmc.world.ChunkBlockEntity
import org.basaltmc.world.WorldHeight

/**
 * // parity: net.minecraft.world.level.levelgen.structure.templatesystem.StructureTemplate,
 * scoped to what this project can currently represent. Real vanilla block states carry
 * properties (facing, waterlogged, axis...) that this project's [Blocks] registry only models for
 * the states worldgen currently writes. The NBT `Properties` compound is retained and resolved
 * whenever that exact derived state is registered; horizontal facings are also mirrored/rotated
 * at placement. Unregistered properties still fall back to the block's default state. Multiple
 * alternative palettes (real vanilla's `palettes` list, used
 * by a handful of non-village structures for random variation) are not modeled. Block entity NBT
 * is preserved for blocks Basalt can store, including vanilla loot seeds for random containers.
 */
public class StructureTemplate {
    public var size: StructurePos = StructurePos(0, 0, 0)
        private set

    private val blocksBacking: MutableList<StructureBlockInfo> = mutableListOf()

    public val blocks: List<StructureBlockInfo> get() = blocksBacking

    public fun load(nbt: NbtCompound) {
        blocksBacking.clear()
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
     * // parity: StructureTemplate.placeInWorld, narrowed to blocks and block entities (no fluid
     * spread or entities: documented gaps above). [clipTo] mirrors real
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
    ) {
        for (blockInfo in blocksBacking) {
            if (
                (ignoreAir && Blocks.isAir(blockInfo.stateId)) ||
                (ignoreStructureBlocks && Blocks.byStateId(blockInfo.stateId)?.block?.path == "structure_block")
            ) {
                continue
            }
            val worldPos = StructureTransform.transform(blockInfo.pos, mirror, rotation, pivot) + position
            if (!clipTo.isInside(worldPos)) continue
            val localX = worldPos.x and 15
            val localZ = worldPos.z and 15
            if (worldPos.y !in WorldHeight.MIN_Y..WorldHeight.MAX_Y) continue
            // parity: JigsawReplacementProcessor, applied to every jigsaw block unconditionally.
            val stateId =
                if (blockInfo.frontFacing != null) {
                    val finalStateName =
                        blockInfo.blockEntityNbt?.getString("final_state", "minecraft:air") ?: "minecraft:air"
                    if (Identifier.parse(finalStateName.substringBefore('[')).path == "structure_void") continue
                    parsedAndTransformedStateId(finalStateName, mirror, rotation)
                } else if (processors.rules.isNotEmpty()) {
                    val worldStateId = chunk.getBlockStateId(localX, worldPos.y, localZ)
                    val processedStateId = processors.processAt(blockInfo.stateId, worldStateId, worldPos)
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
            chunk.setBlockStateId(localX, worldPos.y, localZ, stateId)
            placeBlockEntity(chunk, blockInfo, stateId, worldPos, random)
        }
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
        return Blocks.stateIdOf(block, transformed) ?: stateId
    }

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
