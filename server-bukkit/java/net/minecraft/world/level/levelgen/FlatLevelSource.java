// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen;
import java.util.Objects;
import java.util.stream.Stream;
import net.minecraft.core.HolderSet;
import net.minecraft.util.Util;
import net.minecraft.world.level.biome.FixedBiomeSource;
import net.minecraft.world.level.block.Blocks;
import net.minecraft.world.level.block.state.BlockState;
import com.mojang.serialization.MapCodec;
import java.util.List;
import java.util.Set;
import java.util.concurrent.CompletableFuture;
import net.minecraft.core.BlockPos;
import net.minecraft.core.Holder;
import net.minecraft.core.HolderLookup;
import net.minecraft.server.level.WorldGenRegion;
import net.minecraft.world.level.LevelHeightAccessor;
import net.minecraft.world.level.NoiseColumn;
import net.minecraft.world.level.StructureManager;
import net.minecraft.world.level.biome.Biome;
import net.minecraft.world.level.biome.BiomeManager;
import net.minecraft.world.level.chunk.ChunkAccess;
import net.minecraft.world.level.chunk.ChunkGenerator;
import net.minecraft.world.level.chunk.ChunkGeneratorStructureState;
import net.minecraft.world.level.levelgen.blending.Blender;
import net.minecraft.world.level.levelgen.densityfunction.SamplerContext;
import net.minecraft.world.level.levelgen.flat.FlatLevelGeneratorSettings;
import net.minecraft.world.level.levelgen.structure.StructureSet;
public class FlatLevelSource extends ChunkGenerator {
    private final FlatLevelGeneratorSettings settings;
    public static final MapCodec<FlatLevelSource> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.levelgen.FlatLevelSource.class, "CODEC");
    public FlatLevelSource(FlatLevelGeneratorSettings generatorSettings) {
        super(new FixedBiomeSource(generatorSettings.getBiome()), Util.memoize(generatorSettings::adjustGenerationSettings));
        this.settings = generatorSettings;
    }
    public ChunkGeneratorStructureState createState(HolderLookup<StructureSet> structureSets, RandomState randomState, long levelSeed) {
        Stream<Holder<StructureSet>> chosen = settings.structureOverrides().map(HolderSet::stream)
            .orElseGet(() -> structureSets.listElements().map(element -> element));
        return ChunkGeneratorStructureState.createForFlat(randomState, levelSeed, getOrigin(randomState), biomeSource, chosen);
    }
    protected MapCodec<? extends ChunkGenerator> codec() {
        return CODEC;
    }
    public FlatLevelGeneratorSettings settings() {
        return settings;
    }
    public int getSpawnHeight(LevelHeightAccessor heightAccessor) {
        return heightAccessor.getMinY() + Math.min(heightAccessor.getHeight(), settings.getLayers().size());
    }
    public CompletableFuture<ChunkAccess> buildTerrain(ChunkAccess chunk, Blender blender, RandomState randomState, StructureManager structureManager, BiomeManager biomeManager, WorldGenRegion carverBiomeRegion, Set<Holder<Biome>> possibleBiomes) {
        List<BlockState> stack = settings.getLayers();
        BlockPos.MutableBlockPos cursor = new BlockPos.MutableBlockPos();
        Heightmap oceanFloor = chunk.getOrCreateHeightmapUnprimed(Heightmap.Types.OCEAN_FLOOR_WG);
        Heightmap worldSurface = chunk.getOrCreateHeightmapUnprimed(Heightmap.Types.WORLD_SURFACE_WG);
        int count = Math.min(chunk.getHeight(), stack.size());
        for (int level = 0; level < count; level++) {
            BlockState state = stack.get(level);
            if (state == null) {
                continue;
            }
            int y = chunk.getMinY() + level;
            for (int x = 0; x < 16; x++) {
                for (int z = 0; z < 16; z++) {
                    chunk.setBlockState(cursor.set(x, y, z), state);
                    oceanFloor.update(x, y, z, state);
                    worldSurface.update(x, y, z, state);
                }
            }
        }
        return CompletableFuture.completedFuture(chunk);
    }
    public int getBaseHeight(int x, int z, Heightmap.Types type, LevelHeightAccessor heightAccessor, RandomState randomState) {
        List<BlockState> stack = settings.getLayers();
        for (int level = Math.min(stack.size() - 1, heightAccessor.getMaxY()); level >= 0; level--) {
            BlockState state = stack.get(level);
            if (state != null && type.isOpaque().test(state)) {
                return heightAccessor.getMinY() + level + 1;
            }
        }
        return heightAccessor.getMinY();
    }
    public NoiseColumn getBaseColumn(int x, int z, LevelHeightAccessor heightAccessor, RandomState randomState) {
        BlockState[] column = settings.getLayers().stream().limit(heightAccessor.getHeight())
            .map(state -> state == null ? Blocks.AIR.defaultBlockState() : state).toArray(BlockState[]::new);
        return new NoiseColumn(heightAccessor.getMinY(), column);
    }
    public void addDebugScreenInfo(List<String> result, RandomState randomState, BlockPos feetPos, SamplerContext samplerContext) {
        return;
    }
    public void spawnOriginalMobs(WorldGenRegion worldGenRegion) {
        return;
    }
    public int getMinY() {
        return 0;
    }
    public int getGenDepth() {
        return 384;
    }
    public int getSeaLevel() {
        return -63;

    }
}
