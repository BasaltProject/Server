// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.server.level;

import net.minecraft.network.protocol.game.ClientboundDamageEventPacket;
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
import com.mojang.datafixers.util.Pair;
import it.unimi.dsi.fastutil.longs.LongSet;
import java.io.IOException;
import java.nio.file.Path;
import java.util.Collection;
import java.util.UUID;
import java.util.function.BooleanSupplier;
import java.util.function.Predicate;
import java.util.stream.Stream;
import net.minecraft.CrashReport;
import net.minecraft.CrashReportCategory;
import net.minecraft.core.Direction;
import net.minecraft.core.HolderSet;
import net.minecraft.core.SectionPos;
import net.minecraft.core.particles.ExplosionParticleInfo;
import net.minecraft.core.particles.ParticleOptions;
import net.minecraft.network.protocol.game.ClientboundLevelParticlesPacket;
import net.minecraft.server.ServerScoreboard;
import net.minecraft.server.waypoints.ServerWaypointManager;
import net.minecraft.sounds.SoundEvent;
import net.minecraft.sounds.SoundSource;
import net.minecraft.tags.TagKey;
import net.minecraft.util.ProgressListener;
import net.minecraft.util.debug.LevelDebugSynchronizers;
import net.minecraft.util.random.WeightedList;
import net.minecraft.util.valueproviders.IntProvider;
import net.minecraft.world.TickRateManager;
import net.minecraft.world.attribute.EnvironmentAttributeSystem;
import net.minecraft.world.clock.ServerClockManager;
import net.minecraft.world.damagesource.DamageSource;
import net.minecraft.world.entity.ReputationEventHandler;
import net.minecraft.world.entity.ai.village.ReputationEventType;
import net.minecraft.world.entity.ai.village.poi.PoiManager;
import net.minecraft.world.entity.boss.enderdragon.EnderDragon;
import net.minecraft.world.entity.boss.enderdragon.EnderDragonPart;
import net.minecraft.world.entity.player.Player;
import net.minecraft.world.entity.raid.Raid;
import net.minecraft.world.entity.raid.Raids;
import net.minecraft.world.item.crafting.RecipeManager;
import net.minecraft.world.level.ChunkPos;
import net.minecraft.world.level.CustomSpawner;
import net.minecraft.world.level.ExplosionDamageCalculator;
import net.minecraft.world.level.biome.Biome;
import net.minecraft.world.level.biome.BiomeResolver;
import net.minecraft.world.level.dimension.end.EnderDragonFight;
import net.minecraft.world.level.entity.EntityTypeTest;
import net.minecraft.world.level.entity.LevelCallback;
import net.minecraft.world.level.gameevent.GameEvent;
import net.minecraft.world.level.gamerules.GameRules;
import net.minecraft.world.level.levelgen.structure.BoundingBox;
import net.minecraft.world.level.levelgen.structure.Structure;
import net.minecraft.world.level.pathfinder.PathTypeCache;
import net.minecraft.world.level.portal.PortalForcer;
import net.minecraft.world.level.redstone.Orientation;
import net.minecraft.world.level.saveddata.WeatherData;
import net.minecraft.world.level.saveddata.maps.MapId;
import net.minecraft.world.level.saveddata.maps.MapItemSavedData;
import net.minecraft.world.level.storage.LevelData;
import net.minecraft.world.level.storage.ServerLevelData;
import net.minecraft.world.phys.Vec3;
import net.minecraft.world.ticks.LevelTicks;
public class ServerLevel extends Level implements WorldGenLevel, ServerEntityGetter {
    private static final net.minecraft.world.item.crafting.RecipeManager kRecipes =
        new net.minecraft.world.item.crafting.RecipeManager(null);
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
        BlockState state = getBlockState(blockPos);
        if (state.isAir()) {
            return false;
        }
        if (flag) {
            world.getBlockAt(blockPos.getX(), blockPos.getY(), blockPos.getZ()).breakNaturally();
            return true;
        }
        return setBlock(blockPos, getFluidState(blockPos).createLegacyBlock(), 3, value);
    }
    private final java.util.concurrent.atomic.AtomicLong subTicks = new java.util.concurrent.atomic.AtomicLong();

    @Override
    public long nextSubTickCount() {
        return subTicks.getAndIncrement();
    }

    @Override
    public long getGameTime() {
        return world.getGameTime();
    }

    @Override
    public net.minecraft.world.Difficulty getDifficulty() {
        return net.minecraft.world.Difficulty.byId(world.getDifficulty().getValue());
    }

    @Override
    public net.minecraft.world.level.storage.LevelData getLevelData() {
        return new net.minecraft.world.level.storage.ServerLevelData() {
            @Override
            public long getGameTime() {
                return world.getGameTime();
            }

            @Override
            public boolean isHardcore() {
                return world.isHardcore();
            }

            @Override
            public net.minecraft.world.Difficulty getDifficulty() {
                return ServerLevel.this.getDifficulty();
            }

            @Override
            public boolean isDifficultyLocked() {
                return false;
            }

            @Override
            public LevelData.RespawnData getRespawnData() {
                return ServerLevel.this.getRespawnData();
            }

            @Override
            public String getLevelName() {
                return world.getName();
            }

            @Override
            public boolean isInitialized() {
                return true;
            }

            @Override
            public void setInitialized(boolean initialized) {
            }

            @Override
            public boolean isAllowCommands() {
                return true;
            }

            @Override
            public void setAllowCommands(boolean allowCommands) {
            }

            @Override
            public void setGameTime(long time) {
                world.setFullTime(time);
            }

            @Override
            public void setSpawn(LevelData.RespawnData respawnData) {
                ServerLevel.this.setRespawnData(respawnData);
            }
        };
    }

    @Override
    public net.minecraft.world.level.biome.Biome.Precipitation precipitationAt(BlockPos pos) {
        if (!isRaining() || world.getHighestBlockYAt(pos.getX(), pos.getZ()) > pos.getY()) {
            return net.minecraft.world.level.biome.Biome.Precipitation.NONE;
        }
        double temperature = org.basaltmc.bukkit.WorldEnvNatives.temperature(world.getName(), pos.getX(), pos.getY(),
            pos.getZ());
        return temperature < 0.15 ? net.minecraft.world.level.biome.Biome.Precipitation.SNOW
            : net.minecraft.world.level.biome.Biome.Precipitation.RAIN;
    }

    @Override
    protected void emitSound(double x, double y, double z, String key, net.minecraft.sounds.SoundSource source,
            float volume, float pitch) {
        world.playSound(new org.bukkit.Location(world, x, y, z), key,
            org.bukkit.SoundCategory.valueOf(source == net.minecraft.sounds.SoundSource.UI ? "MASTER" : source.name()),
            volume, pitch);
    }

    @Override
    protected void emitExplosion(double x, double y, double z, float power, boolean fire, boolean breakBlocks) {
        world.createExplosion(x, y, z, power, fire, breakBlocks);
    }

    @Override
    public boolean canHaveWeather() {
        return world.getEnvironment() == org.bukkit.World.Environment.NORMAL;
    }

    @Override
    public float getRainLevel(float a) {
        return world.hasStorm() ? 1.0F : 0.0F;
    }

    @Override
    public float getThunderLevel(float a) {
        return world.hasStorm() && world.isThundering() ? 1.0F : 0.0F;
    }

    @Override
    public void setRainLevel(float rainLevel) {
        world.setStorm(rainLevel > 0.2F);
    }

    @Override
    public void setThunderLevel(float thunderLevel) {
        world.setThundering(thunderLevel > 0.9F);
    }

    public ResourceKey dimension() {
        return dimension;
    }
    public Holder dimensionTypeRegistration() {
        String type = switch (world.getEnvironment()) {
            case NETHER -> "the_nether";
            case THE_END -> "the_end";
            default -> "overworld";
        };
        return registryAccess().lookupOrThrow(Registries.DIMENSION_TYPE).get(ResourceKey.create(Registries.DIMENSION_TYPE, Identifier.withDefaultNamespace(type))).orElseThrow();
    }
    public FeatureFlagSet enabledFeatures() {
        throw Unsupported.hit("net.minecraft.server.level.ServerLevel.enabledFeatures");
    }
    private BiomeManager kBiomeManager;
    public BiomeManager getBiomeManager() {
        BiomeManager manager = kBiomeManager;
        if (manager == null) {
            manager = new BiomeManager(new net.minecraft.world.level.biome.BiomeResolver() {
                public Holder<Biome> getNoiseBiome(int qx, int qy, int qz) {
                    return getUncachedNoiseBiome(qx, qy, qz);
                }
            }, BiomeManager.obfuscateSeed(world.getSeed()));
            kBiomeManager = manager;
        }
        return manager;
    }
    public Holder<Biome> getUncachedNoiseBiome(int quartX, int quartY, int quartZ) {
        org.bukkit.block.Biome biome = world.getBiome(quartX << 2, quartY << 2, quartZ << 2);
        if (biome == null) {
            world.getChunkAt(quartX >> 2, quartZ >> 2);
            biome = world.getBiome(quartX << 2, quartY << 2, quartZ << 2);
        }
        var lookup = registryAccess().lookupOrThrow(net.minecraft.core.registries.Registries.BIOME);
        return lookup.get(ResourceKey.create(net.minecraft.core.registries.Registries.BIOME, Identifier.parse(biome.getKey().toString()))).orElseThrow();
    }
    public BlockEntity getBlockEntity(BlockPos blockPos) {
        return getChunk(blockPos.getX() >> 4, blockPos.getZ() >> 4).getBlockEntity(blockPos);
    }
    public BlockState getBlockState(BlockPos blockPos) {
        BlockState found = Block.stateById(WorldNatives.state(world.getName(), blockPos.getX(), blockPos.getY(), blockPos.getZ(), true));
        return found == null ? net.minecraft.world.level.block.Blocks.AIR.defaultBlockState() : found;
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
        return world.getSeaLevel();
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
    public CraftWorld getWorld() {
        return world;
    }
    public WorldBorder getWorldBorder() {
        return WorldBorder.backedBy(world.getWorldBorder());
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
        return getPlayers(player -> true);
    }
    public RegistryAccess registryAccess() {
        return ServerAccess.registries();
    }
    public boolean removeBlock(BlockPos blockPos, boolean flag) {
        return setBlock(blockPos, getFluidState(blockPos).createLegacyBlock(), 3 | (flag ? 64 : 0));
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
        return new StructureManager(this);
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
    public static final BlockPos END_SPAWN_POINT = null;
    public static final IntProvider RAIN_DELAY = null;
    public static final IntProvider RAIN_DURATION = null;
    public static final IntProvider THUNDER_DURATION = null;
    protected final Raids raids = null;
    private net.minecraft.world.level.portal.PortalForcer portalForcer;
    public ServerLevel(MinecraftServer server, Executor executor, LevelStorageSource.LevelStorageAccess levelStorage, ServerLevelData levelData, ResourceKey<Level> dimension, LevelStem levelStem, boolean isDebug, long biomeZoomSeed, List<CustomSpawner> customSpawners, boolean tickTime) {
        super((net.minecraft.world.level.storage.WritableLevelData) null, (net.minecraft.resources.ResourceKey) null, (net.minecraft.core.RegistryAccess) null, (net.minecraft.core.Holder) null, false, false, (long) 0, (int) 0); throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.server.level.ServerLevel#<init>");
    }
    public int getNextEntityId() {
        return org.basaltmc.bukkit.RestNatives.nextEntityId();
    }
    public void setDragonFight(EnderDragonFight fight) {
    }
    public BiomeResolver uncachedBiomeResolver() {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.server.level.ServerLevel#uncachedBiomeResolver");
    }
    public ServerClockManager clockManager() {
        return MinecraftServer.getServer().clockManager();
    }
    private EnvironmentAttributeSystem kEnvironmentAttributes;
    public EnvironmentAttributeSystem environmentAttributes() {
        EnvironmentAttributeSystem system = kEnvironmentAttributes;
        if (system == null) {
            system = EnvironmentAttributeSystem.builder().build();
            kEnvironmentAttributes = system;
        }
        return system;
    }
    public EnvironmentAttributeSystem setEnvironmentAttributes(EnvironmentAttributeSystem environmentAttributes) {
        EnvironmentAttributeSystem previous = environmentAttributes();
        kEnvironmentAttributes = environmentAttributes;
        return previous;
    }
    public void tick(BooleanSupplier haveTime) {
    }
    public boolean shouldTickBlocksAt(long chunkPos) {
        return WorldNatives.chunkLoaded(world.getName(), (int) chunkPos, (int) (chunkPos >> 32));
    }
    protected void tickTime() {
    }
    public void tickCustomSpawners(boolean spawnEnemies) {
    }
    public void tickChunk(LevelChunk chunk, int tickSpeed) {
    }
    public void tickThunder(LevelChunk chunk) {
    }
    public void tickPrecipitation(BlockPos pos) {
    }
    protected BlockPos findLightningTargetAround(BlockPos pos) {
        int[] target = org.basaltmc.bukkit.WorldEnvNatives.lightningTarget(world.getName(), pos.getX(), pos.getZ());
        return target != null && target.length >= 3 ? new BlockPos(target[0], target[1], target[2]) : pos;
    }
    public boolean isHandlingTick() {
        return false;
    }
    public boolean canSleepThroughNights() {
        return true;
    }
    public void updateSleepingPlayerList() {

    }
    public ServerScoreboard getScoreboard() {
        return net.minecraft.server.MinecraftServer.getServer().getScoreboard();
    }
    public ServerWaypointManager getWaypointManager() {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.server.level.ServerLevel#getWaypointManager");
    }
    public float getMoonBrightness(BlockPos pos) {
        return world.getMoonPhase().ordinal() == 4 ? 0.0F : 1.0F;
    }
    public void resetWeatherCycle() {
        world.setStorm(false);
        world.setThundering(false);
    }
    public void resetEmptyTime() {

    }
    public void tickNonPassenger(Entity entity) {
    }
    public void updateNeighboursOnBlockSet(BlockPos pos, BlockState oldState) {
    }
    public boolean mayInteract(Entity entity, BlockPos pos) {
        return !(entity instanceof net.minecraft.world.entity.player.Player) || getWorldBorder().isWithinBounds(pos);
    }
    public void save(ProgressListener progressListener, boolean flush, boolean noSave) {
        WorldNatives.saveWorld(world.getName());
    }
    public <T extends Entity> List<? extends T> getEntities(EntityTypeTest<Entity, T> type, Predicate<? super T> selector) {
        List<T> out = new java.util.ArrayList<>();
        getEntities(type, selector, out, Integer.MAX_VALUE);
        return out;
    }
    public <T extends Entity> void getEntities(EntityTypeTest<Entity, T> type, Predicate<? super T> selector, List<? super T> result) {
        getEntities(type, selector, result, Integer.MAX_VALUE);
    }
    public <T extends Entity> void getEntities(EntityTypeTest<Entity, T> type, Predicate<? super T> selector, List<? super T> result, int maxResults) {
        for (Entity entity : handlesIn(null)) {
            T cast = type.tryCast(entity);
            if (cast != null && selector.test(cast)) {
                result.add(cast);
                if (result.size() >= maxResults) {
                    return;
                }
            }
        }
    }
    public List<? extends EnderDragon> getDragons() {
        java.util.List<EnderDragon> out = new java.util.ArrayList<>();
        for (org.bukkit.entity.EnderDragon each : world.getEntitiesByClass(org.bukkit.entity.EnderDragon.class)) {
            if (each.isValid() && ((org.bukkit.craftbukkit.entity.CraftEntity) each).getHandle() instanceof EnderDragon dragon) {
                out.add(dragon);
            }
        }
        return out;
    }
    public List<ServerPlayer> getPlayers(Predicate<? super ServerPlayer> selector) {
        return getPlayers(selector, Integer.MAX_VALUE);
    }
    public List<ServerPlayer> getPlayers(Predicate<? super ServerPlayer> selector, int maxResults) {
        java.util.List<ServerPlayer> found = new java.util.ArrayList<>();
        for (org.bukkit.entity.Player player : world.getPlayers()) {
            ServerPlayer handle = ((org.bukkit.craftbukkit.entity.CraftPlayer) player).getHandle();
            if (selector.test(handle)) {
                found.add(handle);
                if (found.size() >= maxResults) {
                    break;
                }
            }
        }
        return found;
    }
    public ServerPlayer getRandomPlayer() {
        java.util.List<ServerPlayer> all = getPlayers(player -> true);
        return all.isEmpty() ? null : all.get(random.nextInt(all.size()));
    }
    public boolean addFreshEntity(Entity entity) {
        return entity.spawnIn("DEFAULT");
    }
    public boolean addWithUUID(Entity entity) {
        return addFreshEntity(entity);
    }
    public void addDuringTeleport(Entity entity) {
        addFreshEntity(entity);
    }
    public void addNewPlayer(ServerPlayer player) {
    }
    public void addRespawnedPlayer(ServerPlayer player) {
    }
    public boolean tryAddFreshEntityWithPassengers(Entity entity) {
        return addFreshEntity(entity);
    }
    public void unload(LevelChunk levelChunk) {

    }
    public void removePlayerImmediately(ServerPlayer player, Entity.RemovalReason reason) {
    }
    public void destroyBlockProgress(int id, BlockPos blockPos, int progress) {

    }
    public void playSeededSound(Entity except, double x, double y, double z, Holder<SoundEvent> sound, SoundSource source, float volume, float pitch, long seed) {
        sound.unwrapKey().ifPresent(found -> org.basaltmc.bukkit.WorldEnvNatives.sound(world.getName(),
            found.identifier().toString(), source.name().toLowerCase(java.util.Locale.ROOT), x, y, z, volume, pitch, seed));
    }
    public void playSeededSound(Entity except, Entity sourceEntity, Holder<SoundEvent> sound, SoundSource source, float volume, float pitch, long seed) {
        playSeededSound(except, sourceEntity.getX(), sourceEntity.getY(), sourceEntity.getZ(), sound, source, volume, pitch, seed);
    }
    public void globalLevelEvent(int type, BlockPos pos, int data) {
        org.basaltmc.bukkit.WorldEnvNatives.levelEvent(world.getName(), type, pos.getX(), pos.getY(), pos.getZ(), data, true);
    }
    public void levelEvent(Entity source, int type, BlockPos pos, int data) {
        org.basaltmc.bukkit.WorldEnvNatives.levelEvent(world.getName(), type, pos.getX(), pos.getY(), pos.getZ(), data, false);
    }
    public int getLogicalHeight() {
        return world.getLogicalHeight();
    }
    public void gameEvent(Holder<GameEvent> gameEvent, Vec3 position, GameEvent.Context context) {
        String key = gameEvent.unwrapKey().map(found -> found.identifier().toString()).orElse(null);
        if (key != null) {
            org.basaltmc.bukkit.WorldEnvNatives.gameEvent(world.getName(), key, position.x, position.y, position.z, "");
        }
    }
    public void updateNeighborsAt(BlockPos pos, Block sourceBlock, Orientation orientation) {
        updateNeighborsAt(pos, sourceBlock);
    }
    public void updateNeighborsAtExceptFromFacing(BlockPos pos, Block blockObject, Direction skipDirection, Orientation orientation) {
        java.util.List<BlockPos> targets = new java.util.ArrayList<>(5);
        for (Direction direction : Direction.values()) {
            if (direction != skipDirection) {
                targets.add(pos.relative(direction));
            }
        }
        int[] flat = new int[targets.size() * 3];
        for (int i = 0; i < targets.size(); i++) {
            flat[i * 3] = targets.get(i).getX();
            flat[i * 3 + 1] = targets.get(i).getY();
            flat[i * 3 + 2] = targets.get(i).getZ();
        }
        int[] sources = new int[targets.size()];
        java.util.Arrays.fill(sources, blockObject.defaultBlockState().stateId());
        WorldNatives.neighborChanged(world.getName(), flat, sources);
    }
    public void neighborChanged(BlockPos pos, Block changedBlock, Orientation orientation) {
        Effects.neighborChanged(this, pos, changedBlock == null ? null : changedBlock.defaultBlockState());
    }
    public void neighborChanged(BlockState state, BlockPos pos, Block changedBlock, Orientation orientation, boolean movedByPiston) {
        Effects.neighborChanged(this, pos, changedBlock == null ? null : changedBlock.defaultBlockState());
    }
    public void broadcastEntityEvent(Entity entity, byte event) {
        for (org.bukkit.EntityEffect effect : org.bukkit.EntityEffect.values()) {
            if (effect.getData() == event && entity.live() != null) {
                entity.live().playEffect(effect);
                return;
            }
        }
    }
    public void broadcastDamageEvent(Entity entity, DamageSource source) {
        ClientboundDamageEventPacket packet = new ClientboundDamageEventPacket(entity, source);
        for (org.bukkit.entity.Player each : world.getPlayers()) {
            ((org.bukkit.craftbukkit.entity.CraftPlayer) each).getHandle().connection.send(packet);
        }
    }
    public void explode(Entity source, DamageSource damageSource, ExplosionDamageCalculator damageCalculator, double x, double y, double z, float r, boolean fire, Level.ExplosionInteraction interactionType, ParticleOptions smallExplosionParticles, ParticleOptions largeExplosionParticles, WeightedList<ExplosionParticleInfo> blockParticles, Holder<SoundEvent> explosionSound) {
        emitExplosion(x, y, z, r, fire, interactionType != Level.ExplosionInteraction.NONE);
    }
    public void blockEvent(BlockPos pos, Block block, int b0, int b1) {
        org.basaltmc.bukkit.WorldEnvNatives.blockEvent(world.getName(), pos.getX(), pos.getY(), pos.getZ(), b0, b1);
    }
    public LevelTicks<Block> getBlockTicks() {
        return new LevelTicks<>(world.getName(), false, this::getGameTime);
    }
    public LevelTicks<Fluid> getFluidTicks() {
        return new LevelTicks<>(world.getName(), true, this::getGameTime);
    }
    public PortalForcer getPortalForcer() {
        if (portalForcer == null) {
            portalForcer = new net.minecraft.world.level.portal.PortalForcer(this);
        }
        return portalForcer;
    }
    public <T extends ParticleOptions> int sendParticles(T particle, double x, double y, double z, int count, double xDist, double yDist, double zDist, double speed) {
        org.basaltmc.bukkit.WorldEnvNatives.particle(world.getName(), ((net.minecraft.core.particles.ParticleType<?>) particle.getType()).toString(), x, y, z, count, xDist, yDist, zDist, speed, speed, speed, 0, false, null, null, null);
        return count;
    }
    public <T extends ParticleOptions> int sendParticles(T particle, double x, double y, double z, int count, double xDist, double yDist, double zDist, double speed, ClientboundLevelParticlesPacket.RandomizationType randomizationType) {
        org.basaltmc.bukkit.WorldEnvNatives.particle(world.getName(), ((net.minecraft.core.particles.ParticleType<?>) particle.getType()).toString(), x, y, z, count, xDist, yDist, zDist, speed, speed, speed, 0, false, null, null, null);
        return count;
    }
    public <T extends ParticleOptions> int sendParticles(T particle, double x, double y, double z, int count, double xDist, double yDist, double zDist, double xSpeed, double ySpeed, double zSpeed) {
        org.basaltmc.bukkit.WorldEnvNatives.particle(world.getName(), ((net.minecraft.core.particles.ParticleType<?>) particle.getType()).toString(), x, y, z, count, xDist, yDist, zDist, xSpeed, ySpeed, zSpeed, 0, false, null, null, null);
        return count;
    }
    public <T extends ParticleOptions> int sendParticles(T particle, double x, double y, double z, int count, double xDist, double yDist, double zDist, double xSpeed, double ySpeed, double zSpeed, ClientboundLevelParticlesPacket.RandomizationType randomizationType) {
        org.basaltmc.bukkit.WorldEnvNatives.particle(world.getName(), ((net.minecraft.core.particles.ParticleType<?>) particle.getType()).toString(), x, y, z, count, xDist, yDist, zDist, xSpeed, ySpeed, zSpeed, 0, false, null, null, null);
        return count;
    }
    public <T extends ParticleOptions> int sendParticles(T particle, boolean overrideLimiter, boolean alwaysShow, double x, double y, double z, int count, double xDist, double yDist, double zDist, double speed) {
        org.basaltmc.bukkit.WorldEnvNatives.particle(world.getName(), ((net.minecraft.core.particles.ParticleType<?>) particle.getType()).toString(), x, y, z, count, xDist, yDist, zDist, speed, speed, speed, 0, alwaysShow, null, null, null);
        return count;
    }
    public <T extends ParticleOptions> int sendParticles(T particle, boolean overrideLimiter, boolean alwaysShow, double x, double y, double z, int count, double xDist, double yDist, double zDist, double speed, ClientboundLevelParticlesPacket.RandomizationType randomizationType) {
        org.basaltmc.bukkit.WorldEnvNatives.particle(world.getName(), ((net.minecraft.core.particles.ParticleType<?>) particle.getType()).toString(), x, y, z, count, xDist, yDist, zDist, speed, speed, speed, 0, alwaysShow, null, null, null);
        return count;
    }
    public <T extends ParticleOptions> int sendParticles(T particle, boolean overrideLimiter, boolean alwaysShow, double x, double y, double z, int count, double xDist, double yDist, double zDist, double xSpeed, double ySpeed, double zSpeed, ClientboundLevelParticlesPacket.RandomizationType randomizationType) {
        org.basaltmc.bukkit.WorldEnvNatives.particle(world.getName(), ((net.minecraft.core.particles.ParticleType<?>) particle.getType()).toString(), x, y, z, count, xDist, yDist, zDist, xSpeed, ySpeed, zSpeed, 0, alwaysShow, null, null, null);
        return count;
    }
    public <T extends ParticleOptions> boolean sendParticles(ServerPlayer player, T particle, boolean overrideLimiter, boolean alwaysShow, double x, double y, double z, int count, double xDist, double yDist, double zDist, double speed) {
        org.basaltmc.bukkit.WorldEnvNatives.particle(world.getName(), ((net.minecraft.core.particles.ParticleType<?>) particle.getType()).toString(), x, y, z, count, xDist, yDist, zDist, speed, speed, speed, 0, alwaysShow, null, null, player.getStringUUID());
        return true;
    }
    public Entity getEntity(int id) {
        return super.getEntity(id);
    }
    public Entity getEntityInAnyDimension(UUID uuid) {
        return super.getEntity(uuid);
    }
    public Player getPlayerInAnyDimension(UUID uuid) {
        return super.getEntity(uuid) instanceof Player player ? player : null;
    }
    public Entity getEntityOrPart(int id) {
        return getEntity(id);
    }
    public Collection<EnderDragonPart> dragonParts() {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.server.level.ServerLevel#dragonParts");
    }
    public BlockPos findNearestMapStructure(TagKey<Structure> structureTag, BlockPos origin, int maxSearchRadius, boolean createReference) {
        org.bukkit.Location from = new org.bukkit.Location(world, origin.getX(), origin.getY(), origin.getZ());
        org.bukkit.util.StructureSearchResult best = null;
        double bestDistance = Double.MAX_VALUE;
        for (String member : org.basaltmc.bukkit.item.TagSets.members("structure", structureTag.location().toString())) {
            org.bukkit.generator.structure.Structure structure = org.bukkit.Registry.STRUCTURE.get(org.bukkit.NamespacedKey.fromString(member));
            org.bukkit.util.StructureSearchResult found = structure == null ? null : world.locateNearestStructure(from, structure, maxSearchRadius, !createReference);
            if (found != null && found.getLocation().distanceSquared(from) < bestDistance) {
                best = found;
                bestDistance = found.getLocation().distanceSquared(from);
            }
        }
        return best == null ? null : new BlockPos(best.getLocation().getBlockX(), best.getLocation().getBlockY(), best.getLocation().getBlockZ());
    }
    public BlockPos findNearestMapStructure(HolderSet<Structure> structureTag, BlockPos origin, int maxSearchRadius, boolean createReference) {
        org.bukkit.Location from = new org.bukkit.Location(world, origin.getX(), origin.getY(), origin.getZ());
        org.bukkit.util.StructureSearchResult best = null;
        double bestDistance = Double.MAX_VALUE;
        for (Holder<Structure> holder : structureTag) {
            org.bukkit.generator.structure.Structure structure = holder.unwrapKey().map(key -> org.bukkit.Registry.STRUCTURE.get(org.bukkit.NamespacedKey.fromString(key.identifier().toString()))).orElse(null);
            org.bukkit.util.StructureSearchResult found = structure == null ? null : world.locateNearestStructure(from, structure, maxSearchRadius, !createReference);
            if (found != null && found.getLocation().distanceSquared(from) < bestDistance) {
                best = found;
                bestDistance = found.getLocation().distanceSquared(from);
            }
        }
        return best == null ? null : new BlockPos(best.getLocation().getBlockX(), best.getLocation().getBlockY(), best.getLocation().getBlockZ());
    }
    public Pair<BlockPos, Holder<Biome>> findClosestBiome3d(Predicate<Holder<Biome>> biomeTest, BlockPos origin, int maxSearchRadius, int sampleResolutionHorizontal, int sampleResolutionVertical) {
        var lookup = registryAccess().lookupOrThrow(net.minecraft.core.registries.Registries.BIOME);
        List<org.bukkit.block.Biome> wanted = new java.util.ArrayList<>();
        for (org.bukkit.block.Biome biome : org.bukkit.Registry.BIOME) {
            var holder = lookup.get(ResourceKey.create(net.minecraft.core.registries.Registries.BIOME, net.minecraft.resources.Identifier.parse(biome.getKey().toString())));
            if (holder.isPresent() && biomeTest.test(holder.get())) {
                wanted.add(biome);
            }
        }
        org.bukkit.util.BiomeSearchResult found = wanted.isEmpty() ? null : world.locateNearestBiome(
            new org.bukkit.Location(world, origin.getX(), origin.getY(), origin.getZ()), maxSearchRadius, sampleResolutionHorizontal, sampleResolutionVertical, wanted.toArray(new org.bukkit.block.Biome[0]));
        if (found == null) {
            return null;
        }
        var holder = lookup.get(ResourceKey.create(net.minecraft.core.registries.Registries.BIOME, net.minecraft.resources.Identifier.parse(found.getBiome().getKey().toString())));
        return holder.isEmpty() ? null : Pair.of(new BlockPos(found.getLocation().getBlockX(), found.getLocation().getBlockY(), found.getLocation().getBlockZ()), holder.get());
    }
    public RecipeManager recipeAccess() {
        return kRecipes;
    }
    public TickRateManager tickRateManager() {
        return MinecraftServer.getServer().tickRateManager();
    }
    public boolean noSave() {
        return noSave;
    }
    public MapItemSavedData getMapData(MapId id) {
        return MapItemSavedData.fromNative(id.id());
    }
    public void setMapData(MapId id, MapItemSavedData data) {

    }
    public MapId getFreeMapId() {
        return new net.minecraft.world.level.saveddata.maps.MapId(org.bukkit.Bukkit.createMap(getWorld()).getId());
    }
    public void setRespawnData(LevelData.RespawnData respawnData) {
        world.setSpawnLocation(respawnData.pos().getX(), respawnData.pos().getY(), respawnData.pos().getZ(), respawnData.yaw());
    }
    public LevelData.RespawnData getRespawnData() {
        org.bukkit.Location spawn = world.getSpawnLocation();
        return new LevelData.RespawnData(net.minecraft.core.GlobalPos.of(dimension, new BlockPos(spawn.getBlockX(), spawn.getBlockY(), spawn.getBlockZ())), spawn.getYaw(), spawn.getPitch());
    }
    public LongSet getForceLoadedChunks() {
        it.unimi.dsi.fastutil.longs.LongSet forced = new it.unimi.dsi.fastutil.longs.LongOpenHashSet();
        for (org.bukkit.Chunk chunk : world.getForceLoadedChunks()) {
            forced.add(ChunkPos.pack(chunk.getX(), chunk.getZ()));
        }
        return forced;
    }
    public boolean setChunkForced(int chunkX, int chunkZ, boolean forced) {
        world.setChunkForceLoaded(chunkX, chunkZ, forced);
        return true;
    }
    public PoiManager getPoiManager() {
        if (poiManager == null) {
            poiManager = new org.basaltmc.bukkit.facade.NativePoiManager(world.getName());
        }
        return poiManager;
    }
    private PoiManager poiManager;
    private static final java.util.Set<String> kVillagePois = java.util.Set.of("home", "meeting", "armorer", "butcher",
        "cartographer", "cleric", "farmer", "fisherman", "fletcher", "leatherworker", "librarian", "mason", "shepherd",
        "toolsmith", "weaponsmith");
    public boolean isVillage(BlockPos pos) {
        return isVillage(SectionPos.of(pos));
    }
    public boolean isVillage(SectionPos sectionPos) {
        return sectionsToVillage(sectionPos) <= 2;
    }
    public boolean isCloseToVillage(BlockPos pos, int sectionDistance) {
        return sectionsToVillage(SectionPos.of(pos)) <= sectionDistance;
    }
    public int sectionsToVillage(SectionPos pos) {
        int cx = pos.x() * 16 + 8;
        int cy = pos.y() * 16 + 8;
        int cz = pos.z() * 16 + 8;
        for (int distance = 0; distance <= 6; distance++) {
            String[] rows = org.basaltmc.bukkit.WorldEnvNatives.pointsOfInterest(world.getName(), new String[0], cx, cy, cz,
                distance * 16 + 8, 2);
            for (String row : rows == null ? new String[0] : rows) {
                String type = row.substring(0, row.indexOf('|'));
                if (kVillagePois.contains(type.substring(type.indexOf(':') + 1))) {
                    return distance;
                }
            }
        }
        return 7;
    }
    public Raids getRaids() {
        return new Raids(world);
    }
    public Raid getRaidAt(BlockPos pos) {
        return getRaids().getNearbyRaid(pos, 9216);
    }
    public boolean isRaided(BlockPos pos) {
        return getRaidAt(pos) != null;
    }
    public void onReputationEvent(ReputationEventType type, Entity source, ReputationEventHandler target) {
    }
    public void saveDebugReport(Path rootDir) throws IOException {
    }
    public void clearBlockEvents(BoundingBox bb) {

    }
    public Iterable<Entity> getAllEntities() {
        return handlesIn(null);
    }
    public String toString() {
        return "ServerLevel[" + world.getName() + "]";
    }
    public boolean isFlat() {
        return world.getWorldType() == org.bukkit.WorldType.FLAT;
    }
    public EnderDragonFight getDragonFight() {
        return world.getEnderDragonBattle() == null ? null : new EnderDragonFight(world);
    }
    public WeatherData getWeatherData() {
        return new WeatherData(world);
    }
    public String getWatchdogStats() {
        return "";
    }
    public void addLegacyChunkEntities(Stream<Entity> loaded) {
    }
    public void addWorldGenChunkEntities(Stream<Entity> loaded) {
    }
    public void startTickingChunk(LevelChunk levelChunk) {

    }
    public void onStructureStartsAvailable(ChunkAccess chunk) {

    }
    public PathTypeCache getPathTypeCache() {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.server.level.ServerLevel#getPathTypeCache");
    }
    public void waitForEntities(ChunkPos centerChunk, int radius) {

    }
    public boolean isSpawningMonsters() {
        return world.getAllowMonsters();
    }
    public void close() throws IOException {
        getWorld().save();
    }
    public String gatherChunkSourceStats() {
        return getChunkSource().gatherStats();
    }
    public boolean areEntitiesLoaded(long chunkKey) {
        return WorldNatives.chunkLoaded(world.getName(), ChunkPos.getX(chunkKey), ChunkPos.getZ(chunkKey));
    }
    public boolean isPositionTickingWithEntitiesLoaded(long key) {
        return areEntitiesLoaded(key);
    }
    public boolean isPositionEntityTicking(BlockPos pos) {
        return world.isChunkLoaded(pos.getX() >> 4, pos.getZ() >> 4);
    }
    public boolean areEntitiesActuallyLoadedAndTicking(ChunkPos pos) {
        return WorldNatives.chunkLoaded(world.getName(), pos.x(), pos.z());
    }
    public boolean anyPlayerCloseEnoughForSpawning(BlockPos pos) {
        for (org.bukkit.entity.Player player : world.getPlayers()) {
            double dx = player.getLocation().getX() - (pos.getX() + 0.5);
            double dy = player.getLocation().getY() - (pos.getY() + 0.5);
            double dz = player.getLocation().getZ() - (pos.getZ() + 0.5);
            if (dx * dx + dy * dy + dz * dz < 16384.0) {
                return true;
            }
        }
        return false;
    }
    public boolean anyPlayerCloseEnoughForSpawning(ChunkPos pos) {
        return anyPlayerCloseEnoughForSpawning(new BlockPos(pos.getMiddleBlockX(), 0, pos.getMiddleBlockZ()));
    }
    public boolean canSpreadFireAround(BlockPos pos) {
        int radius = getGameRules().get(net.minecraft.world.level.gamerules.GameRules.FIRE_SPREAD_RADIUS_AROUND_PLAYER);
        if (radius == -1) {
            return true;
        }
        double reach = (double) radius * radius;
        for (org.bukkit.entity.Player each : world.getPlayers()) {
            org.bukkit.Location at = each.getLocation();
            double dx = at.getX() - (pos.getX() + 0.5);
            double dz = at.getZ() - (pos.getZ() + 0.5);
            if (dx * dx + dz * dz < reach) {
                return true;
            }
        }
        return false;
    }
    public boolean canSpawnEntitiesInChunk(ChunkPos pos) {
        return WorldNatives.chunkLoaded(world.getName(), pos.x(), pos.z());
    }
    public GameRules getGameRules() {
        return GameRules.forWorld(world.getName());
    }
    public CrashReportCategory fillReportDetails(CrashReport report) {
        return null;
    }
    public void onBlockEntityAdded(BlockEntity blockEntity) {
    }
    public LevelDebugSynchronizers debugSynchronizers() {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.server.level.ServerLevel#debugSynchronizers");
    }
    public boolean isAllowedToEnterPortal(Level toLevel) {
        return true;
    }
    public boolean isPvpAllowed() {
        return world.getPVP();
    }
    public boolean isCommandBlockEnabled() {
        return true;
    }
    public boolean isSpawnerBlockEnabled() {
        return true;
    }
    final class EntityCallbacks implements LevelCallback<Entity> {
        private EntityCallbacks() {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.server.level.ServerLevel.EntityCallbacks#<init>");
        }
        public void onCreated(Entity entity) {
            
        }
        public void onDestroyed(Entity entity) {
            
        }
        public void onTickingStart(Entity entity) {
            
        }
        public void onTickingEnd(Entity entity) {
            
        }
        public void onTrackingStart(Entity entity) {
            
        }
        public void onTrackingEnd(Entity entity) {
            
        }
        public void onSectionChange(Entity entity) {
            
        }
    }
}
