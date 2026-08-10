package org.basaltmc.world.region

import org.basaltmc.nbt.NbtCompound
import org.basaltmc.nbt.NbtList
import org.basaltmc.nbt.NbtString
import org.basaltmc.nbt.NbtTag
import org.basaltmc.nbt.NbtType
import org.basaltmc.protocol.play.BitStorage
import org.basaltmc.registry.Biomes
import org.basaltmc.registry.Blocks
import org.basaltmc.registry.Identifier
import org.basaltmc.world.Chunk
import org.basaltmc.world.ChunkBlockEntity
import org.basaltmc.world.ChunkHeight
import org.basaltmc.world.ChunkScheduledTick
import org.basaltmc.world.ChunkSection

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
 * Non-default states emitted by worldgen preserve their vanilla `Properties` compound in the
 * palette and resolve it back to the same network state ID when loaded. Block and fluid ticks use
 * the vanilla SavedTick shape and preserve insertion order; executing due ticks remains a runtime
 * concern outside this codec. Deliberately out of scope for now: block/light data
 * (`BlockLight`/`SkyLight`), entities, heightmaps, and structures. Block entities preserve full
 * custom NBT plus vanilla id/x/y/z metadata. Section biome palettes preserve the actual
 * quart-resolution IDs generated for every dimension.
 */
public object ChunkNbt {
    // world_version for Minecraft 26.2, per the project's own protocol constants.
    private const val DATA_VERSION = 4903
    private const val BLOCK_MIN_INDIRECT_BITS = 4
    private const val BLOCK_MAX_INDIRECT_BITS = 8
    private const val BIOME_MIN_INDIRECT_BITS = 1
    private const val BIOME_MAX_INDIRECT_BITS = 3
    private const val SECTION_BLOCK_COUNT = 16 * 16 * 16
    private const val SECTION_BIOME_COUNT = 4 * 4 * 4
    private val blockDirectBits = bitsFor(Blocks.stateCount)
    private val biomeDirectBits = bitsFor(Biomes.REGISTRY.size)
    private val BLOCK_ENTITY_METADATA_KEYS = setOf("id", "x", "y", "z", "keepPacked")

    public fun write(chunk: Chunk): NbtCompound {
        val tag = NbtCompound()
        tag.putInt("DataVersion", DATA_VERSION)
        tag.putInt("xPos", chunk.chunkX)
        tag.putInt("yPos", chunk.verticalBounds.minSection)
        tag.putInt("zPos", chunk.chunkZ)
        tag.putLong("LastUpdate", 0L)
        tag.putLong("InhabitedTime", 0L)
        tag.putString("Status", "minecraft:full")

        val sections = mutableListOf<NbtTag>()
        for (i in 0 until chunk.sectionCount) {
            val sectionTag = NbtCompound()
            sectionTag.putByte("Y", (chunk.verticalBounds.minSection + i).toByte())
            sectionTag["block_states"] = encodeBlockStates(chunk.section(i))
            sectionTag["biomes"] = encodeBiomes(chunk.section(i))
            sections += sectionTag
        }
        tag["sections"] = NbtList(NbtType.COMPOUND, sections)
        val blockEntities = chunk.blockEntities().map { encodeBlockEntity(chunk, it) }
        tag["block_entities"] = NbtList(if (blockEntities.isEmpty()) NbtType.END else NbtType.COMPOUND, blockEntities)
        tag["block_ticks"] = encodeTicks(chunk, chunk.scheduledBlockTicks())
        tag["fluid_ticks"] = encodeTicks(chunk, chunk.scheduledFluidTicks())
        return tag
    }

