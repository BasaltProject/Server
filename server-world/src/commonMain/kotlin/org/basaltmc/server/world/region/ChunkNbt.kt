package org.basaltmc.server.world.region

import org.basaltmc.server.registry.Biomes
import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.world.Chunk
import org.basaltmc.server.world.ChunkBlockEntity
import org.basaltmc.server.world.ChunkGeneratedEntity
import org.basaltmc.server.world.ChunkHeight
import org.basaltmc.server.world.ChunkScheduledTick
import org.basaltmc.server.world.ChunkSection
import org.basaltmc.wire.common.Identifier
import org.basaltmc.wire.nbt.NbtCompound
import org.basaltmc.wire.nbt.NbtDouble
import org.basaltmc.wire.nbt.NbtFloat
import org.basaltmc.wire.nbt.NbtList
import org.basaltmc.wire.nbt.NbtString
import org.basaltmc.wire.nbt.NbtTag
import org.basaltmc.wire.nbt.NbtType
import org.basaltmc.wire.nbt.deepCopy
import org.basaltmc.wire.protocol.play.BitStorage

/** Thrown for chunk NBT that doesn't parse: an unknown block, a missing palette, a corrupt section. */
public class ChunkNbtException(
    message: String,
) : Exception(message)

/**
 * // parity: net.minecraft.world.level.chunk.storage.SerializableChunkData, the on-disk chunk NBT.
 * Sections carry the wire format's palette-plus-optional-`data`-longs shape, NBT-framed, with no
 * explicit bits-per-entry: both sides recompute it from the palette size
 * (`Strategy.getConfigurationForPaletteSize`), blocks 4-8 bit indirect, biomes 1-3.
 *
 * Block states keep their `Properties` compound and resolve back to the same network state ID,
 * block entities keep their custom NBT, and ticks keep the SavedTick shape and insertion order.
 * Worldgen entities use a Basalt-owned list until vanilla's separate entity-region storage exists.
 * Light data, heightmaps and structures stay out of scope.
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

        val scratch = PaletteScratch()
        val sections = mutableListOf<NbtTag>()
        for (i in 0 until chunk.sectionCount) {
            val sectionTag = NbtCompound()
            sectionTag.putByte("Y", (chunk.verticalBounds.minSection + i).toByte())
            sectionTag["block_states"] = encodeBlockStates(chunk.section(i), scratch)
            sectionTag["biomes"] = encodeBiomes(chunk.section(i), scratch)
            sections += sectionTag
        }
        tag["sections"] = NbtList(NbtType.COMPOUND, sections)
        val blockEntities = chunk.blockEntities().map { encodeBlockEntity(chunk, it) }
        tag["block_entities"] = NbtList(if (blockEntities.isEmpty()) NbtType.END else NbtType.COMPOUND, blockEntities)
        tag["block_ticks"] = encodeTicks(chunk, chunk.scheduledBlockTicks())
        tag["fluid_ticks"] = encodeTicks(chunk, chunk.scheduledFluidTicks())
        tag[GENERATED_ENTITIES_KEY] = encodeGeneratedEntities(chunk)
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
        // The sections were filled behind the chunk's back, so its heightmaps mean nothing yet.
        chunk.invalidateHeightmaps()
        val blockEntities = tag.getList("block_entities")
        if (blockEntities != null) {
            for (entry in blockEntities) chunk.setBlockEntity(decodeBlockEntity(chunk, entry))
        }
        decodeTicks(chunk, tag.getList("block_ticks")).forEach(chunk::scheduleBlockTick)
        decodeTicks(chunk, tag.getList("fluid_ticks")).forEach(chunk::scheduleFluidTick)
        decodeGeneratedEntities(tag.getList(GENERATED_ENTITIES_KEY)).forEach(chunk::addGeneratedEntity)
        return chunk
    }

    private fun encodeGeneratedEntities(chunk: Chunk): NbtList =
        NbtList(
            if (chunk.generatedEntities().isEmpty()) NbtType.END else NbtType.COMPOUND,
            chunk.generatedEntities().map { entity ->
                NbtCompound().apply {
                    putString("id", entity.type.toString())
                    this["Pos"] =
                        NbtList(
                            NbtType.DOUBLE,
                            listOf(NbtDouble(entity.x), NbtDouble(entity.y), NbtDouble(entity.z)),
                        )
                    this["Rotation"] = NbtList(NbtType.FLOAT, listOf(NbtFloat(entity.yaw), NbtFloat(entity.pitch)))
                    this["BasaltData"] = entity.data.deepCopy()
                }
            },
        )

    private fun decodeGeneratedEntities(entries: NbtList?): List<ChunkGeneratedEntity> =
        entries?.map { entry ->
            val tag = entry as? NbtCompound ?: throw ChunkNbtException("generated entity entry is not a compound")
            val type =
                tag.getString("id").takeIf(String::isNotEmpty) ?: throw ChunkNbtException("generated entity has no id")
            val pos = tag.getList("Pos") ?: throw ChunkNbtException("generated entity $type has no Pos")
            val rotation = tag.getList("Rotation") ?: throw ChunkNbtException("generated entity $type has no Rotation")
            if (pos.size != 3 ||
                rotation.size != 2
            ) {
                throw ChunkNbtException("generated entity $type has invalid vectors")
            }
            ChunkGeneratedEntity(
                type = Identifier.parse(type),
                x = (pos[0] as? NbtDouble)?.value ?: throw ChunkNbtException("generated entity $type has invalid Pos"),
                y = (pos[1] as? NbtDouble)?.value ?: throw ChunkNbtException("generated entity $type has invalid Pos"),
                z = (pos[2] as? NbtDouble)?.value ?: throw ChunkNbtException("generated entity $type has invalid Pos"),
                yaw =
                    (rotation[0] as? NbtFloat)?.value
                        ?: throw ChunkNbtException("generated entity $type has invalid Rotation"),
                pitch =
                    (rotation[1] as? NbtFloat)?.value
                        ?: throw ChunkNbtException("generated entity $type has invalid Rotation"),
                data = (tag["BasaltData"] as? NbtCompound)?.deepCopy() ?: NbtCompound(),
            )
        } ?: emptyList()

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

    private fun encodeBlockStates(
        section: ChunkSection,
        scratch: PaletteScratch,
    ): NbtCompound =
        encodePalette(
            values = scratch.blockValues.also(section::copyBlockStateIdsInto),
            minIndirectBits = BLOCK_MIN_INDIRECT_BITS,
            maxIndirectBits = BLOCK_MAX_INDIRECT_BITS,
            directBits = blockDirectBits,
            elementType = NbtType.COMPOUND,
            slotOfValue = scratch.blockSlots,
            paletteValues = scratch.paletteValues,
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

    private fun encodeBiomes(
        section: ChunkSection,
        scratch: PaletteScratch,
    ): NbtCompound =
        encodePalette(
            values = scratch.biomeValues.also(section::copyBiomeIdsInto),
            minIndirectBits = BIOME_MIN_INDIRECT_BITS,
            maxIndirectBits = BIOME_MAX_INDIRECT_BITS,
            directBits = biomeDirectBits,
            elementType = NbtType.STRING,
            slotOfValue = scratch.biomeSlots,
            paletteValues = scratch.paletteValues,
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

    /**
     * The palette is derived in first-appearance order, exactly as before, but through primitive
     * arrays: a `LinkedHashSet<Int>` plus a `Map<Int, Int>` boxed every one of the 4096 values twice
     * per section, which was 200000 boxed lookups a chunk and all but two milliseconds of a save.
     *
     * [slotOfValue] holds `slot + 1` so a zeroed array means "absent" and only the entries actually
     * used are cleared afterwards, leaving it reusable without refilling it.
     */
    private inline fun encodePalette(
        values: IntArray,
        minIndirectBits: Int,
        maxIndirectBits: Int,
        directBits: Int,
        elementType: NbtType,
        slotOfValue: IntArray,
        paletteValues: IntArray,
        elementToNbt: (Int) -> NbtTag,
    ): NbtCompound {
        var paletteSize = 0
        for (value in values) {
            if (slotOfValue[value] != 0) continue
            paletteValues[paletteSize] = value
            paletteSize++
            slotOfValue[value] = paletteSize
        }

        val entries = ArrayList<NbtTag>(paletteSize)
        for (i in 0 until paletteSize) entries.add(elementToNbt(paletteValues[i]))

        val tag = NbtCompound()
        tag["palette"] = NbtList(elementType, entries)
        if (paletteSize > 1) {
            val neededBits = bitsFor(paletteSize)
            val indirect = neededBits <= maxIndirectBits
            val bitsPerEntry = if (indirect) neededBits.coerceAtLeast(minIndirectBits) else directBits
            val storage = BitStorage(bitsPerEntry, values.size)
            if (indirect) {
                for (i in values.indices) storage[i] = slotOfValue[values[i]] - 1
            } else {
                for (i in values.indices) storage[i] = values[i]
            }
            tag.putLongArray("data", storage.data)
        }

        for (i in 0 until paletteSize) slotOfValue[paletteValues[i]] = 0
        return tag
    }

    /**
     * Reused across a chunk's sections, and owned by one [write] call: this object is shared, and
     * saves run on their own thread while generation workers run on theirs.
     */
    private class PaletteScratch {
        val blockSlots = IntArray(Blocks.stateCount)
        val biomeSlots = IntArray(Biomes.REGISTRY.size)
        val paletteValues = IntArray(SECTION_BLOCK_COUNT)
        val blockValues = IntArray(SECTION_BLOCK_COUNT)
        val biomeValues = IntArray(SECTION_BIOME_COUNT)
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

    private const val GENERATED_ENTITIES_KEY = "basalt_generated_entities"
}
