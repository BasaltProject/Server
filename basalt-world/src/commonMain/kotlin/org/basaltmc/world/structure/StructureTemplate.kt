package org.basaltmc.world.structure

import org.basaltmc.nbt.NbtCompound
import org.basaltmc.nbt.NbtInt
import org.basaltmc.random.WorldgenRandom
import org.basaltmc.registry.Blocks
import org.basaltmc.registry.Identifier
import org.basaltmc.world.Chunk
import org.basaltmc.world.WorldHeight

/**
 * // parity: net.minecraft.world.level.levelgen.structure.templatesystem.StructureTemplate,
 * scoped to what this project can currently represent. Real vanilla block states carry
 * properties (facing, waterlogged, axis...) that this project's [Blocks] registry doesn't model
 * yet (every block here is its default state only, see `Blocks`'s own KDoc): the NBT `Properties`
 * compound on each palette entry is read but discarded, and [StructureTemplate.load] resolves
 * only the block's `Name`. This means a rotated/mirrored piece places the right blocks in the
 * right positions but with the wrong facing on directional blocks (doors, stairs, logs...): a
 * real, visible gap, but a strict superset of doing nothing, and consistent with this project's
 * existing block-state scope. Multiple alternative palettes (real vanilla's `palettes` list, used
 * by a handful of non-village structures for random variation) and block entity contents (loot
 * tables, sign text) are not modeled either: this project has no inventory/loot system yet.
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
        val paletteStateIds =
            IntArray(paletteList.size) { i ->
                val entry = paletteList[i] as NbtCompound
                val name = entry.getString("Name")
                Blocks.REGISTRY.byIdentifier(Identifier.parse(name))?.stateId ?: Blocks.AIR.stateId
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
            blocksBacking.add(StructureBlockInfo(pos, stateId, blockEntityNbt, frontFacing))
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
            PlacedJigsaw(
                pos = worldPos,
                front = front.mirror(mirror).rotate(rotation),
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
     * // parity: StructureTemplate.placeInWorld, narrowed to block placement only (no fluid
     * spread, no block-entity content, no entities: documented gaps above). [clipTo] mirrors real
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
    ) {
        for (blockInfo in blocksBacking) {
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
                    Blocks.REGISTRY.byIdentifier(Identifier.parse(finalStateName))?.stateId ?: Blocks.AIR.stateId
                } else if (random != null && processors.rules.isNotEmpty()) {
                    val worldStateId = chunk.getBlockStateId(localX, worldPos.y, localZ)
                    processors.process(blockInfo.stateId, worldStateId, random)
                } else {
                    blockInfo.stateId
                }
            chunk.setBlockStateId(localX, worldPos.y, localZ, stateId)
        }
    }
}

public data class StructureBlockInfo(
    val pos: StructurePos,
    val stateId: Int,
    val blockEntityNbt: NbtCompound?,
    val frontFacing: StructureDirection?,
)

/** // parity: StructureTemplate.JigsawBlockInfo, world-space. [finalStateName] is what real vanilla's `JigsawReplacementProcessor` swaps the jigsaw block itself for once placement is done. */
public data class PlacedJigsaw(
    val pos: StructurePos,
    val front: StructureDirection,
    val name: String,
    val pool: String,
    val target: String,
    val finalStateName: String,
    val placementPriority: Int,
    val selectionPriority: Int,
)