    public fun read(
        tag: NbtCompound,
        verticalBounds: ChunkHeight = ChunkHeight.OVERWORLD,
    ): Chunk {
        val chunk = Chunk(tag.getInt("xPos"), tag.getInt("zPos"), verticalBounds)
        val sections = tag.getList("sections") ?: throw ChunkNbtException("missing sections list")
        for (sectionEntry in sections) {
            val sectionTag = sectionEntry as? NbtCompound ?: throw ChunkNbtException("section entry is not a compound")
            val y = sectionTag.getByte("Y").toInt()
            val index = y - verticalBounds.minSection
            if (index !in 0 until chunk.sectionCount) continue
            val blockStates = sectionTag.getCompound("block_states")
            chunk.section(index).setBlockStateIds(decodeBlockStates(blockStates))
            val biomes = sectionTag.getCompound("biomes")
            chunk.section(index).setBiomeIds(decodeBiomes(biomes))
        }
        val blockEntities = tag.getList("block_entities")
        if (blockEntities != null) {
            for (entry in blockEntities) chunk.setBlockEntity(decodeBlockEntity(chunk, entry))
        }
        decodeTicks(chunk, tag.getList("block_ticks")).forEach(chunk::scheduleBlockTick)
        decodeTicks(chunk, tag.getList("fluid_ticks")).forEach(chunk::scheduleFluidTick)
        return chunk
    }

    private fun encodeTicks(
        chunk: Chunk,
        ticks: List<ChunkScheduledTick>,
    ): NbtList =
        NbtList(
            if (ticks.isEmpty()) NbtType.END else NbtType.COMPOUND,
            ticks.map { tick ->
                NbtCompound().apply {
                    putString("i", tick.type.toString())
                    putInt("x", chunk.chunkX * 16 + tick.localX)
                    putInt("y", tick.y)
                    putInt("z", chunk.chunkZ * 16 + tick.localZ)
                    putInt("t", tick.delay)
                    putInt("p", tick.priority)
                }
            },
        )

    private fun decodeTicks(
        chunk: Chunk,
        entries: NbtList?,
    ): List<ChunkScheduledTick> =
        entries?.map { entry ->
            val tag = entry as? NbtCompound ?: throw ChunkNbtException("scheduled tick entry is not a compound")
            val type =
                tag.getString("i").takeIf(String::isNotEmpty) ?: throw ChunkNbtException("scheduled tick has no id")
            val worldX = tag.getInt("x")
            val y = tag.getInt("y")
            val worldZ = tag.getInt("z")
            val localX = worldX - chunk.chunkX * 16
            val localZ = worldZ - chunk.chunkZ * 16
            if (localX !in 0..15 || localZ !in 0..15 || y !in chunk.verticalBounds.minY..chunk.verticalBounds.maxY) {
                throw ChunkNbtException("scheduled tick $type at ($worldX,$y,$worldZ) is outside the chunk")
            }
            ChunkScheduledTick(
                Identifier.parse(type),
                localX,
                y,
                localZ,
                tag.getInt("t"),
                tag.getInt("p").coerceIn(-3, 3),
            )
        } ?: emptyList()

    private fun encodeBlockEntity(
        chunk: Chunk,
        blockEntity: ChunkBlockEntity,
    ): NbtCompound {
        val tag = NbtCompound()
        for ((name, value) in blockEntity.data) tag[name] = value
        tag.putString("id", blockEntity.type.toString())
        tag.putInt("x", chunk.chunkX * 16 + blockEntity.localX)
        tag.putInt("y", blockEntity.y)
        tag.putInt("z", chunk.chunkZ * 16 + blockEntity.localZ)
        return tag
    }

    private fun decodeBlockEntity(
        chunk: Chunk,
        entry: NbtTag,
    ): ChunkBlockEntity {
        val tag = entry as? NbtCompound ?: throw ChunkNbtException("block entity entry is not a compound")
        val id = tag.getString("id").takeIf(String::isNotEmpty) ?: throw ChunkNbtException("block entity has no id")
        val worldX = tag.getInt("x")
        val y = tag.getInt("y")
        val worldZ = tag.getInt("z")
        val localX = worldX - chunk.chunkX * 16
        val localZ = worldZ - chunk.chunkZ * 16
        if (localX !in 0..15 || localZ !in 0..15 || y !in chunk.verticalBounds.minY..chunk.verticalBounds.maxY) {
            throw ChunkNbtException(
                "block entity $id at ($worldX,$y,$worldZ) is outside chunk (${chunk.chunkX},${chunk.chunkZ})",
            )
        }
        val data = NbtCompound()
        for ((name, value) in tag) {
            if (name !in BLOCK_ENTITY_METADATA_KEYS) data[name] = value
        }
        return ChunkBlockEntity(localX, y, localZ, Identifier.parse(id), data)
    }

