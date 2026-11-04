// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level;

import it.unimi.dsi.fastutil.longs.LongSet;
import java.util.List;
import java.util.Map;
import java.util.function.Consumer;
import java.util.function.Predicate;
import net.minecraft.core.BlockPos;
import net.minecraft.core.Holder;
import net.minecraft.core.HolderSet;
import net.minecraft.core.RegistryAccess;
import net.minecraft.server.level.WorldGenRegion;
import net.minecraft.tags.TagKey;
import net.minecraft.world.level.chunk.StructureAccess;
import net.minecraft.world.level.levelgen.WorldOptions;
import net.minecraft.world.level.levelgen.structure.Structure;
import net.minecraft.world.level.levelgen.structure.StructureCheck;
import net.minecraft.world.level.levelgen.structure.StructureCheckResult;
import net.minecraft.world.level.levelgen.structure.StructureStart;
import net.minecraft.world.level.levelgen.structure.placement.StructurePlacement;
public class StructureManager {
    private LevelAccessor level;
    private WorldOptions worldOptions;
    private StructureCheck structureCheck;
    private String world;

    protected StructureManager() {
    }

    public StructureManager(net.minecraft.server.level.ServerLevel level) {
        this.level = level;
        this.world = level.getWorld().getName();
    }

    private java.util.Set<String> startNamesAt(ChunkPos pos) {
        java.util.Set<String> names = new java.util.HashSet<>();
        if (world != null) {
            for (String row : org.basaltmc.bukkit.WorldEnvNatives.structureStarts(world, pos.x(), pos.z())) {
                names.add(row.substring(0, row.indexOf('|')));
            }
        }
        return names;
    }
    public StructureManager(LevelAccessor level, WorldOptions worldOptions, StructureCheck structureCheck) {
        this.level = level;
        this.worldOptions = worldOptions;
        this.structureCheck = structureCheck;
    }
    public StructureManager forWorldGenRegion(WorldGenRegion region) {
        return new StructureManager(region, worldOptions, structureCheck);
    }
    public List<StructureStart> startsForStructure(int sectionX, int sectionZ, Predicate<Structure> matcher) {
        List<StructureStart> found = new java.util.ArrayList<>();
        for (java.util.Map.Entry<Structure, StructureStart> entry : level.getChunk(sectionX, sectionZ).getAllStarts().entrySet()) {
            if (matcher.test(entry.getKey()) && entry.getValue().isValid()) {
                found.add(entry.getValue());
            }
        }
        return found;
    }
    public List<StructureStart> startsForStructure(int sectionX, int sectionZ, Structure structure) {
        return startsForStructure(sectionX, sectionZ, candidate -> candidate == structure);
    }
    public void fillStartsForStructure(Structure structure, LongSet referencesForStructure, Consumer<StructureStart> consumer) {
        for (long reference : referencesForStructure) {
            ChunkPos pos = ChunkPos.unpack(reference);
            StructureStart start = level.getChunk(pos.x(), pos.z()).getStartForStructure(structure);
            if (start != null && start.isValid()) {
                consumer.accept(start);
            }
        }
    }
    public StructureStart getStartForStructure(Structure structure, StructureAccess chunk) {
        return chunk.getStartForStructure(structure);
    }
    public void setStartForStructure(Structure structure, StructureStart start, StructureAccess chunk) {
        chunk.setStartForStructure(structure, start);
    }
    public void addReferenceForStructure(Structure structure, long reference, StructureAccess chunk) {
        chunk.addReferenceForStructure(structure, reference);
    }
    public boolean shouldGenerateStructures() {
        return !(level instanceof net.minecraft.server.level.ServerLevel serverLevel) || serverLevel.getWorld().canGenerateStructures();
    }
    public StructureStart getStructureAt(BlockPos blockPos, Structure structure) {
        return getStructureWithPieceAt(blockPos, candidate -> candidate.value() == structure);
    }
    public StructureStart getStructureAt(BlockPos blockPos, HolderSet<Structure> structures) {
        return getStructureWithPieceAt(blockPos, structures);
    }
    public StructureStart getStructureWithPieceAt(BlockPos blockPos, TagKey<Structure> structureTag) {
        return getStructureWithPieceAt(blockPos, candidate -> candidate.is(structureTag));
    }
    public StructureStart getStructureWithPieceAt(BlockPos blockPos, HolderSet<Structure> structures) {
        return getStructureWithPieceAt(blockPos, candidate -> structures.contains(candidate));
    }
    public StructureStart getStructureWithPieceAt(BlockPos blockPos, Predicate<Holder<Structure>> predicate) {
        for (java.util.Map.Entry<Structure, LongSet> entry : getAllStructuresAt(blockPos).entrySet()) {
            @SuppressWarnings("unchecked")
            Holder<Structure> holder = (Holder<Structure>) (Holder<?>) level.registryAccess()
                .lookupOrThrow(net.minecraft.core.registries.Registries.STRUCTURE).wrapAsHolder(entry.getKey());
            if (!predicate.test(holder)) {
                continue;
            }
            StructureStart[] found = new StructureStart[1];
            fillStartsForStructure(entry.getKey(), entry.getValue(), start -> {
                if (found[0] == null && structureHasPieceAt(blockPos, start)) {
                    found[0] = start;
                }
            });
            if (found[0] != null) {
                return found[0];
            }
        }
        return StructureStart.INVALID_START;
    }
    public StructureStart getStructureWithPieceAt(int x, int y, int z, Structure structure) {
        StructureStart[] found = new StructureStart[1];
        fillStartsForStructure(structure, getAllStructuresAt(x, y, z).getOrDefault(structure, new it.unimi.dsi.fastutil.longs.LongOpenHashSet()), start -> {
            if (found[0] == null && structureHasPieceAt(x, y, z, start)) {
                found[0] = start;
            }
        });
        return found[0] == null ? StructureStart.INVALID_START : found[0];
    }
    public boolean structureHasPieceAt(BlockPos blockPos, StructureStart structureStart) {
        return structureHasPieceAt(blockPos.getX(), blockPos.getY(), blockPos.getZ(), structureStart);
    }
    public boolean structureHasPieceAt(int x, int y, int z, StructureStart structureStart) {
        return structureStart.getBoundingBox() != null && structureStart.getBoundingBox().isInside(x, y, z);
    }
    public Map<Structure, LongSet> getAllStructuresAt(BlockPos pos) {
        return getAllStructuresAt(pos.getX(), pos.getY(), pos.getZ());
    }
    public Map<Structure, LongSet> getAllStructuresAt(int x, int y, int z) {
        Map<Structure, LongSet> found = new java.util.HashMap<>();
        for (java.util.Map.Entry<Structure, LongSet> entry : level.getChunk(x >> 4, z >> 4).getAllReferences().entrySet()) {
            found.put(entry.getKey(), entry.getValue());
        }
        return found;
    }
    public StructureCheckResult checkStructurePresence(ChunkPos pos, Structure structure, StructurePlacement placement, boolean createReference) {
        java.util.Set<String> names = startNamesAt(pos);
        if (names.isEmpty()) {
            return StructureCheckResult.START_NOT_PRESENT;
        }
        String key = structureName(structure);
        return key == null || names.contains(key) || names.contains("minecraft:" + key) ? StructureCheckResult.START_PRESENT : StructureCheckResult.START_NOT_PRESENT;
    }
    public void addReference(StructureStart start) {
        start.addReference();
    }
    private String structureName(Structure structure) {
        try {
            return level.registryAccess().lookupOrThrow(net.minecraft.core.registries.Registries.STRUCTURE).getKey(structure).toString();
        } catch (RuntimeException unresolved) {
            return null;
        }
    }

    public RegistryAccess registryAccess() {
        return level.registryAccess();
    }
}
