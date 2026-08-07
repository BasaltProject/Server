package org.basaltmc.world.region

import org.basaltmc.nbt.NbtCompound
import org.basaltmc.nbt.NbtList
import org.basaltmc.nbt.NbtString
import org.basaltmc.nbt.NbtTag
import org.basaltmc.nbt.NbtType
import org.basaltmc.protocol.play.BitStorage
import org.basaltmc.registry.Blocks
import org.basaltmc.registry.Identifier
import org.basaltmc.world.Chunk
import org.basaltmc.world.ChunkSection
import org.basaltmc.world.WorldHeight

/** Thrown for chunk NBT that doesn't parse: an unknown block, a missing palette, a corrupt section. */
public class ChunkNbtException(
    message: String,
) : Exception(message)

/**
 * Maps a [Chunk] to and from vanilla's real on-disk chunk NBT (`net.minecraft.world.level.chunk
 * .storage.SerializableChunkData`, decompiled, not guessed): `DataVersion`/`xPos`/`yPos`/`zPos`/
 * `Status`/`sections`, each section a `Y` byte plus `block_states`/`biomes`, both the same
 * palette-plus-optional-`data`-longs shape `net.minecraft.world.level.chunk.PalettedContainer`
 * uses on the wire (`basalt-protocol`'s own `PalettedContainer`), just NBT-framed instead of
 * varint-framed: a `"palette"` list (one entry per distinct value, `{"Name": "minecraft:stone"}`
 * for blocks, a bare string for biomes) and an optional `"data"` `LongArray` holding
 * [BitStorage]-packed indices into that palette (single-entry palettes have no `data` tag at all,
 * matching vanilla's zero-bit `SingleValuePalette`). Bits per entry is never stored explicitly:
 * both sides recompute it from the palette's own size
 * (`net.minecraft.world.level.chunk.Strategy.getConfigurationForPaletteSize`), decompiled for the
 * exact thresholds: blocks go single-value / 4-8 bit indirect (floored to 4) / direct beyond that,
 * biomes go single-value / 1-3 bit indirect / direct beyond that.
 *
 * Deliberately out of scope for now, since nothing in `basalt-world` produces or needs them yet:
 * block/light data (`BlockLight`/`SkyLight`), entities, block entities, ticks, heightmaps,
 * structures, and per-block properties (every block here is its bare default state). Biomes are
 * always a single-entry `minecraft:plains` palette, matching [Chunk]'s own single-biome model.
 */
public object ChunkNbt {
    // world_version for Minecraft 26.2, per the project's own protocol constants.
    private const val DATA_VERSION = 4903
    private const val BLOCK_MIN_INDIRECT_BITS = 4
    private const val BLOCK_MAX_INDIRECT_BITS = 8
    private const val SECTION_BLOCK_COUNT = 16 * 16 * 16
    private val blockDirectBits = bitsFor(Blocks.REGISTRY.size)

    public fun write(chunk: Chunk): NbtCompound {
        val tag = NbtCompound()
        tag.putInt("DataVersion", DATA_VERSION)
        tag.putInt("xPos", chunk.chunkX)
        tag.putInt("yPos", WorldHeight.MIN_SECTION)
        tag.putInt("zPos", chunk.chunkZ)
        tag.putLong("LastUpdate", 0L)
        tag.putLong("InhabitedTime", 0L)
        tag.putString("Status", "minecraft:full")

        val sections = mutableListOf<NbtTag>()
        for (i in 0 until chunk.sectionCount) {
            val sectionTag = NbtCompound()
            sectionTag.putByte("Y", (WorldHeight.MIN_SECTION + i).toByte())
            sectionTag["block_states"] = encodeBlockStates(chunk.section(i))
            sectionTag["biomes"] = encodeBiomes()
            sections += sectionTag
        }
        tag["sections"] = NbtList(NbtType.COMPOUND, sections)
        return tag
    }