    private fun encodeBlockStates(section: ChunkSection): NbtCompound =
        encodePalette(
            values = section.blockStateIds(),
            minIndirectBits = BLOCK_MIN_INDIRECT_BITS,
            maxIndirectBits = BLOCK_MAX_INDIRECT_BITS,
            directBits = blockDirectBits,
            elementType = NbtType.COMPOUND,
        ) { stateId ->
            val block =
                Blocks.byStateId(stateId) ?: throw ChunkNbtException("no block registered for state id $stateId")
            val entry = NbtCompound()
            entry.putString("Name", block.block.toString())
            if (block.properties.isNotEmpty()) {
                val properties = NbtCompound()
                for ((name, value) in block.properties) properties.putString(name, value)
                entry["Properties"] = properties
            }
            entry
        }

    private fun encodeBiomes(section: ChunkSection): NbtCompound =
        encodePalette(
            values = section.biomeIds(),
            minIndirectBits = BIOME_MIN_INDIRECT_BITS,
            maxIndirectBits = BIOME_MAX_INDIRECT_BITS,
            directBits = biomeDirectBits,
            elementType = NbtType.STRING,
        ) { biomeId ->
            val identifier =
                Biomes.REGISTRY.identifierOf(biomeId)
                    ?: throw ChunkNbtException("no biome registered for id $biomeId")
            NbtString(identifier.toString())
        }

    private fun decodeBlockStates(tag: NbtCompound): IntArray {
        val paletteList = tag.getList("palette") ?: throw ChunkNbtException("missing block_states palette")
        val paletteIds =
            paletteList.map { entry ->
                val entryTag = entry as? NbtCompound ?: throw ChunkNbtException("invalid block palette entry: $entry")
                val name =
                    entryTag.getString("Name").takeIf { it.isNotEmpty() }
                        ?: throw ChunkNbtException("invalid block palette entry: $entry")
                val propertiesTag = entryTag.getCompound("Properties")
                val properties =
                    propertiesTag.associate { (key, value) ->
                        key to
                            (
                                (value as? NbtString)?.value
                                    ?: throw ChunkNbtException("invalid property $key for block $name")
                            )
                    }
                Blocks.stateIdOf(Identifier.parse(name), properties)
                    ?: throw ChunkNbtException("unknown block state $name$properties")
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

    private fun decodeBiomes(tag: NbtCompound): IntArray {
        val paletteList = tag.getList("palette") ?: throw ChunkNbtException("missing biomes palette")
        val paletteIds =
            paletteList.map { entry ->
                val identifier =
                    (entry as? NbtString)?.value?.let(Identifier::parse)
                        ?: throw ChunkNbtException("invalid biome palette entry: $entry")
                Biomes.REGISTRY.idOf(identifier)
                    ?: throw ChunkNbtException("unknown biome $identifier")
            }
        return decodePalette(
            paletteIds = paletteIds,
            data = tag.getLongArray("data"),
            entryCount = SECTION_BIOME_COUNT,
            minIndirectBits = BIOME_MIN_INDIRECT_BITS,
            maxIndirectBits = BIOME_MAX_INDIRECT_BITS,
            directBits = biomeDirectBits,
        )
    }

    private inline fun encodePalette(
        values: IntArray,
        minIndirectBits: Int,
        maxIndirectBits: Int,
        directBits: Int,
        elementType: NbtType,
        elementToNbt: (Int) -> NbtTag,
    ): NbtCompound {
        val palette = LinkedHashSet<Int>()
        for (value in values) palette.add(value)

        val tag = NbtCompound()
        tag["palette"] = NbtList(elementType, palette.map(elementToNbt))
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
}
