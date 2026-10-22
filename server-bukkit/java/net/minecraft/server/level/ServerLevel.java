package net.minecraft.server.level;

import ca.spottedleaf.moonrise.patches.chunk_system.level.entity.EntityLookup;
import io.papermc.paper.configuration.WorldConfiguration;
import io.papermc.paper.world.PaperWorldLoader;
import io.papermc.paper.world.saveddata.PaperLevelOverrides;
import java.util.List;
import java.util.Map;
import java.util.concurrent.Executor;
import net.minecraft.core.BlockPos;
import net.minecraft.core.Holder;
import net.minecraft.core.RegistryAccess;
import net.minecraft.resources.ResourceKey;
import net.minecraft.server.MinecraftServer;
import net.minecraft.util.RandomSource;
import net.minecraft.world.DifficultyInstance;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.flag.FeatureFlagSet;
import net.minecraft.world.level.Level;
import net.minecraft.world.level.StructureManager;
import net.minecraft.world.level.WorldGenLevel;
import net.minecraft.world.level.biome.BiomeManager;
import net.minecraft.world.level.block.Block;
import net.minecraft.world.level.block.entity.BlockEntity;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.border.WorldBorder;
import net.minecraft.world.level.chunk.ChunkAccess;
import net.minecraft.world.level.chunk.LevelChunk;
import net.minecraft.world.level.chunk.PalettedContainerFactory;
import net.minecraft.world.level.chunk.status.ChunkStatus;
import net.minecraft.world.level.dimension.LevelStem;
import net.minecraft.world.level.entity.LevelEntityGetter;
import net.minecraft.world.level.levelgen.WorldGenSettings;
import net.minecraft.world.level.levelgen.structure.templatesystem.StructureTemplateManager;
import net.minecraft.world.level.lighting.LevelLightEngine;
import net.minecraft.world.level.material.Fluid;
import net.minecraft.world.level.material.FluidState;
import net.minecraft.world.level.storage.LevelStorageSource;
import net.minecraft.world.level.storage.SavedDataStorage;
import org.basaltmc.bukkit.Unsupported;
import org.bukkit.World;
import org.bukkit.craftbukkit.CraftServer;
import org.bukkit.craftbukkit.CraftWorld;
import org.bukkit.event.entity.CreatureSpawnEvent;
import org.bukkit.generator.BiomeProvider;
import org.bukkit.generator.ChunkGenerator;
public class ServerLevel extends Level implements WorldGenLevel, ServerEntityGetter {
    public boolean captureBlockStates = false;
    public boolean captureTreeGeneration = false;
    public Map capturedBlockStates;
    public boolean noSave = false;
    public PaperLevelOverrides serverLevelData;
    public WorldGenSettings worldGenSettings;
    public ServerLevel(MinecraftServer minecraftServer, Executor executor, LevelStorageSource.LevelStorageAccess levelStorageAccess, WorldGenSettings worldGenSettings, ResourceKey resourceKey, LevelStem levelStem, boolean flag, long value, List list, boolean flag2, ResourceKey resourceKey2, World.Environment environment, ChunkGenerator chunkGenerator, BiomeProvider biomeProvider, SavedDataStorage savedDataStorage, PaperWorldLoader.LoadedWorldData loadedWorldData) {
    }
    public boolean addFreshEntity(Entity entity, CreatureSpawnEvent.SpawnReason spawnReason) {
        throw Unsupported.hit("net.minecraft.server.level.ServerLevel.addFreshEntity");
    }
    public void addFreshEntityWithPassengers(Entity entity, CreatureSpawnEvent.SpawnReason spawnReason) {
        throw Unsupported.hit("net.minecraft.server.level.ServerLevel.addFreshEntityWithPassengers");
    }
    public boolean destroyBlock(BlockPos blockPos, boolean flag, Entity entity, int value) {
        throw Unsupported.hit("net.minecraft.server.level.ServerLevel.destroyBlock");
    }
    public ResourceKey dimension() {
        throw Unsupported.hit("net.minecraft.server.level.ServerLevel.dimension");
    }
    public Holder dimensionTypeRegistration() {
        throw Unsupported.hit("net.minecraft.server.level.ServerLevel.dimensionTypeRegistration");
    }
    public FeatureFlagSet enabledFeatures() {
        throw Unsupported.hit("net.minecraft.server.level.ServerLevel.enabledFeatures");
    }
    public BiomeManager getBiomeManager() {
        throw Unsupported.hit("net.minecraft.server.level.ServerLevel.getBiomeManager");
    }
    public BlockEntity getBlockEntity(BlockPos blockPos) {
        throw Unsupported.hit("net.minecraft.server.level.ServerLevel.getBlockEntity");
    }
    public BlockState getBlockState(BlockPos blockPos) {
        throw Unsupported.hit("net.minecraft.server.level.ServerLevel.getBlockState");
    }
    public BlockState getBlockStateIfLoaded(BlockPos blockPos) {
        throw Unsupported.hit("net.minecraft.server.level.ServerLevel.getBlockStateIfLoaded");
    }
    public LevelChunk getChunk(int value, int value2) {
        throw Unsupported.hit("net.minecraft.server.level.ServerLevel.getChunk");
    }
    public ChunkAccess getChunk(int value, int value2, ChunkStatus chunkStatus) {
        throw Unsupported.hit("net.minecraft.server.level.ServerLevel.getChunk");
    }
    public ChunkAccess getChunk(int value, int value2, ChunkStatus chunkStatus, boolean flag) {
        throw Unsupported.hit("net.minecraft.server.level.ServerLevel.getChunk");
    }
    public ChunkAccess getChunk(BlockPos blockPos) {
        throw Unsupported.hit("net.minecraft.server.level.ServerLevel.getChunk");
    }
    public ChunkAccess getChunkIfLoadedImmediately(int value, int value2) {
        throw Unsupported.hit("net.minecraft.server.level.ServerLevel.getChunkIfLoadedImmediately");
    }
    public ServerChunkCache getChunkSource() {
        throw Unsupported.hit("net.minecraft.server.level.ServerLevel.getChunkSource");
    }
    public CraftServer getCraftServer() {
        throw Unsupported.hit("net.minecraft.server.level.ServerLevel.getCraftServer");
    }
    public DifficultyInstance getCurrentDifficultyAt(BlockPos pos) {
        throw Unsupported.hit("net.minecraft.server.level.ServerLevel.getCurrentDifficultyAt");
    }
    public SavedDataStorage getDataStorage() {
        throw Unsupported.hit("net.minecraft.server.level.ServerLevel.getDataStorage");
    }
    public LevelEntityGetter getEntities() {
        throw Unsupported.hit("net.minecraft.server.level.ServerLevel.getEntities");
    }
    public FluidState getFluidIfLoaded(BlockPos blockPos) {
        throw Unsupported.hit("net.minecraft.server.level.ServerLevel.getFluidIfLoaded");
    }
    public int getHeight() {
        throw Unsupported.hit("net.minecraft.server.level.ServerLevel.getHeight");
    }
    public ServerLevel getLevel() {
        throw Unsupported.hit("net.minecraft.server.level.ServerLevel.getLevel");
    }
    public LevelLightEngine getLightEngine() {
        throw Unsupported.hit("net.minecraft.server.level.ServerLevel.getLightEngine");
    }
    public int getMaxY() {
        throw Unsupported.hit("net.minecraft.server.level.ServerLevel.getMaxY");
    }
    public int getMinY() {
        throw Unsupported.hit("net.minecraft.server.level.ServerLevel.getMinY");
    }
    public ServerLevel getMinecraftWorld() {
        throw Unsupported.hit("net.minecraft.server.level.ServerLevel.getMinecraftWorld");
    }
    public RandomSource getRandom() {
        throw Unsupported.hit("net.minecraft.server.level.ServerLevel.getRandom");
    }
    public int getRawBrightness(BlockPos blockPos, int value) {
        throw Unsupported.hit("net.minecraft.server.level.ServerLevel.getRawBrightness");
    }
    public int getSeaLevel() {
        throw Unsupported.hit("net.minecraft.server.level.ServerLevel.getSeaLevel");
    }
    public int getSectionIndex(int value) {
        throw Unsupported.hit("net.minecraft.server.level.ServerLevel.getSectionIndex");
    }
    public int getSectionsCount() {
        throw Unsupported.hit("net.minecraft.server.level.ServerLevel.getSectionsCount");
    }
    public long getSeed() {
        throw Unsupported.hit("net.minecraft.server.level.ServerLevel.getSeed");
    }
    public MinecraftServer getServer() {
        throw Unsupported.hit("net.minecraft.server.level.ServerLevel.getServer");
    }
    public StructureTemplateManager getStructureTemplateManager() {
        throw Unsupported.hit("net.minecraft.server.level.ServerLevel.getStructureTemplateManager");
    }
    public Holder getUncachedNoiseBiome(int quartX, int quartY, int quartZ) {
        throw Unsupported.hit("net.minecraft.server.level.ServerLevel.getUncachedNoiseBiome");
    }
    public CraftWorld getWorld() {
        throw Unsupported.hit("net.minecraft.server.level.ServerLevel.getWorld");
    }
    public WorldBorder getWorldBorder() {
        throw Unsupported.hit("net.minecraft.server.level.ServerLevel.getWorldBorder");
    }
    public boolean isDebug() {
        throw Unsupported.hit("net.minecraft.server.level.ServerLevel.isDebug");
    }
    public boolean isInsideBuildHeight(int value) {
        throw Unsupported.hit("net.minecraft.server.level.ServerLevel.isInsideBuildHeight");
    }
    public boolean isOutsideBuildHeight(int value) {
        throw Unsupported.hit("net.minecraft.server.level.ServerLevel.isOutsideBuildHeight");
    }
    public boolean isOutsideBuildHeight(BlockPos blockPos) {
        throw Unsupported.hit("net.minecraft.server.level.ServerLevel.isOutsideBuildHeight");
    }
    public EntityLookup moonrise$getEntityLookup() {
        throw Unsupported.hit("net.minecraft.server.level.ServerLevel.moonrise$getEntityLookup");
    }
    public PalettedContainerFactory palettedContainerFactory() {
        throw Unsupported.hit("net.minecraft.server.level.ServerLevel.palettedContainerFactory");
    }
    public WorldConfiguration paperConfig() {
        throw Unsupported.hit("net.minecraft.server.level.ServerLevel.paperConfig");
    }
    public List players() {
        throw Unsupported.hit("net.minecraft.server.level.ServerLevel.players");
    }
    public RegistryAccess registryAccess() {
        throw Unsupported.hit("net.minecraft.server.level.ServerLevel.registryAccess");
    }
    public boolean removeBlock(BlockPos blockPos, boolean flag) {
        throw Unsupported.hit("net.minecraft.server.level.ServerLevel.removeBlock");
    }
    public void removeBlockEntity(BlockPos blockPos) {
        throw Unsupported.hit("net.minecraft.server.level.ServerLevel.removeBlockEntity");
    }
    public void scheduleTick(BlockPos blockPos, Fluid fluid, int value) {
        throw Unsupported.hit("net.minecraft.server.level.ServerLevel.scheduleTick");
    }
    public void sendBlockUpdated(BlockPos blockPos, BlockState blockState, BlockState blockState2, int value) {
        throw Unsupported.hit("net.minecraft.server.level.ServerLevel.sendBlockUpdated");
    }
    public boolean setBlock(BlockPos blockPos, BlockState blockState, int value) {
        throw Unsupported.hit("net.minecraft.server.level.ServerLevel.setBlock");
    }
    public boolean setBlock(BlockPos blockPos, BlockState blockState, int value, int value2) {
        throw Unsupported.hit("net.minecraft.server.level.ServerLevel.setBlock");
    }
    public StructureManager structureManager() {
        throw Unsupported.hit("net.minecraft.server.level.ServerLevel.structureManager");
    }
    public void updateNeighborsAt(BlockPos pos, Block sourceBlock) {
        throw Unsupported.hit("net.minecraft.server.level.ServerLevel.updateNeighborsAt");
    }
    public void updateNeighbourForOutputSignal(BlockPos blockPos, Block block) {
        throw Unsupported.hit("net.minecraft.server.level.ServerLevel.updateNeighbourForOutputSignal");
    }
    public void updatePOIOnBlockStateChange(BlockPos pos, BlockState oldState, BlockState newState) {
        throw Unsupported.hit("net.minecraft.server.level.ServerLevel.updatePOIOnBlockStateChange");
    }
}
