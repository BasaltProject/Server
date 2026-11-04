// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.server.level;
import java.util.List;
import java.util.function.Predicate;
import java.util.function.Supplier;
import net.minecraft.core.BlockPos;
import net.minecraft.core.Holder;
import net.minecraft.core.RegistryAccess;
import net.minecraft.core.particles.ParticleOptions;
import net.minecraft.server.MinecraftServer;
import net.minecraft.sounds.SoundEvent;
import net.minecraft.sounds.SoundSource;
import net.minecraft.util.RandomSource;
import net.minecraft.util.StaticCache2D;
import net.minecraft.world.DifficultyInstance;
import net.minecraft.world.attribute.EnvironmentAttributeReader;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.entity.player.Player;
import net.minecraft.world.flag.FeatureFlagSet;
import net.minecraft.world.level.ChunkPos;
import net.minecraft.world.level.WorldGenLevel;
import net.minecraft.world.level.biome.Biome;
import net.minecraft.world.level.biome.BiomeManager;
import net.minecraft.world.level.block.Block;
import net.minecraft.world.level.block.entity.BlockEntity;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.blockscan.BlockMatcher;
import net.minecraft.world.level.border.WorldBorder;
import net.minecraft.world.level.chunk.ChunkAccess;
import net.minecraft.world.level.chunk.ChunkSource;
import net.minecraft.world.level.chunk.status.ChunkStatus;
import net.minecraft.world.level.chunk.status.ChunkStep;
import net.minecraft.world.level.dimension.DimensionType;
import net.minecraft.world.level.entity.EntityTypeTest;
import net.minecraft.world.level.gameevent.GameEvent;
import net.minecraft.world.level.levelgen.Heightmap;
import net.minecraft.world.level.lighting.LevelLightEngine;
import net.minecraft.world.level.material.Fluid;
import net.minecraft.world.level.material.FluidState;
import net.minecraft.world.level.storage.LevelData;
import net.minecraft.world.phys.AABB;
import net.minecraft.world.phys.Vec3;
import net.minecraft.world.ticks.LevelTickAccess;
public class WorldGenRegion implements WorldGenLevel {
    private static final int kWriteRadius = 1;
    private final ServerLevel level;
    private final StaticCache2D<GenerationChunkHolder> cache;
    private final ChunkStep generatingStep;
    private final ChunkAccess center;
    private Supplier<String> currentlyGenerating;

