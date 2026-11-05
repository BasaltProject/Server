// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level;

import net.minecraft.world.level.biome.BiomeResolver;
import java.util.stream.Stream;
import net.minecraft.core.BlockPos;
import net.minecraft.core.Holder;
import net.minecraft.core.HolderLookup;
import net.minecraft.core.Registry;
import net.minecraft.core.RegistryAccess;
import net.minecraft.resources.ResourceKey;
import net.minecraft.world.attribute.EnvironmentAttributeReader;
import net.minecraft.world.flag.FeatureFlagSet;
import net.minecraft.world.level.biome.Biome;
import net.minecraft.world.level.biome.BiomeManager;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.blockscan.BlockMatcher;
import net.minecraft.world.level.blockscan.OrderedBlockMatcher;
import net.minecraft.world.level.chunk.ChunkAccess;
import net.minecraft.world.level.chunk.status.ChunkStatus;
import net.minecraft.world.level.dimension.DimensionType;
import net.minecraft.world.level.levelgen.Heightmap;
import net.minecraft.world.phys.AABB;
public interface LevelReader extends BlockAndLightGetter, CollisionGetter, SignalGetter, BiomeResolver {
    ChunkAccess getChunk(int chunkX, int chunkZ, ChunkStatus targetStatus, boolean loadOrGenerate);
    default boolean hasChunk(int chunkX, int chunkZ) {
        return this instanceof net.minecraft.server.level.ServerLevel server
            && org.basaltmc.bukkit.WorldNatives.chunkLoaded(server.getWorld().getName(), chunkX, chunkZ);
    }
    default int getHeight(Heightmap.Types type, int x, int z) {
        if (this instanceof net.minecraft.server.level.ServerLevel level) {
            return level.getChunkSource().getGenerator().getBaseHeight(x, z, type, level, null);
        }
        for (int y = getMinY() + getHeight() - 1; y >= getMinY(); y--) {
            if (!getBlockState(new BlockPos(x, y, z)).isAir()) {
                return y + 1;
            }
        }
        return getMinY();
    }
    default int getHeight(Heightmap.Types type, BlockPos pos) {
        return getHeight(type, pos.getX(), pos.getZ());
    }
    default int getSkyDarken() {
        return 0;
    }
    BiomeManager getBiomeManager();
    default Holder<Biome> getBiome(BlockPos pos) {
        return getBiomeManager().getBiome(pos);
    }
    default Stream<BlockState> getBlockStatesIfLoaded(AABB box) {
        java.util.List<BlockState> found = new java.util.ArrayList<>();
        for (int x = (int) Math.floor(box.minX); x <= (int) Math.floor(box.maxX); x++) {
            for (int z = (int) Math.floor(box.minZ); z <= (int) Math.floor(box.maxZ); z++) {
                if (!hasChunk(x >> 4, z >> 4)) {
                    continue;
                }
                for (int y = (int) Math.floor(box.minY); y <= (int) Math.floor(box.maxY); y++) {
                    found.add(getBlockState(new net.minecraft.core.BlockPos(x, y, z)));
                }
            }
        }
        return found.stream();
    }
    default Holder<Biome> getNoiseBiome(int quartX, int quartY, int quartZ) {
        return getUncachedNoiseBiome(quartX, quartY, quartZ);
    }
    Holder<Biome> getUncachedNoiseBiome(int quartX, int quartY, int quartZ);
    default boolean isClientSide() {
        return false;
    }
    default int getSeaLevel() {
        return this instanceof Level level ? level.getSeaLevel() : 63;
    }
    DimensionType dimensionType();
    int getMinY();
    int getHeight();
    default BlockPos getHeightmapPos(Heightmap.Types type, BlockPos pos) {
        return new BlockPos(pos.getX(), getHeight(type, pos.getX(), pos.getZ()), pos.getZ());
    }
    default boolean isEmptyBlock(BlockPos pos) {
        return getBlockState(pos).isAir();
    }
    default boolean canSeeSkyFromBelowWater(BlockPos pos) {
        if (pos.getY() >= getSeaLevel()) {
            return canSeeSky(pos);
        }
        BlockPos scan = new BlockPos(pos.getX(), getSeaLevel(), pos.getZ());
        if (!canSeeSky(scan)) {
            return false;
        }
        for (BlockPos at = scan.below(); at.getY() > pos.getY(); at = at.below()) {
            BlockState state = getBlockState(at);
            if (state.getLightDampening() > 0 && state.getFluidState().isEmpty()) {
                return false;
            }
        }
        return true;
    }
    default float getPathfindingCostFromLightLevels(BlockPos pos) {
        return getLightLevelDependentMagicValue(pos) - 0.5F;
    }
    default float getLightLevelDependentMagicValue(BlockPos pos) {
        float v = getMaxLocalRawBrightness(pos, getSkyDarken()) / 15.0F;
        float curved = v / (4.0F - 3.0F * v);
        return net.minecraft.util.Mth.lerp(dimensionType().ambientLight(), curved, 1.0F);
    }
    default ChunkAccess getChunk(BlockPos pos) {
        return getChunk(pos.getX() >> 4, pos.getZ() >> 4);
    }
    default ChunkAccess getChunk(int chunkX, int chunkZ) {
        return getChunk(chunkX, chunkZ, net.minecraft.world.level.chunk.status.ChunkStatus.FULL, true);
    }
    default ChunkAccess getChunk(int chunkX, int chunkZ, ChunkStatus status) {
        return getChunk(chunkX, chunkZ, status, true);
    }
    default BlockGetter getChunkForCollisions(int chunkX, int chunkZ) {
        return hasChunk(chunkX, chunkZ) && this instanceof net.minecraft.server.level.ServerLevel server
            ? server.getChunk(chunkX, chunkZ) : null;
    }
    default boolean isWaterAt(BlockPos pos) {
        return getFluidState(pos).is(net.minecraft.world.level.material.Fluids.WATER);
    }
    default boolean containsAnyLiquid(AABB box) {
        for (BlockPos pos : BlockPos.betweenClosed(box)) {
            if (!getFluidState(pos).isEmpty()) {
                return true;
            }
        }
        return false;
    }
    default int getMaxLocalRawBrightness(BlockPos pos) {
        return getMaxLocalRawBrightness(pos, getSkyDarken());
    }
    default int getMaxLocalRawBrightness(BlockPos pos, int skyDarkening) {
        return pos.getX() >= -30000000 && pos.getZ() >= -30000000 && pos.getX() < 30000000 && pos.getZ() < 30000000
            ? getRawBrightness(pos, skyDarkening) : 15;
    }
    default int getEffectiveSkyBrightness(BlockPos pos) {
        return Math.max(getBrightness(LightLayer.SKY, pos) - getSkyDarken(), 0);
    }
    default BlockMatcher findBlocksIn(AABB box) {
        return new net.minecraft.world.level.blockscan.BoxBlockMatcher(this, BlockPos.containing(box.minX, box.minY, box.minZ), BlockPos.containing(box.maxX - 1.0E-7, box.maxY - 1.0E-7, box.maxZ - 1.0E-7));
    }
    default BlockMatcher findBlocksIn(BlockPos from, BlockPos to) {
        return new net.minecraft.world.level.blockscan.BoxBlockMatcher(this, from, to);
    }
    default OrderedBlockMatcher findBlocksIn(Iterable<BlockPos> positions) {
        return new OrderedBlockMatcher(this, positions);
    }
    default OrderedBlockMatcher findBlocksInBoxByManhattanDistance(BlockPos origin, int range) {
        return findBlocksInBoxByManhattanDistance(origin, range, range);
    }
    default OrderedBlockMatcher findBlocksInBoxByManhattanDistance(BlockPos origin, int rangeXZ, int rangeY) {
        return new OrderedBlockMatcher(this, BlockPos.withinBoxByManhattanDistance(origin, rangeXZ, rangeY, rangeXZ));
    }
    default OrderedBlockMatcher findBlocksInManhattan(BlockPos origin, int range) {
        return new OrderedBlockMatcher(this, BlockPos.withinManhattan(origin, range));
    }
    default boolean hasChunkAt(int blockX, int blockZ) {
        return hasChunk(blockX >> 4, blockZ >> 4);
    }
    default boolean hasChunkAt(BlockPos pos) {
        return hasChunkAt(pos.getX(), pos.getZ());
    }
    default boolean hasChunksAt(BlockPos pos0, BlockPos pos1) {
        return hasChunksAt(pos0.getX(), pos0.getY(), pos0.getZ(), pos1.getX(), pos1.getY(), pos1.getZ());
    }
    default boolean hasChunksAt(int x0, int y0, int z0, int x1, int y1, int z1) {
        return y1 >= getMinY() && y0 <= getMaxY() && hasChunksAt(x0, z0, x1, z1);
    }
    default boolean hasChunksAt(int x0, int z0, int x1, int z1) {
        int minX = x0 >> 4;
        int maxX = x1 >> 4;
        int minZ = z0 >> 4;
        int maxZ = z1 >> 4;
        for (int x = minX; x <= maxX; x++) {
            for (int z = minZ; z <= maxZ; z++) {
                if (!hasChunk(x, z)) {
                    return false;
                }
            }
        }
        return true;
    }
    RegistryAccess registryAccess();
    default FeatureFlagSet enabledFeatures() {
        return net.minecraft.world.flag.FeatureFlagSet.of();
    }
    default <T> HolderLookup<T> holderLookup(ResourceKey<? extends Registry<? extends T>> key) {
        return registryAccess().lookupOrThrow(key);
    }
    EnvironmentAttributeReader environmentAttributes();
}
