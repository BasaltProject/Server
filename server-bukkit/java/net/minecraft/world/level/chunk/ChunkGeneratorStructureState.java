// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.chunk;

import java.util.List;
import java.util.stream.Stream;
import net.minecraft.core.Holder;
import net.minecraft.core.HolderLookup;
import net.minecraft.world.level.ChunkPos;
import net.minecraft.world.level.biome.BiomeSource;
import net.minecraft.world.level.levelgen.RandomState;
import net.minecraft.world.level.levelgen.structure.Structure;
import net.minecraft.world.level.levelgen.structure.StructureSet;
import net.minecraft.world.level.levelgen.structure.placement.ConcentricRingsStructurePlacement;
import net.minecraft.world.level.levelgen.structure.placement.StructurePlacement;

public class ChunkGeneratorStructureState {
    private final RandomState randomState;
    private final BiomeSource biomeSource;
    private final long levelSeed;
    private final ChunkPos origin;
    private final List<Holder<StructureSet>> possibleStructureSets;

    public static ChunkGeneratorStructureState createForFlat(RandomState randomState, long levelSeed, ChunkPos origin, BiomeSource biomeSource, Stream<Holder<StructureSet>> structureOverrides) {
        return new ChunkGeneratorStructureState(randomState, biomeSource, levelSeed, origin, structureOverrides.toList());
    }

    public static ChunkGeneratorStructureState createForNormal(RandomState randomState, long levelSeed, ChunkPos origin, BiomeSource biomeSource, HolderLookup<StructureSet> allStructures) {
        return new ChunkGeneratorStructureState(randomState, biomeSource, levelSeed, origin, allStructures.listElements().map(reference -> (Holder<StructureSet>) reference).toList());
    }

    private ChunkGeneratorStructureState(RandomState randomState, BiomeSource biomeSource, long levelSeed, ChunkPos origin, List<Holder<StructureSet>> possibleStructureSets) {
        this.randomState = randomState;
        this.biomeSource = biomeSource;
        this.levelSeed = levelSeed;
        this.origin = origin;
        this.possibleStructureSets = possibleStructureSets;
    }

    public static ChunkGeneratorStructureState ofSeed(RandomState randomState, long levelSeed) {
        return new ChunkGeneratorStructureState(randomState, null, levelSeed, ChunkPos.ZERO, List.of());
    }

    public List<Holder<StructureSet>> possibleStructureSets() {
        return possibleStructureSets;
    }

    public ChunkPos getDimensionOrigin() {
        return origin;
    }

    public void ensureStructuresGenerated() {
        return;
    }

    public List<ChunkPos> getRingPositionsFor(ConcentricRingsStructurePlacement placement) {
        return List.of();
    }

    public List<StructurePlacement> getPlacementsForStructure(Holder<Structure> structure) {
        return List.of();
    }

    public RandomState randomState() {
        return randomState;
    }

    public boolean hasStructureChunkInRange(Holder<StructureSet> structureSet, int sourceX, int sourceZ, int range) {
        return false;
    }

    public long getLevelSeed() {
        return levelSeed;
    }
}
