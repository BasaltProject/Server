// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util.filefix.fixes;

import com.mojang.datafixers.schemas.Schema;
import com.mojang.serialization.Dynamic;
import it.unimi.dsi.fastutil.longs.Long2ObjectMap;
import it.unimi.dsi.fastutil.longs.Long2ObjectOpenHashMap;
import it.unimi.dsi.fastutil.longs.LongArrayList;
import it.unimi.dsi.fastutil.longs.LongList;
import java.io.IOException;
import java.util.ArrayList;
import java.util.Comparator;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.Optional;
import java.util.concurrent.CompletableFuture;
import net.minecraft.core.registries.Registries;
import net.minecraft.nbt.CompoundTag;
import net.minecraft.nbt.NbtOps;
import net.minecraft.nbt.Tag;
import net.minecraft.resources.Identifier;
import net.minecraft.resources.ResourceKey;
import net.minecraft.server.level.ChunkMap;
import net.minecraft.util.datafix.DataFixTypes;
import net.minecraft.util.datafix.fixes.References;
import net.minecraft.util.filefix.CanceledFileFixException;
import net.minecraft.util.filefix.FileFix;
import net.minecraft.util.filefix.access.ChunkNbt;
import net.minecraft.util.filefix.access.CompressedNbt;
import net.minecraft.util.filefix.access.FileAccess;
import net.minecraft.util.filefix.access.FileAccessProvider;
import net.minecraft.util.filefix.access.FileRelation;
import net.minecraft.util.filefix.access.FileResourceTypes;
import net.minecraft.util.filefix.access.LevelDat;
import net.minecraft.util.filefix.access.SavedDataNbt;
import net.minecraft.util.worldupdate.UpgradeProgress;
import net.minecraft.world.level.ChunkPos;
import net.minecraft.world.level.Level;
import net.minecraft.world.level.chunk.storage.RegionStorageInfo;

public class LegacyStructureFileFix extends FileFix {
    public static final int STRUCTURE_RANGE = 8;
    public static final List<String> OVERWORLD_LEGACY_STRUCTURES =
        List.of("Monument", "Stronghold", "Mineshaft", "Temple", "Mansion");
    public static final Map<String, String> LEGACY_TO_CURRENT_MAP = Map.of(
        "Iglu", "Igloo", "TeDP", "Desert_Pyramid", "TeJP", "Jungle_Pyramid", "TeSH", "Swamp_Hut");
    public static final List<String> NETHER_LEGACY_STRUCTURES = List.of("Fortress");
    public static final List<String> END_LEGACY_STRUCTURES = List.of("EndCity");
    private static final ResourceKey<Level> OVERWORLD_KEY =
        ResourceKey.create(Registries.DIMENSION, Identifier.withDefaultNamespace("overworld"));
    private static final ResourceKey<Level> NETHER_KEY =
        ResourceKey.create(Registries.DIMENSION, Identifier.withDefaultNamespace("the_nether"));
    private static final ResourceKey<Level> END_KEY =
        ResourceKey.create(Registries.DIMENSION, Identifier.withDefaultNamespace("the_end"));

    public LegacyStructureFileFix(Schema schema) {
        super(schema);
    }