    public WorldGenRegion(ServerLevel level, StaticCache2D<GenerationChunkHolder> cache, ChunkStep generatingStep, ChunkAccess center) {
        this.level = level;
        this.cache = cache;
        this.generatingStep = generatingStep;
        this.center = center;
    }
    public boolean isOldChunkAround(ChunkPos pos, int range) {
        return false;
    }
    public ChunkPos getCenter() {
        return center.getPos();
    }
    public void setCurrentlyGenerating(Supplier<String> currentlyGenerating) {
        this.currentlyGenerating = currentlyGenerating;
    }
    public ChunkAccess getChunk(int chunkX, int chunkZ) {
        return level.getChunk(chunkX, chunkZ);
    }
    public ChunkAccess getChunk(int chunkX, int chunkZ, ChunkStatus targetStatus, boolean loadOrGenerate) {
        return level.getChunk(chunkX, chunkZ);
    }
    public boolean hasChunk(int chunkX, int chunkZ) {
        return level.hasChunk(chunkX, chunkZ);
    }
    public BlockState getBlockState(BlockPos pos) {
        return level.getBlockState(pos);
    }
    public FluidState getFluidState(BlockPos pos) {
        return level.getFluidState(pos);
    }
    public Player getNearestPlayer(double x, double y, double z, double maxDist, Predicate<Entity> predicate) {
        return level.getNearestPlayer(x, y, z, maxDist, predicate);
    }
    public int getSkyDarken() {
        return 0;
    }
    public BiomeManager getBiomeManager() {
        return level.getBiomeManager();
    }
    public Holder<Biome> getUncachedNoiseBiome(int quartX, int quartY, int quartZ) {
        return level.getUncachedNoiseBiome(quartX, quartY, quartZ);
    }
    public LevelLightEngine getLightEngine() {
        return level.getLightEngine();
    }
    public boolean destroyBlock(BlockPos pos, boolean dropResources, Entity breaker, int updateLimit) {
        return ensureCanWrite(pos) && level.destroyBlock(pos, dropResources, breaker, updateLimit);
    }
    public BlockEntity getBlockEntity(BlockPos pos) {
        return level.getBlockEntity(pos);
    }
    public BlockMatcher findBlocksIn(BlockPos from, BlockPos to) {
        return level.findBlocksIn(from, to);
    }
    public boolean isWithinWriteZone(BlockPos pos) {
        return Math.abs(pos.getX() / 16 - center.getPos().x()) <= kWriteRadius && Math.abs(pos.getZ() / 16 - center.getPos().z()) <= kWriteRadius;
    }
    public boolean ensureCanWrite(BlockPos pos) {
        return isWithinWriteZone(pos);
    }
    public boolean setBlock(BlockPos pos, BlockState blockState, int updateFlags, int updateLimit) {
        return ensureCanWrite(pos) && level.setBlock(pos, blockState, updateFlags, updateLimit);
    }
    public boolean addFreshEntity(Entity entity) {
        return level.addFreshEntity(entity);
    }
    public boolean removeBlock(BlockPos pos, boolean movedByPiston) {
        return ensureCanWrite(pos) && level.removeBlock(pos, movedByPiston);
    }
    public WorldBorder getWorldBorder() {
        return level.getWorldBorder();
    }
    public boolean isClientSide() {
        return false;
    }
    public ServerLevel getLevel() {
        return level;
    }
    public RegistryAccess registryAccess() {
        return level.registryAccess();
    }
    public FeatureFlagSet enabledFeatures() {
        return level.enabledFeatures();
    }
    public LevelData getLevelData() {
        return level.getLevelData();
    }
    public DifficultyInstance getCurrentDifficultyAt(BlockPos pos) {
        return level.getCurrentDifficultyAt(pos);
    }
    public MinecraftServer getServer() {
        return level.getServer();
    }
    public ChunkSource getChunkSource() {
        return level.getChunkSource();
    }
    public long getSeed() {
        return level.getSeed();
    }
    public LevelTickAccess<Block> getBlockTicks() {
        return level.getBlockTicks();
    }
    public LevelTickAccess<Fluid> getFluidTicks() {
        return level.getFluidTicks();
    }
    public int getSeaLevel() {
        return level.getSeaLevel();
    }
    public RandomSource getRandom() {
        return level.getRandom();
    }
    public int getHeight(Heightmap.Types type, int x, int z) {
        return level.getHeight(type, x, z);
    }
    public void playSound(Entity except, BlockPos pos, SoundEvent sound, SoundSource source, float volume, float pitch) {
        level.playSound(except, pos, sound, source, volume, pitch);
    }
    public void addParticle(ParticleOptions particle, double x, double y, double z, double xd, double yd, double zd) {
        level.addParticle(particle, x, y, z, xd, yd, zd);
    }
    public void levelEvent(Entity source, int type, BlockPos pos, int data) {
        level.levelEvent(source, type, pos, data);
    }
    public void gameEvent(Holder<GameEvent> gameEvent, Vec3 position, GameEvent.Context context) {
        level.gameEvent(gameEvent, position, context);
    }
    public DimensionType dimensionType() {
        return level.dimensionType();
    }
    public boolean isStateAtPosition(BlockPos pos, Predicate<BlockState> predicate) {
        return predicate.test(getBlockState(pos));
    }
    public boolean isFluidAtPosition(BlockPos pos, Predicate<FluidState> predicate) {
        return predicate.test(getFluidState(pos));
    }
    public <T extends Entity> List<T> getEntities(EntityTypeTest<Entity, T> type, AABB bb, Predicate<? super T> selector) {
        return level.getEntities(type, bb, selector);
    }
    public List<Entity> getEntities(Entity except, AABB bb, Predicate<? super Entity> selector) {
        return level.getEntities(except, bb, selector);
    }
    public List<Player> players() {
        return level.players();
    }
    public int getMinY() {
        return level.getMinY();
    }
    public int getHeight() {
        return level.getHeight();
    }
    public long nextSubTickCount() {
        return level.nextSubTickCount();
    }
    public EnvironmentAttributeReader environmentAttributes() {
        return level.environmentAttributes();
    }
}
