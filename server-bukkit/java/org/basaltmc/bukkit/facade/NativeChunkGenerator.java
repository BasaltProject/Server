package org.basaltmc.bukkit.facade;

import net.minecraft.server.level.ServerLevel;
import net.minecraft.world.level.LevelHeightAccessor;
import net.minecraft.world.level.chunk.ChunkGenerator;
import net.minecraft.world.level.levelgen.Heightmap;
import org.basaltmc.bukkit.WorldNatives;

public final class NativeChunkGenerator extends ChunkGenerator {
    private final ServerLevel level;

    public NativeChunkGenerator(ServerLevel level) {
        this.level = level;
    }

    private static String kind(Heightmap.Types type) {
        return switch (type) {
            case WORLD_SURFACE_WG, WORLD_SURFACE -> "surface";
            case OCEAN_FLOOR_WG, OCEAN_FLOOR -> "ocean_floor";
            default -> "motion_blocking";
        };
    }

    @Override
    public int getSeaLevel() {
        return level.getSeaLevel();
    }

    @Override
    public int getMinY() {
        return level.getMinY();
    }

    @Override
    public int getGenDepth() {
        return level.getHeight();
    }

    @Override
    public int getSpawnHeight(LevelHeightAccessor heightAccessor) {
        return Math.min(getSeaLevel() + 1, level.getMinY() + level.getHeight() - 1);
    }

    @Override
    public int getBaseHeight(int x, int z, Heightmap.Types type, LevelHeightAccessor heightAccessor,
            net.minecraft.world.level.levelgen.RandomState randomState) {
        return WorldNatives.highest(level.getWorld().getName(), x, z, kind(type), true) + 1;
    }

    @Override
    public int getFirstFreeHeight(int x, int z, Heightmap.Types type, LevelHeightAccessor heightAccessor,
            net.minecraft.world.level.levelgen.RandomState randomState) {
        return getBaseHeight(x, z, type, heightAccessor, randomState);
    }

    @Override
    public int getFirstOccupiedHeight(int x, int z, Heightmap.Types type, LevelHeightAccessor heightAccessor,
            net.minecraft.world.level.levelgen.RandomState randomState) {
        return getBaseHeight(x, z, type, heightAccessor, randomState) - 1;
    }
    @Override
    public void validate() {
    }

    @Override
    public net.minecraft.world.level.ChunkPos getOrigin(net.minecraft.world.level.levelgen.RandomState randomState) {
        return net.minecraft.world.level.ChunkPos.ZERO;
    }

    @Override
    public java.util.Optional<net.minecraft.resources.Identifier> getTypeNameForDataFixer() {
        return java.util.Optional.of(net.minecraft.resources.Identifier.withDefaultNamespace("noise"));
    }

    @Override
    public java.util.concurrent.CompletableFuture<net.minecraft.world.level.chunk.ChunkAccess> createBiomes(
            net.minecraft.world.level.levelgen.RandomState randomState,
            net.minecraft.world.level.levelgen.blending.Blender blender,
            net.minecraft.world.level.StructureManager structureManager,
            net.minecraft.world.level.chunk.ChunkAccess protoChunk) {
        return java.util.concurrent.CompletableFuture.completedFuture(protoChunk);
    }

    @Override
    public java.util.concurrent.CompletableFuture<net.minecraft.world.level.chunk.ChunkAccess> buildTerrain(
            net.minecraft.world.level.chunk.ChunkAccess chunk, net.minecraft.world.level.levelgen.blending.Blender blender,
            net.minecraft.world.level.levelgen.RandomState randomState,
            net.minecraft.world.level.StructureManager structureManager,
            net.minecraft.world.level.biome.BiomeManager biomeManager,
            net.minecraft.server.level.WorldGenRegion carverBiomeRegion,
            java.util.Set<net.minecraft.core.Holder<net.minecraft.world.level.biome.Biome>> possibleBiomes) {
        return java.util.concurrent.CompletableFuture.completedFuture(chunk);
    }

    @Override
    public void applyBiomeDecoration(net.minecraft.world.level.WorldGenLevel level,
            net.minecraft.world.level.chunk.ChunkAccess chunk, net.minecraft.world.level.StructureManager structureManager) {
    }

    @Override
    public void spawnOriginalMobs(net.minecraft.server.level.WorldGenRegion worldGenRegion) {
    }

    @Override
    public void createReferences(net.minecraft.world.level.WorldGenLevel level,
            net.minecraft.world.level.StructureManager structureManager, net.minecraft.world.level.chunk.ChunkAccess centerChunk) {
    }

    @Override
    public void addDebugScreenInfo(java.util.List<String> result, net.minecraft.world.level.levelgen.RandomState randomState,
            net.minecraft.core.BlockPos feetPos, net.minecraft.world.level.levelgen.densityfunction.SamplerContext samplerContext) {
    }

    @Override
    public void createStructures(net.minecraft.core.RegistryAccess registryAccess,
            net.minecraft.world.level.chunk.ChunkGeneratorStructureState state,
            net.minecraft.world.level.StructureManager structureManager,
            net.minecraft.world.level.chunk.ChunkAccess centerChunk,
            net.minecraft.world.level.levelgen.structure.templatesystem.StructureTemplateManager structureTemplateManager,
            net.minecraft.resources.ResourceKey<net.minecraft.world.level.Level> dimension) {
        net.minecraft.world.level.ChunkPos pos = centerChunk.getPos();
        String[] rows = org.basaltmc.bukkit.WorldEnvNatives.structureStarts(level.getWorld().getName(), pos.x(), pos.z());
        var lookup = registryAccess.lookupOrThrow(net.minecraft.core.registries.Registries.STRUCTURE);
        for (String row : rows == null ? new String[0] : rows) {
            String[] parts = row.split("\\|", -1);
            String[] n = parts[3].split(",");
            var holder = lookup.get(net.minecraft.resources.ResourceKey.create(net.minecraft.core.registries.Registries.STRUCTURE,
                net.minecraft.resources.Identifier.parse(parts[0])));
            if (holder.isEmpty() || Integer.parseInt(parts[1]) != pos.x() || Integer.parseInt(parts[2]) != pos.z()) {
                continue;
            }
            var box = new net.minecraft.world.level.levelgen.structure.BoundingBox(Integer.parseInt(n[0]), Integer.parseInt(n[1]),
                Integer.parseInt(n[2]), Integer.parseInt(n[3]), Integer.parseInt(n[4]), Integer.parseInt(n[5]));
            var structure = (net.minecraft.world.level.levelgen.structure.Structure) holder.get().value();
            centerChunk.setStartForStructure(structure,
                new net.minecraft.world.level.levelgen.structure.StructureStart(structure, pos, 0, box));
        }
    }
}
