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
    default ChunkAccess getChunk(int chunkX, int chunkZ, ChunkStatus targetStatus, boolean loadOrGenerate) {
        if (this instanceof Level level) {
            return level.getChunk(chunkX, chunkZ);
        }
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.LevelReader#getChunk");
    }
    default boolean hasChunk(int chunkX, int chunkZ) {
        return this instanceof net.minecraft.server.level.ServerLevel server
            && org.basaltmc.bukkit.WorldNatives.chunkLoaded(server.getWorld().getName(), chunkX, chunkZ);
    }
    default int getHeight(Heightmap.Types type, int x, int z) {
        if (this instanceof net.minecraft.server.level.ServerLevel level) {
            return level.getChunkSource().getGenerator().getBaseHeight(x, z, type, level, null);
        }
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.LevelReader#getHeight");
    }
    default int getHeight(Heightmap.Types type, BlockPos pos) {
        return getHeight(type, pos.getX(), pos.getZ());
    }
    default int getSkyDarken() {
        return 0;
    }
    default BiomeManager getBiomeManager() {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.LevelReader#getBiomeManager");
    }
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
    default Holder<Biome> getUncachedNoiseBiome(int quartX, int quartY, int quartZ) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.LevelReader#getUncachedNoiseBiome");
    }
    default boolean isClientSide() {
        return false;
    }
    default int getSeaLevel() {
        return this instanceof Level level ? level.getSeaLevel() : 63;
    }
    default DimensionType dimensionType() {
        if (this instanceof Level level) {
            return level.dimensionType();
        }
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.LevelReader#dimensionType");
    }
    default int getMinY() {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.LevelReader#getMinY");
    }
    default int getHeight() {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.LevelReader#getHeight");
    }
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
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.LevelReader#findBlocksIn");
    }
    default BlockMatcher findBlocksIn(BlockPos from, BlockPos to) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.LevelReader#findBlocksIn");
    }
    default OrderedBlockMatcher findBlocksIn(Iterable<BlockPos> positions) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.LevelReader#findBlocksIn");
    }
    default OrderedBlockMatcher findBlocksInBoxByManhattanDistance(BlockPos origin, int range) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.LevelReader#findBlocksInBoxByManhattanDistance");
    }
    default OrderedBlockMatcher findBlocksInBoxByManhattanDistance(BlockPos origin, int rangeXZ, int rangeY) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.LevelReader#findBlocksInBoxByManhattanDistance");
    }
    default OrderedBlockMatcher findBlocksInManhattan(BlockPos origin, int range) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.LevelReader#findBlocksInManhattan");
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
    default RegistryAccess registryAccess() {
        if (this instanceof Level level) {
            return level.registryAccess();
        }
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.LevelReader#registryAccess");
    }
    default FeatureFlagSet enabledFeatures() {
        return net.minecraft.world.flag.FeatureFlagSet.of();
    }
    default <T> HolderLookup<T> holderLookup(ResourceKey<? extends Registry<? extends T>> key) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.LevelReader#holderLookup");
    }
    default EnvironmentAttributeReader environmentAttributes() {
        if (this instanceof Level level) {
            return level.environmentAttributes();
        }
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.LevelReader#environmentAttributes");
    }
}
