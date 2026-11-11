// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen;
import java.util.stream.Collectors;
import java.util.stream.StreamSupport;
import net.minecraft.core.SectionPos;
import net.minecraft.core.registries.BuiltInRegistries;
import net.minecraft.util.Mth;
import net.minecraft.world.level.biome.FixedBiomeSource;
import net.minecraft.world.level.block.Blocks;
import com.mojang.serialization.MapCodec;
import java.util.List;
import java.util.Set;
import java.util.concurrent.CompletableFuture;
import net.minecraft.core.BlockPos;
import net.minecraft.world.level.ChunkPos;
import net.minecraft.core.Holder;
import net.minecraft.server.level.WorldGenRegion;
import net.minecraft.world.level.LevelHeightAccessor;
import net.minecraft.world.level.NoiseColumn;
import net.minecraft.world.level.StructureManager;
import net.minecraft.world.level.WorldGenLevel;
import net.minecraft.world.level.biome.Biome;
import net.minecraft.world.level.biome.BiomeManager;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.chunk.ChunkAccess;
import net.minecraft.world.level.chunk.ChunkGenerator;
import net.minecraft.world.level.levelgen.blending.Blender;
import net.minecraft.world.level.levelgen.densityfunction.SamplerContext;
public class DebugLevelSource extends ChunkGenerator {
    private static final class Grid {
        static final List<BlockState> ALL_BLOCKS = StreamSupport.stream(BuiltInRegistries.BLOCK.spliterator(), false)
            .flatMap(block -> block.getStateDefinition().getPossibleStates().stream()).collect(Collectors.toList());
        static final int WIDTH = Mth.ceil(Mth.sqrt((float) ALL_BLOCKS.size()));
        static final int HEIGHT = Mth.ceil((float) ALL_BLOCKS.size() / (float) WIDTH);
    }
    public static final MapCodec<DebugLevelSource> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.levelgen.DebugLevelSource.class, "CODEC");
    protected static final BlockState AIR = Blocks.AIR.defaultBlockState();
    protected static final BlockState BARRIER = Blocks.BARRIER.defaultBlockState();
    public static final int HEIGHT = 70;
    public static final int BARRIER_HEIGHT = 60;
    public DebugLevelSource(Holder.Reference<Biome> plains) {
        super(new FixedBiomeSource(plains));
    }
    protected MapCodec<? extends ChunkGenerator> codec() {
        return CODEC;
    }
    public void applyBiomeDecoration(WorldGenLevel level, ChunkAccess chunk, StructureManager structureManager) {
        BlockPos.MutableBlockPos cursor = new BlockPos.MutableBlockPos();
        ChunkPos pos = chunk.getPos();
        for (int x = 0; x < 16; x++) {
            for (int z = 0; z < 16; z++) {
                int worldX = SectionPos.sectionToBlockCoord(pos.x(), x);
                int worldZ = SectionPos.sectionToBlockCoord(pos.z(), z);
                level.setBlock(cursor.set(worldX, BARRIER_HEIGHT, worldZ), BARRIER, 2);
                level.setBlock(cursor.set(worldX, HEIGHT, worldZ), getBlockStateFor(worldX, worldZ), 2);
            }
        }
    }
    public CompletableFuture<ChunkAccess> buildTerrain(ChunkAccess chunk, Blender blender, RandomState randomState, StructureManager structureManager, BiomeManager biomeManager, WorldGenRegion carverBiomeRegion, Set<Holder<Biome>> possibleBiomes) {
        return CompletableFuture.completedFuture(chunk);
    }
    public int getBaseHeight(int x, int z, Heightmap.Types type, LevelHeightAccessor heightAccessor, RandomState randomState) {
        return 0;
    }
    public NoiseColumn getBaseColumn(int x, int z, LevelHeightAccessor heightAccessor, RandomState randomState) {
        return new NoiseColumn(0, new BlockState[0]);
    }
    public void addDebugScreenInfo(List<String> result, RandomState randomState, BlockPos feetPos, SamplerContext samplerContext) {
        return;
    }
    public static BlockState getBlockStateFor(int worldX, int worldZ) {
        if (worldX <= 0 || worldZ <= 0 || worldX % 2 == 0 || worldZ % 2 == 0) {
            return AIR;
        }
        int column = worldX / 2;
        int row = worldZ / 2;
        if (column > Grid.WIDTH || row > Grid.HEIGHT) {
            return AIR;
        }
        int index = Mth.abs(column * Grid.WIDTH + row);
        return index < Grid.ALL_BLOCKS.size() ? Grid.ALL_BLOCKS.get(index) : AIR;
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
        return 63;

    }
}