    public fun read(tag: NbtCompound): Chunk {
        val chunk = Chunk(tag.getInt("xPos"), tag.getInt("zPos"))
        val sections = tag.getList("sections") ?: throw ChunkNbtException("missing sections list")
        for (sectionEntry in sections) {
            val sectionTag = sectionEntry as? NbtCompound ?: throw ChunkNbtException("section entry is not a compound")
            val y = sectionTag.getByte("Y").toInt()
            val index = y - WorldHeight.MIN_SECTION
            if (index !in 0 until chunk.sectionCount) continue
            val blockStates = sectionTag.getCompound("block_states")
            chunk.section(index).setBlockStateIds(decodeBlockStates(blockStates))
        }
        return chunk
    }

    private fun encodeBlockStates(section: ChunkSection): NbtCompound =
        encodePalette(
            values = section.blockStateIds(),
            minIndirectBits = BLOCK_MIN_INDIRECT_BITS,
            maxIndirectBits = BLOCK_MAX_INDIRECT_BITS,
            directBits = blockDirectBits,
        ) { stateId ->
            val block =
                Blocks.REGISTRY.byId(stateId) ?: throw ChunkNbtException("no block registered for state id $stateId")
            val entry = NbtCompound()
            entry.putString("Name", block.block.toString())
            entry
        }

    private fun encodeBiomes(): NbtCompound {
        val tag = NbtCompound()
        tag["palette"] = NbtList(NbtType.STRING, listOf(NbtString(PLAINS_BIOME)))
        return tag
    }

    private fun decodeBlockStates(tag: NbtCompound): IntArray {
        val paletteList = tag.getList("palette") ?: throw ChunkNbtException("missing block_states palette")
        val paletteIds =
            paletteList.map { entry ->
                val name =
                    (entry as? NbtCompound)?.getString("Name")
                        ?: throw ChunkNbtException("invalid block palette entry: $entry")
                Blocks.REGISTRY.idOf(Identifier.parse(name)) ?: throw ChunkNbtException("unknown block $name")
            }
        return decodePalette(
            paletteIds = paletteIds,
            data = tag.getLongArray("data"),
            entryCount = SECTION_BLOCK_COUNT,
            minIndirectBits = BLOCK_MIN_INDIRECT_BITS,
            maxIndirectBits = BLOCK_MAX_INDIRECT_BITS,
            directBits = blockDirectBits,
        )
    }

    private inline fun encodePalette(
        values: IntArray,
        minIndirectBits: Int,
        maxIndirectBits: Int,
        directBits: Int,
        elementToNbt: (Int) -> NbtTag,
    ): NbtCompound {
        val palette = LinkedHashSet<Int>()
        for (value in values) palette.add(value)

        val tag = NbtCompound()
        tag["palette"] = NbtList(NbtType.COMPOUND, palette.map(elementToNbt))
        if (palette.size == 1) return tag

        val neededBits = bitsFor(palette.size)
        val indirect = neededBits <= maxIndirectBits
        val bitsPerEntry = if (indirect) neededBits.coerceAtLeast(minIndirectBits) else directBits
        val storage = BitStorage(bitsPerEntry, values.size)
        if (indirect) {
            val indexOf = palette.withIndex().associate { (index, value) -> value to index }
            for (i in values.indices) storage[i] = indexOf.getValue(values[i])
        } else {
            for (i in values.indices) storage[i] = values[i]
        }
        tag.putLongArray("data", storage.data)
        return tag
    }

    private fun decodePalette(
        paletteIds: List<Int>,
        data: LongArray,
        entryCount: Int,
        minIndirectBits: Int,
        maxIndirectBits: Int,
        directBits: Int,
    ): IntArray {
        if (paletteIds.size == 1) return IntArray(entryCount) { paletteIds[0] }

        val neededBits = bitsFor(paletteIds.size)
        val indirect = neededBits <= maxIndirectBits
        val bitsPerEntry = if (indirect) neededBits.coerceAtLeast(minIndirectBits) else directBits
        val storage = BitStorage(bitsPerEntry, entryCount, data)
        return IntArray(entryCount) { i -> if (indirect) paletteIds[storage[i]] else storage[i] }
    }

    private fun bitsFor(distinctCount: Int): Int {
        var bits = 0
        while ((1 shl bits) < distinctCount) bits++
        return bits
    }

    private const val PLAINS_BIOME = "minecraft:plains"
}