    @Override
    public void makeFixer() {
        addFileContentFix(files -> {
            List<FileAccess<SavedDataNbt>> overworldData = structureData(files, OVERWORLD_LEGACY_STRUCTURES);
            List<FileAccess<SavedDataNbt>> netherData = structureData(files, NETHER_LEGACY_STRUCTURES);
            List<FileAccess<SavedDataNbt>> endData = structureData(files, END_LEGACY_STRUCTURES);
            FileAccess<LevelDat> levelDat = files.getFileAccess(FileResourceTypes.LEVEL_DAT,
                FileRelation.ORIGIN.forFile("level.dat"));
            FileAccess<ChunkNbt> overworldChunks = chunks(files, "overworld", OVERWORLD_KEY, FileRelation.OLD_OVERWORLD);
            FileAccess<ChunkNbt> netherChunks = chunks(files, "the_nether", NETHER_KEY, FileRelation.OLD_NETHER);
            FileAccess<ChunkNbt> endChunks = chunks(files, "the_end", END_KEY, FileRelation.OLD_END);
            return progress -> {
                Optional<Dynamic<Tag>> level = levelDat.getOnlyFile().read();
                if (level.isEmpty()) {
                    return;
                }
                progress.setType(UpgradeProgress.Type.LEGACY_STRUCTURES);
                extractAndStore(level.get(), List.of(
                    new DimensionEntry(OVERWORLD_KEY, overworldData, overworldChunks, new Long2ObjectOpenHashMap<>()),
                    new DimensionEntry(NETHER_KEY, netherData, netherChunks, new Long2ObjectOpenHashMap<>()),
                    new DimensionEntry(END_KEY, endData, endChunks, new Long2ObjectOpenHashMap<>())), progress);
            };
        });
    }

    private static FileAccess<ChunkNbt> chunks(FileAccessProvider files, String name, ResourceKey<Level> key,
            FileRelation old) {
        RegionStorageInfo info = new RegionStorageInfo(name, key, "chunk");
        return files.getFileAccess(FileResourceTypes.chunk(DataFixTypes.CHUNK, info), old.resolve(FileRelation.REGION));
    }

    private static List<FileAccess<SavedDataNbt>> structureData(FileAccessProvider files, List<String> ids) {
        List<FileAccess<SavedDataNbt>> access = new ArrayList<>();
        for (String id : ids) {
            access.add(files.getFileAccess(FileResourceTypes.savedData(References.SAVED_DATA_STRUCTURE_FEATURE_INDICES,
                CompressedNbt.MissingSeverity.MINOR), FileRelation.DATA.forFile(id + ".dat")));
        }
        return access;
    }

    private static void extractAndStore(Dynamic<Tag> levelData, List<DimensionEntry> entries, UpgradeProgress progress)
            throws IOException {
        progress.setStatus(UpgradeProgress.Status.COUNTING);
        for (DimensionEntry entry : entries) {
            for (FileAccess<SavedDataNbt> source : entry.structureFileAccess) {
                Optional<Dynamic<Tag>> data = source.getOnlyFile().read();
                if (data.isPresent()) {
                    extractLegacyStructureData(data.get(), entry.structures);
                }
            }
            progress.addTotalFileFixOperations(entry.structures.size());
        }
        progress.setStatus(UpgradeProgress.Status.UPGRADING);
        for (DimensionEntry entry : entries) {
            String generator = "minecraft:noise";
            if (entry.dimensionKey == OVERWORLD_KEY) {
                generator = switch (levelData.get("generatorName").asString("buffet")) {
                    case "flat" -> "minecraft:flat";
                    case "debug_all_block_states" -> "minecraft:debug";
                    default -> "minecraft:noise";
                };
            }
            CompoundTag context = ChunkMap.getChunkDataFixContextTag(entry.dimensionKey,
                Optional.ofNullable(Identifier.tryParse(generator)));
            storeToChunks(entry.structures, entry.chunkFileAccess.getOnlyFile(), context, progress);
        }
    }

    private static void extractLegacyStructureData(Dynamic<Tag> structureData,
            Long2ObjectMap<LegacyStructureData> container) {
        Map<Dynamic<Tag>, Dynamic<Tag>> features = structureData.get("Features")
            .asMap(feature -> feature, feature -> feature);
        for (Dynamic<Tag> value : features.values()) {
            long position = ChunkPos.pack(value.get("ChunkX").asInt(0), value.get("ChunkZ").asInt(0));
            List<Dynamic<Tag>> children = value.get("Children").asList(child -> child);
            Dynamic<Tag> feature = value;
            if (!children.isEmpty()) {
                Optional<String> current = children.getFirst().get("id").asString().result()
                    .map(LEGACY_TO_CURRENT_MAP::get);
                if (current.isPresent()) {
                    feature = feature.set("id", feature.createString(current.get()));
                }
            }
            Dynamic<Tag> stored = feature;
            stored.get("id").asString().ifSuccess(id -> {
                container.computeIfAbsent(position, key -> new LegacyStructureData()).addStart(id, stored);
                for (int x = ChunkPos.getX(position) - STRUCTURE_RANGE; x <= ChunkPos.getX(position) + STRUCTURE_RANGE; x++) {
                    for (int z = ChunkPos.getZ(position) - STRUCTURE_RANGE; z <= ChunkPos.getZ(position) + STRUCTURE_RANGE; z++) {
                        container.computeIfAbsent(ChunkPos.pack(x, z), key -> new LegacyStructureData())
                            .addIndex(id, position);
                    }
                }
            });
        }
    }

