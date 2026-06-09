package net.minecraft.server.level;

import net.minecraft.resources.Identifier;
import net.minecraft.core.registries.Registries;
import net.minecraft.server.dedicated.DedicatedServer;
import org.basaltmc.bukkit.facade.ServerAccess;
import org.basaltmc.bukkit.WorldNatives;
import org.basaltmc.bukkit.facade.Effects;
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
import net.minecraft.world.level.entity.PersistentEntitySectionManager;
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
    private ServerChunkCache chunkSource;
    private PersistentEntitySectionManager entityManager;
    private CraftWorld world;
    private ResourceKey<Level> dimension;
    private final RandomSource random = RandomSource.create();

    public ServerLevel(CraftWorld world) {
        this.world = world;
        this.entityManager = new PersistentEntitySectionManager(this);
        String[] strings = WorldNatives.worldStrings(world.getName());
        this.dimension = ResourceKey.create(Registries.DIMENSION, Identifier.parse(
            strings == null || strings[0].isEmpty() ? "minecraft:overworld" : strings[0]));
    }
    public PaperLevelOverrides serverLevelData;
    public WorldGenSettings worldGenSettings;
    public ServerLevel(MinecraftServer minecraftServer, Executor executor, LevelStorageSource.LevelStorageAccess levelStorageAccess, WorldGenSettings worldGenSettings, ResourceKey resourceKey, LevelStem levelStem, boolean flag, long value, List list, boolean flag2, ResourceKey resourceKey2, World.Environment environment, ChunkGenerator chunkGenerator, BiomeProvider biomeProvider, SavedDataStorage savedDataStorage, PaperWorldLoader.LoadedWorldData loadedWorldData) {
        throw new UnsupportedOperationException("A temporary server level cannot be made yet");
    }
    public boolean addFreshEntity(Entity entity, CreatureSpawnEvent.SpawnReason spawnReason) {
        entity.spawnReason = spawnReason;
        return entity.spawnIn(spawnReason.name().toLowerCase(java.util.Locale.ROOT));
    }
    public void addFreshEntityWithPassengers(Entity entity, CreatureSpawnEvent.SpawnReason spawnReason) {
        addFreshEntity(entity, spawnReason);
    }
    public boolean destroyBlock(BlockPos blockPos, boolean flag, Entity entity, int value) {
        throw Unsupported.hit("net.minecraft.server.level.ServerLevel.destroyBlock");
    }
    public ResourceKey dimension() {
        return dimension;
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
        return getChunk(blockPos.getX() >> 4, blockPos.getZ() >> 4).getBlockEntity(blockPos);
    }
    public BlockState getBlockState(BlockPos blockPos) {
        return Block.stateById(WorldNatives.state(world.getName(), blockPos.getX(), blockPos.getY(), blockPos.getZ(), true));
    }
    public BlockState getBlockStateIfLoaded(BlockPos blockPos) {
        int id = WorldNatives.state(world.getName(), blockPos.getX(), blockPos.getY(), blockPos.getZ(), false);
        return id < 0 ? null : Block.stateById(id);
    }
    public LevelChunk getChunk(int value, int value2) {
        return getChunkSource().getChunk(value, value2, true);
    }
    public ChunkAccess getChunk(int value, int value2, ChunkStatus chunkStatus) {
        return getChunkSource().getChunk(value, value2, true);
    }
    public ChunkAccess getChunk(int value, int value2, ChunkStatus chunkStatus, boolean flag) {
        return getChunkSource().getChunk(value, value2, flag);
    }
    public ChunkAccess getChunk(BlockPos blockPos) {
        return getChunkSource().getChunk(blockPos.getX() >> 4, blockPos.getZ() >> 4, true);
    }
    public ChunkAccess getChunkIfLoadedImmediately(int value, int value2) {
        return getChunkSource().getChunk(value, value2, false);
    }
    public synchronized ServerChunkCache getChunkSource() {
        if (chunkSource == null) {
            chunkSource = new ServerChunkCache(this);
        }
        return chunkSource;
    }
    public CraftServer getCraftServer() {
        return (CraftServer) org.bukkit.Bukkit.getServer();
    }
    public DifficultyInstance getCurrentDifficultyAt(BlockPos pos) {
        throw Unsupported.hit("net.minecraft.server.level.ServerLevel.getCurrentDifficultyAt");
    }
    public SavedDataStorage getDataStorage() {
        throw Unsupported.hit("net.minecraft.server.level.ServerLevel.getDataStorage");
    }
    public LevelEntityGetter getEntities() {
        return entityManager.getEntityGetter();
    }
    public FluidState getFluidIfLoaded(BlockPos blockPos) {
        BlockState state = getBlockStateIfLoaded(blockPos);
        return state == null ? null : state.getFluidState();
    }
    public int getHeight() {
        return world.getMaxHeight() - world.getMinHeight();
    }
    public ServerLevel getLevel() {
        return this;
    }
    public LevelLightEngine getLightEngine() {
        return getChunkSource().getLightEngine();
    }
    public int getMaxY() {
        return world.getMaxHeight() - 1;
    }
    public int getMinY() {
        return world.getMinHeight();
    }
    public ServerLevel getMinecraftWorld() {
        return this;
    }
    public RandomSource getRandom() {
        return random;
    }
    public int getRawBrightness(BlockPos blockPos, int skyDampen) {
        int block = Math.max(0, WorldNatives.lightAt(world.getName(), blockPos.getX(), blockPos.getY(), blockPos.getZ(), 0));
        int sky = Math.max(0, WorldNatives.lightAt(world.getName(), blockPos.getX(), blockPos.getY(), blockPos.getZ(), 1));
        return Math.max(block, sky - skyDampen);
    }
    public int getSeaLevel() {
        return 63;
    }
    public int getSectionIndex(int value) {
        return (value >> 4) - (getMinY() >> 4);
    }
    public int getSectionsCount() {
        return getHeight() >> 4;
    }
    public long getSeed() {
        return world.getSeed();
    }
    public MinecraftServer getServer() {
        return DedicatedServer.instance();
    }
    public StructureTemplateManager getStructureTemplateManager() {
        throw Unsupported.hit("net.minecraft.server.level.ServerLevel.getStructureTemplateManager");
    }
    public Holder getUncachedNoiseBiome(int quartX, int quartY, int quartZ) {
        throw Unsupported.hit("net.minecraft.server.level.ServerLevel.getUncachedNoiseBiome");
    }
    public CraftWorld getWorld() {
        return world;
    }
    public WorldBorder getWorldBorder() {
        throw Unsupported.hit("net.minecraft.server.level.ServerLevel.getWorldBorder");
    }
    public boolean isDebug() {
        return false;
    }
    public boolean isInsideBuildHeight(int value) {
        return value >= getMinY() && value <= getMaxY();
    }
    public boolean isOutsideBuildHeight(int value) {
        return value < getMinY() || value > getMaxY();
    }
    public boolean isOutsideBuildHeight(BlockPos blockPos) {
        return isOutsideBuildHeight(blockPos.getY());
    }
    public EntityLookup moonrise$getEntityLookup() {
        throw Unsupported.hit("net.minecraft.server.level.ServerLevel.moonrise$getEntityLookup");
    }
    public PalettedContainerFactory palettedContainerFactory() {
        return PalettedContainerFactory.create(registryAccess());
    }
    public WorldConfiguration paperConfig() {
        throw Unsupported.hit("net.minecraft.server.level.ServerLevel.paperConfig");
    }
    public List players() {
        throw Unsupported.hit("net.minecraft.server.level.ServerLevel.players");
    }
    public RegistryAccess registryAccess() {
        return ServerAccess.registries();
    }
    public boolean removeBlock(BlockPos blockPos, boolean flag) {
        throw Unsupported.hit("net.minecraft.server.level.ServerLevel.removeBlock");
    }
    public void removeBlockEntity(BlockPos blockPos) {
        getChunk(blockPos.getX() >> 4, blockPos.getZ() >> 4).removeBlockEntity(blockPos);
    }
    public void scheduleTick(BlockPos pos, Fluid fluid, int delay) {
        WorldNatives.scheduleTick(world.getName(), pos.getX(), pos.getY(), pos.getZ(), delay, true);
    }
    public void scheduleTick(BlockPos pos, Block block, int delay) {
        WorldNatives.scheduleTick(world.getName(), pos.getX(), pos.getY(), pos.getZ(), delay, false);
    }
    public void sendBlockUpdated(BlockPos blockPos, BlockState blockState, BlockState blockState2, int value) {
        getChunkSource().blockChanged(blockPos);
    }
    public boolean setBlock(BlockPos pos, BlockState state, int flags) {
        return setBlock(pos, state, flags, 512);
    }
    public boolean setBlock(BlockPos pos, BlockState state, int flags, int limit) {
        if (isOutsideBuildHeight(pos.getY())) {
            return false;
        }
        LevelChunk chunk = getChunk(pos.getX() >> 4, pos.getZ() >> 4);
        BlockState old = chunk.setBlockState(pos, state, flags);
        if (old == null) {
            return false;
        }
        if (getBlockState(pos).stateId() == state.stateId()) {
            if ((flags & 2) != 0) {
                sendBlockUpdated(pos, old, state, flags);
            }
            if ((flags & 1) != 0) {
                updateNeighborsAt(pos, old.getBlock());
            }
            if ((flags & 16) == 0 && limit > 0) {
                int shapeFlags = flags & -34;
                old.updateIndirectNeighbourShapes(this, pos, shapeFlags, limit - 1);
                state.updateNeighbourShapes(this, pos, shapeFlags, limit - 1);
                state.updateIndirectNeighbourShapes(this, pos, shapeFlags, limit - 1);
            }
        }
        return true;
    }
    public StructureManager structureManager() {
        throw Unsupported.hit("net.minecraft.server.level.ServerLevel.structureManager");
    }
    public void updateNeighborsAt(BlockPos pos, Block sourceBlock) {
        Effects.neighborsAt(this, pos, sourceBlock.defaultBlockState());
    }
    public void updateNeighbourForOutputSignal(BlockPos blockPos, Block block) {
        return;
    }
    public void updatePOIOnBlockStateChange(BlockPos pos, BlockState oldState, BlockState newState) {
        return;
    }
}
