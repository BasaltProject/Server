package org.basaltmc.world.structure

import org.basaltmc.nbt.NbtCompound
import org.basaltmc.nbt.NbtInt
import org.basaltmc.nbt.NbtList
import org.basaltmc.nbt.NbtType
import org.basaltmc.registry.Blocks
import org.basaltmc.registry.Identifier
import org.basaltmc.world.Chunk
import kotlin.test.Test
import kotlin.test.assertEquals

/** Synthetic NBT built by hand, matching the real `size`/`palette`/`blocks` structure format, not extracted from any Mojang file. */
private fun syntheticTemplateNbt(): NbtCompound {
    val root = NbtCompound()
    root["size"] = NbtList(NbtType.INT, listOf(NbtInt(2), NbtInt(1), NbtInt(1)))

    val paletteEntry =
        NbtCompound().apply {
            putString("Name", "minecraft:stone")
        }
    root["palette"] = NbtList(NbtType.COMPOUND, listOf(paletteEntry))

    val block0 =
        NbtCompound().apply {
            set("pos", NbtList(NbtType.INT, listOf(NbtInt(0), NbtInt(0), NbtInt(0))))
            putInt("state", 0)
        }
    val block1 =
        NbtCompound().apply {
            set("pos", NbtList(NbtType.INT, listOf(NbtInt(1), NbtInt(0), NbtInt(0))))
            putInt("state", 0)
        }
    root["blocks"] = NbtList(NbtType.COMPOUND, listOf(block0, block1))
    return root
}

class StructureTemplateTest {
    @Test
    fun loadsSizePaletteAndBlocks() {
        val template = StructureTemplate()
        template.load(syntheticTemplateNbt())

        assertEquals(StructurePos(2, 1, 1), template.size)
        assertEquals(2, template.blocks.size)
        val stoneId = Blocks.REGISTRY.byIdentifier(Identifier.vanilla("stone"))!!.stateId
        for (blockInfo in template.blocks) assertEquals(stoneId, blockInfo.stateId)
    }

    @Test
    fun placesBlocksAtTheUnrotatedWorldPosition() {
        val template = StructureTemplate()
        template.load(syntheticTemplateNbt())
        val chunk = Chunk(0, 0)

        val boundingBox =
            template.getBoundingBox(
                StructurePos(4, 64, 4),
                StructureRotation.NONE,
                StructurePos(0, 0, 0),
                StructureMirror.NONE,
            )
        template.placeInto(
            chunk,
            StructurePos(4, 64, 4),
            StructureRotation.NONE,
            StructureMirror.NONE,
            StructurePos(0, 0, 0),
            boundingBox,
        )

        val stoneId = Blocks.REGISTRY.byIdentifier(Identifier.vanilla("stone"))!!.stateId
        assertEquals(stoneId, chunk.getBlockStateId(4, 64, 4))
        assertEquals(stoneId, chunk.getBlockStateId(5, 64, 4))
        assertEquals(Blocks.AIR.stateId, chunk.getBlockStateId(6, 64, 4))
    }

    @Test
    fun rotating90DegreesSwapsTheAxisTheBlocksExtendAlong() {
        val template = StructureTemplate()
        template.load(syntheticTemplateNbt())
        val chunk = Chunk(0, 0)

        val boundingBox =
            template.getBoundingBox(
                StructurePos(4, 64, 4),
                StructureRotation.CLOCKWISE_90,
                StructurePos(0, 0, 0),
                StructureMirror.NONE,
            )
        template.placeInto(
            chunk,
            StructurePos(4, 64, 4),
            StructureRotation.CLOCKWISE_90,
            StructureMirror.NONE,
            StructurePos(0, 0, 0),
            boundingBox,
        )

        val stoneId = Blocks.REGISTRY.byIdentifier(Identifier.vanilla("stone"))!!.stateId
        assertEquals(stoneId, chunk.getBlockStateId(4, 64, 4))
        assertEquals(stoneId, chunk.getBlockStateId(4, 64, 5))
        assertEquals(Blocks.AIR.stateId, chunk.getBlockStateId(5, 64, 4))
    }

    @Test
    fun clippingToABoundingBoxDropsBlocksOutsideIt() {
        val template = StructureTemplate()
        template.load(syntheticTemplateNbt())
        val chunk = Chunk(0, 0)

        val narrowClip = StructureBoundingBox(4, 64, 4, 4, 64, 4)
        template.placeInto(
            chunk,
            StructurePos(4, 64, 4),
            StructureRotation.NONE,
            StructureMirror.NONE,
            StructurePos(0, 0, 0),
            narrowClip,
        )

        val stoneId = Blocks.REGISTRY.byIdentifier(Identifier.vanilla("stone"))!!.stateId
        assertEquals(stoneId, chunk.getBlockStateId(4, 64, 4))
        assertEquals(Blocks.AIR.stateId, chunk.getBlockStateId(5, 64, 4))
    }
}