    private static void storeToChunks(Long2ObjectMap<LegacyStructureData> structures, ChunkNbt chunks,
            CompoundTag context, UpgradeProgress progress) {
        List<Long2ObjectMap.Entry<LegacyStructureData>> ordered = structures.long2ObjectEntrySet().stream()
            .sorted(Comparator.comparingLong(entry -> ChunkPos.pack(ChunkPos.getRegionX(entry.getLongKey()),
                ChunkPos.getRegionZ(entry.getLongKey()))))
            .toList();
        FutureWindow window = new FutureWindow(8);
        for (Long2ObjectMap.Entry<LegacyStructureData> entry : ordered) {
            if (progress.isCanceled()) {
                throw new CanceledFileFixException();
            }
            LegacyStructureData legacy = entry.getValue();
            progress.incrementFinishedOperationsBy(window.push(
                chunks.updateChunk(ChunkPos.unpack(entry.getLongKey()), context, tag -> {
                    CompoundTag level = tag.getCompoundOrEmpty("Level");
                    CompoundTag structure = level.getCompoundOrEmpty("Structures");
                    CompoundTag starts = structure.getCompoundOrEmpty("Starts");
                    CompoundTag references = structure.getCompoundOrEmpty("References");
                    legacy.starts().forEach((id, value) -> starts.put(id, value.convert(NbtOps.INSTANCE).getValue()));
                    legacy.indexes().forEach((id, indexes) -> references.putLongArray(id, indexes.toLongArray()));
                    structure.put("Starts", starts);
                    structure.put("References", references);
                    level.put("Structures", structure);
                    tag.put("Level", level);
                    return tag;
                })));
        }
        progress.incrementFinishedOperationsBy(window.waitForAll());
    }

    public static record LegacyStructureData(Map<String, Dynamic<?>> starts, Map<String, LongList> indexes) {
        public LegacyStructureData() {
            this(new HashMap<>(), new HashMap<>());
        }

        public void addStart(String id, Dynamic<Tag> data) {
            starts.put(id, data);
        }

        public void addIndex(String id, long sourcePos) {
            indexes.computeIfAbsent(id, key -> new LongArrayList()).add(sourcePos);
        }
    }

    private record DimensionEntry(ResourceKey<Level> dimensionKey, List<FileAccess<SavedDataNbt>> structureFileAccess,
            FileAccess<ChunkNbt> chunkFileAccess, Long2ObjectOpenHashMap<LegacyStructureData> structures) {}

    private static final class FutureWindow {
        private final int limit;
        private final List<CompletableFuture<?>> futures = new ArrayList<>();

        FutureWindow(int limit) {
            this.limit = limit;
        }

        int push(CompletableFuture<?> future) {
            int finished = futures.size() >= limit ? waitOnAny() : 0;
            futures.add(future);
            return finished;
        }

        private int waitOnAny() {
            int before = futures.size();
            CompletableFuture.anyOf(futures.toArray(CompletableFuture[]::new)).join();
            futures.removeIf(CompletableFuture::isDone);
            return before - futures.size();
        }

        int waitForAll() {
            int before = futures.size();
            CompletableFuture.allOf(futures.toArray(CompletableFuture[]::new)).join();
            futures.clear();
            return before;
        }
    }
}
