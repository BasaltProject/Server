// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level;

import ca.spottedleaf.moonrise.patches.chunk_system.level.entity.EntityLookup;
import net.minecraft.core.BlockPos;
import net.minecraft.world.level.block.Block;
import net.minecraft.world.level.block.entity.BlockEntity;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.chunk.ChunkSource;
import net.minecraft.world.level.chunk.LevelChunk;
import org.basaltmc.bukkit.Unsupported;
import org.bukkit.craftbukkit.CraftServer;
import org.bukkit.craftbukkit.CraftWorld;
import com.mojang.serialization.Codec;
import java.io.IOException;
import java.util.Collection;
import java.util.List;
import java.util.UUID;
import java.util.function.Consumer;
import java.util.function.Predicate;
import net.minecraft.CrashReport;
import net.minecraft.CrashReportCategory;
import net.minecraft.core.Direction;
import net.minecraft.core.Holder;
import net.minecraft.core.RegistryAccess;
import net.minecraft.core.particles.ExplosionParticleInfo;
import net.minecraft.core.particles.ParticleOptions;
import net.minecraft.network.protocol.Packet;
import net.minecraft.resources.ResourceKey;
import net.minecraft.server.MinecraftServer;
import net.minecraft.sounds.SoundEvent;
import net.minecraft.sounds.SoundSource;
import net.minecraft.util.RandomSource;
import net.minecraft.util.StringRepresentable;
import net.minecraft.util.random.WeightedList;
import net.minecraft.world.TickRateManager;
import net.minecraft.world.attribute.EnvironmentAttributeSystem;
import net.minecraft.world.clock.ClockManager;
import net.minecraft.world.damagesource.DamageSource;
import net.minecraft.world.damagesource.DamageSources;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.entity.boss.enderdragon.EnderDragonPart;
import net.minecraft.world.entity.player.Player;
import net.minecraft.world.item.component.FireworkExplosion;
import net.minecraft.world.item.crafting.RecipeAccess;
import net.minecraft.world.level.biome.Biome;
import net.minecraft.world.level.biome.BiomeManager;
import net.minecraft.world.level.block.entity.TickingBlockEntity;
import net.minecraft.world.level.chunk.ChunkAccess;
import net.minecraft.world.level.chunk.PalettedContainerFactory;
import net.minecraft.world.level.chunk.status.ChunkStatus;
import net.minecraft.world.level.dimension.DimensionType;
import net.minecraft.world.level.entity.EntityTypeTest;
import net.minecraft.world.level.entity.LevelEntityGetter;
import net.minecraft.world.level.levelgen.Heightmap;
import net.minecraft.world.level.lighting.LevelLightEngine;
import net.minecraft.world.level.material.FluidState;
import net.minecraft.world.level.redstone.CollectingNeighborUpdater;
import net.minecraft.world.level.redstone.Orientation;
import net.minecraft.world.level.saveddata.maps.MapId;
import net.minecraft.world.level.saveddata.maps.MapItemSavedData;
import net.minecraft.world.level.storage.LevelData;
import net.minecraft.world.level.storage.WritableLevelData;
import net.minecraft.world.phys.AABB;
import net.minecraft.world.phys.Vec3;
import net.minecraft.world.scores.Scoreboard;
public abstract class Level implements LevelAccessor, AutoCloseable {
    private static final java.util.concurrent.atomic.AtomicInteger kNextEntityId = new java.util.concurrent.atomic.AtomicInteger(1 << 30);

    protected List<Entity> handlesIn(AABB bb) {
        List<Entity> out = new java.util.ArrayList<>();
        java.util.Collection<org.bukkit.entity.Entity> found = bb == null ? getWorld().getEntities()
            : getWorld().getNearbyEntities(new org.bukkit.util.BoundingBox(bb.minX, bb.minY, bb.minZ, bb.maxX, bb.maxY, bb.maxZ));
        for (org.bukkit.entity.Entity entity : found) {
            if (entity instanceof org.bukkit.craftbukkit.entity.CraftEntity craft) {
                out.add(craft.getHandle());
            }
        }
        return out;
    }

    private final RandomSource kRandom = RandomSource.create();
    private final java.util.concurrent.atomic.AtomicLong kSubTickCount = new java.util.concurrent.atomic.AtomicLong();

    protected Level() {
        this.blockEntityTickers = new java.util.ArrayList<>();
        this.neighborUpdater = null;
        this.random = RandomSource.create();
        this.levelData = null;
    }

    protected void emitSound(double x, double y, double z, String key, SoundSource source, float volume, float pitch) {
    }

    protected void emitExplosion(double x, double y, double z, float power, boolean fire, boolean breakBlocks) {
    }
    public BlockEntity getBlockEntity(BlockPos pos) {
        throw Unsupported.hit("net.minecraft.world.level.Level.getBlockEntity");
    }
    public BlockState getBlockState(BlockPos pos) {
        throw Unsupported.hit("net.minecraft.world.level.Level.getBlockState");
    }
    public LevelChunk getChunk(int chunkX, int chunkZ) {
        throw Unsupported.hit("net.minecraft.world.level.Level.getChunk");
    }
    public ChunkSource getChunkSource() {
        throw Unsupported.hit("net.minecraft.world.level.Level.getChunkSource");
    }
    public CraftServer getCraftServer() {
        throw Unsupported.hit("net.minecraft.world.level.Level.getCraftServer");
    }
    public int getSectionIndex(int value) {
        return (value >> 4) - (getMinY() >> 4);
    }
    public CraftWorld getWorld() {
        throw Unsupported.hit("net.minecraft.world.level.Level.getWorld");
    }
    public boolean isClientSide() {
        return false;
    }
    public EntityLookup moonrise$getEntityLookup() {
        throw Unsupported.hit("net.minecraft.world.level.Level.moonrise$getEntityLookup");
    }
    public void sendBlockUpdated(BlockPos blockPos, BlockState blockState, BlockState blockState2, int value) {
        throw Unsupported.hit("net.minecraft.world.level.Level.sendBlockUpdated");
    }
    public void updateNeighborsAt(BlockPos blockPos, Block block) {
        throw Unsupported.hit("net.minecraft.world.level.Level.updateNeighborsAt");
    }
    public void updateNeighbourForOutputSignal(BlockPos pos, Block changedBlock) {
        throw Unsupported.hit("net.minecraft.world.level.Level.updateNeighbourForOutputSignal");
    }
    public void updatePOIOnBlockStateChange(BlockPos pos, BlockState oldState, BlockState newState) {
        throw Unsupported.hit("net.minecraft.world.level.Level.updatePOIOnBlockStateChange");
    }
    public static final Codec<ResourceKey<Level>> RESOURCE_KEY_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.level.Level.class, "RESOURCE_KEY_CODEC");
    public static final ResourceKey<Level> OVERWORLD = ResourceKey.create(net.minecraft.core.registries.Registries.DIMENSION, net.minecraft.resources.Identifier.withDefaultNamespace("overworld"));
    public static final ResourceKey<Level> NETHER = ResourceKey.create(net.minecraft.core.registries.Registries.DIMENSION, net.minecraft.resources.Identifier.withDefaultNamespace("the_nether"));
    public static final ResourceKey<Level> END = ResourceKey.create(net.minecraft.core.registries.Registries.DIMENSION, net.minecraft.resources.Identifier.withDefaultNamespace("the_end"));
    public static final int MAX_LEVEL_SIZE = 30000000;
    public static final int ACROSS_THE_WHOLE_WORLD = 60000000;
    public static final int LONG_PARTICLE_CLIP_RANGE = 512;
    public static final int SHORT_PARTICLE_CLIP_RANGE = 32;
    public static final int MAX_BRIGHTNESS = 15;
    public static final int MAX_ENTITY_SPAWN_Y = 20000000;
    public static final int MIN_ENTITY_SPAWN_Y = -20000000;
    protected final List<TickingBlockEntity> blockEntityTickers;
    protected final CollectingNeighborUpdater neighborUpdater;
    protected int randValue;
    protected final int addend = 0;
    protected float oRainLevel;
    protected float rainLevel;
    protected float oThunderLevel;
    protected float thunderLevel;
    protected final RandomSource random;
    protected final WritableLevelData levelData;
    protected Level(WritableLevelData levelData, ResourceKey<Level> dimension, RegistryAccess registryAccess, Holder<DimensionType> dimensionTypeRegistration, boolean isClientSide, boolean isDebug, long biomeZoomSeed, int maxChainedNeighborUpdates) {
        this.blockEntityTickers = new java.util.ArrayList<>();
        this.neighborUpdater = null;
        this.random = RandomSource.create();
        this.levelData = levelData;
    }
    public int getNextEntityId() {
        return kNextEntityId.incrementAndGet();
    }
    public abstract MinecraftServer getServer();
    public boolean isInWorldBounds(BlockPos pos) {
        return isInsideBuildHeight(pos) && pos.getX() >= -30000000 && pos.getZ() >= -30000000
            && pos.getX() < 30000000 && pos.getZ() < 30000000;
    }
    public boolean isInValidBounds(BlockPos pos) {
        return isInsideBuildHeight(pos) && ChunkPos.isValid(pos.getX() >> 4, pos.getZ() >> 4);
    }
    public static boolean isInSpawnableBounds(BlockPos pos) {
        return pos.getY() >= -20000000 && pos.getY() < 20000000 && pos.getX() >= -30000000
            && pos.getZ() >= -30000000 && pos.getX() < 30000000 && pos.getZ() < 30000000;
    }
    public LevelChunk getChunkAt(BlockPos pos) {
        return getChunk(pos.getX() >> 4, pos.getZ() >> 4);
    }
    public abstract ChunkAccess getChunk(int chunkX, int chunkZ, ChunkStatus status, boolean loadOrGenerate);
    public abstract boolean setBlock(BlockPos pos, BlockState blockState, int updateFlags, int updateLimit);
    public abstract boolean removeBlock(BlockPos pos, boolean movedByPiston);
    public abstract boolean destroyBlock(BlockPos pos, boolean dropResources, Entity breaker, int updateLimit);
    public void addDestroyBlockEffect(BlockPos pos, BlockState blockState) {
        return;
    }
    public void setBlocksDirty(BlockPos pos, BlockState oldState, BlockState newState) {
        return;
    }
    public abstract void updateNeighborsAt(BlockPos pos, Block sourceBlock, Orientation orientation);
    public abstract void updateNeighborsAtExceptFromFacing(BlockPos pos, Block blockObject, Direction skipDirection, Orientation orientation);
    public abstract void neighborChanged(BlockPos pos, Block changedBlock, Orientation orientation);
    public abstract void neighborChanged(BlockState state, BlockPos pos, Block changedBlock, Orientation orientation, boolean movedByPiston);
    public void neighborShapeChanged(Direction direction, BlockPos pos, BlockPos neighborPos, BlockState neighborState, int updateFlags, int updateLimit) {
        return;
    }
    public int getHeight(Heightmap.Types type, int x, int z) {
        return LevelAccessor.super.getHeight(type, x, z);
    }
    public abstract LevelLightEngine getLightEngine();
    public FluidState getFluidState(BlockPos pos) {
        return getBlockState(pos).getFluidState();
    }
    public boolean isBrightOutside() {
        return getSkyDarken() < 4;
    }
    public boolean isDarkOutside() {
        return !isBrightOutside();
    }
    public void playSound(Entity except, BlockPos pos, SoundEvent sound, SoundSource source, float volume, float pitch) {
        emitSound(pos.getX() + 0.5, pos.getY() + 0.5, pos.getZ() + 0.5, sound.location().toString(), source, volume, pitch);
    }
    public void playSeededSound(Entity except, double x, double y, double z, Holder<SoundEvent> sound, SoundSource source, float volume, float pitch, long seed) {
        emitSound(x, y, z, sound.value().location().toString(), source, volume, pitch);
    }
    public void playSeededSound(Entity except, double x, double y, double z, SoundEvent sound, SoundSource source, float volume, float pitch, long seed) {
        emitSound(x, y, z, sound.location().toString(), source, volume, pitch);
    }
    public void playSeededSound(Entity except, Entity sourceEntity, Holder<SoundEvent> sound, SoundSource source, float volume, float pitch, long seed) {
        emitSound(sourceEntity.getX(), sourceEntity.getY(), sourceEntity.getZ(), sound.value().location().toString(), source, volume, pitch);
    }
    public void playSound(Entity except, double x, double y, double z, SoundEvent sound, SoundSource source) {
        emitSound(x, y, z, sound.location().toString(), source, 1.0F, 1.0F);
    }
    public void playSound(Entity except, double x, double y, double z, SoundEvent sound, SoundSource source, float volume, float pitch) {
        emitSound(x, y, z, sound.location().toString(), source, volume, pitch);
    }
    public void playSound(Entity except, double x, double y, double z, Holder<SoundEvent> sound, SoundSource source, float volume, float pitch) {
        emitSound(x, y, z, sound.value().location().toString(), source, volume, pitch);
    }
    public void playSound(Entity except, Entity sourceEntity, SoundEvent sound, SoundSource source, float volume, float pitch) {
        emitSound(sourceEntity.getX(), sourceEntity.getY(), sourceEntity.getZ(), sound.location().toString(), source, volume, pitch);
    }
    public void playSound(Entity except, Entity sourceEntity, Holder<SoundEvent> sound, SoundSource source, float volume, float pitch) {
        emitSound(sourceEntity.getX(), sourceEntity.getY(), sourceEntity.getZ(), sound.value().location().toString(), source, volume, pitch);
    }
    public void playLocalSound(BlockPos pos, SoundEvent sound, SoundSource source, float volume, float pitch, boolean distanceDelay) {
    }
    public void playLocalSound(Entity sourceEntity, SoundEvent sound, SoundSource source, float volume, float pitch) {
    }
    public void playLocalSound(double x, double y, double z, SoundEvent sound, SoundSource source, float volume, float pitch, boolean distanceDelay) {
    }
    public void playPlayerSound(SoundEvent sound, SoundSource source, float volume, float pitch) {
    }
    public void addParticle(ParticleOptions particle, double x, double y, double z, double xd, double yd, double zd) {

    }
    public void addParticle(ParticleOptions particle, boolean overrideLimiter, boolean alwaysShow, double x, double y, double z, double xd, double yd, double zd) {

    }
    public void addAlwaysVisibleParticle(ParticleOptions particle, double x, double y, double z, double xd, double yd, double zd) {

    }
    public void addAlwaysVisibleParticle(ParticleOptions particle, boolean overrideLimiter, double x, double y, double z, double xd, double yd, double zd) {

    }
    public void addBlockEntityTicker(TickingBlockEntity ticker) {
        return;
    }
    public void tickBlockEntities() {
        return;
    }
    public <T extends Entity> void guardEntityTick(Consumer<T> tick, T entity) {
        tick.accept(entity);
    }
    public boolean shouldTickDeath(Entity entity) {
        return true;
    }
    public abstract boolean shouldTickBlocksAt(long chunkPos);
    public abstract boolean shouldTickBlocksAt(BlockPos pos);
    public void explode(Entity source, double x, double y, double z, float r, ExplosionInteraction blockInteraction) {
        emitExplosion(x, y, z, r, false, blockInteraction != ExplosionInteraction.NONE);
    }
    public void explode(Entity source, double x, double y, double z, float r, boolean fire, ExplosionInteraction blockInteraction) {
        emitExplosion(x, y, z, r, fire, blockInteraction != ExplosionInteraction.NONE);
    }
    public void explode(Entity source, DamageSource damageSource, ExplosionDamageCalculator damageCalculator, Vec3 boomPos, float r, boolean fire, ExplosionInteraction blockInteraction) {
        emitExplosion(boomPos.x, boomPos.y, boomPos.z, r, fire, blockInteraction != ExplosionInteraction.NONE);
    }
    public void explode(Entity source, DamageSource damageSource, ExplosionDamageCalculator damageCalculator, double x, double y, double z, float r, boolean fire, ExplosionInteraction interactionType) {
        emitExplosion(x, y, z, r, fire, interactionType != ExplosionInteraction.NONE);
    }
    public void explode(Entity source, DamageSource damageSource, ExplosionDamageCalculator damageCalculator, double x, double y, double z, float r, boolean fire, ExplosionInteraction interactionType, ParticleOptions smallExplosionParticles, ParticleOptions largeExplosionParticles, WeightedList<ExplosionParticleInfo> blockParticles, Holder<SoundEvent> explosionSound) {
        emitExplosion(x, y, z, r, fire, interactionType != ExplosionInteraction.NONE);
    }
    public abstract String gatherChunkSourceStats();
    public void setBlockEntity(BlockEntity blockEntity) {
        getChunkAt(blockEntity.getBlockPos()).setBlockEntity(blockEntity);
    }
    public abstract void removeBlockEntity(BlockPos pos);
    public boolean isLoaded(BlockPos pos) {
        return getChunkSource().getChunk(pos.getX() >> 4, pos.getZ() >> 4, false) != null;
    }
    public boolean loadedAndEntityCanStandOnFace(BlockPos pos, Entity entity, Direction faceDirection) {
        return isLoaded(pos) && getBlockState(pos).isFaceSturdy(this, pos, faceDirection);
    }
    public boolean loadedAndEntityCanStandOn(BlockPos pos, Entity entity) {
        return isLoaded(pos) && getBlockState(pos).isFaceSturdy(this, pos, Direction.UP);
    }
    public void updateSkyBrightness() {
        return;
    }
    public void setSpawnSettings(boolean spawnEnemies) {
        return;
    }
    public abstract void setRespawnData(LevelData.RespawnData respawnData);
    public abstract LevelData.RespawnData getRespawnData();
    public LevelData.RespawnData getWorldBorderAdjustedRespawnData(LevelData.RespawnData respawnData) {
        return respawnData;
    }
    public abstract void close() throws IOException;
    public BlockGetter getChunkForCollisions(int chunkX, int chunkZ) {
        return getChunkSource().getChunk(chunkX, chunkZ, false);
    }
    public List<Entity> getEntities(Entity except, AABB bb, Predicate<? super Entity> selector) {
        List<Entity> out = new java.util.ArrayList<>();
        for (Entity entity : handlesIn(bb)) {
            if (entity != except && entity.getUUID() != null && !entity.getUUID().equals(except == null ? null : except.getUUID())
                && selector.test(entity)) {
                out.add(entity);
            }
        }
        return out;
    }
    public <T extends Entity> List<T> getEntities(EntityTypeTest<Entity, T> type, AABB bb, Predicate<? super T> selector) {
        List<T> out = new java.util.ArrayList<>();
        getEntities(type, bb, selector, out);
        return out;
    }
    public <T extends Entity> void getEntities(EntityTypeTest<Entity, T> type, AABB bb, Predicate<? super T> selector, List<? super T> output) {
        getEntities(type, bb, selector, output, Integer.MAX_VALUE);
    }
    public <T extends Entity> void getEntities(EntityTypeTest<Entity, T> type, AABB bb, Predicate<? super T> selector, List<? super T> output, int maxResults) {
        for (Entity entity : handlesIn(bb)) {
            T cast = type.tryCast(entity);
            if (cast != null && selector.test(cast)) {
                output.add(cast);
                if (output.size() >= maxResults) {
                    return;
                }
            }
        }
    }
    public <T extends Entity> boolean hasEntities(EntityTypeTest<Entity, T> type, AABB bb, Predicate<? super T> selector) {
        for (Entity entity : handlesIn(bb)) {
            T cast = type.tryCast(entity);
            if (cast != null && selector.test(cast)) {
                return true;
            }
        }
        return false;
    }
    public List<Entity> getPushableEntities(Entity pusher, AABB boundingBox) {
        return getEntities(pusher, boundingBox, entity -> entity.isPushable());
    }
    public Entity getEntity(int id) {
        org.bukkit.entity.Entity found = org.basaltmc.bukkit.EntityBridge.of(getWorld(), id);
        return found == null ? null : ((org.bukkit.craftbukkit.entity.CraftEntity) found).getHandle();
    }
    public Entity getEntity(UUID uuid) {
        org.bukkit.entity.Entity found = getWorld().getEntity(uuid);
        return found == null ? null : ((org.bukkit.craftbukkit.entity.CraftEntity) found).getHandle();
    }
    public Entity getEntityInAnyDimension(UUID uuid) {
        return getEntity(uuid);
    }
    public Player getPlayerInAnyDimension(UUID uuid) {
        return getEntity(uuid) instanceof Player player ? player : null;
    }
    public Collection<EnderDragonPart> dragonParts() {
        return List.of();
    }
    public void blockEntityChanged(BlockPos pos) {
        return;
    }
    public abstract void onBlockEntityAdded(BlockEntity blockEntity);
    public long getOverworldClockTime() {
        return getGameTime();
    }
    public long getDefaultClockTime() {
        return getGameTime();
    }
    public boolean mayInteract(Entity entity, BlockPos pos) {
        return true;
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
        for (org.bukkit.entity.Player player : getWorld().getPlayers()) {
            if (player.getLocation().distanceSquared(((org.bukkit.entity.Entity) entity.getBukkitEntity()).getLocation()) < 128.0 * 128.0
                && player instanceof org.bukkit.craftbukkit.entity.CraftPlayer crafted) {
                crafted.getHandle().connection.send(new net.minecraft.network.protocol.game.ClientboundDamageEventPacket(entity, source));
            }
        }
    }
    public void blockEvent(BlockPos pos, Block block, int b0, int b1) {
        return;
    }
    public abstract LevelData getLevelData();
    public abstract TickRateManager tickRateManager();
    public float getRelativeTickSpeed() {
        return 1.0F;
    }
    public float getThunderLevel(float a) {
        return thunderLevel * getRainLevel(a);
    }
    public void setThunderLevel(float thunderLevel) {
        thunderLevel = Math.max(0.0F, Math.min(1.0F, thunderLevel));
    }
    public float getRainLevel(float a) {
        return rainLevel;
    }
    public void setRainLevel(float rainLevel) {
        this.rainLevel = Math.max(0.0F, Math.min(1.0F, rainLevel));
    }
    public boolean canHaveWeather() {
        return true;
    }
    public boolean isThundering() {
        return canHaveWeather() && getThunderLevel(1.0F) > 0.9;
    }
    public boolean isRaining() {
        return canHaveWeather() && getRainLevel(1.0F) > 0.2;
    }
    public boolean isRainingAt(BlockPos pos) {
        return isRaining() && canSeeSky(pos) && precipitationAt(pos) == Biome.Precipitation.RAIN;
    }
    public Biome.Precipitation precipitationAt(BlockPos pos) {
        if (!getWorld().hasStorm()) {
            return Biome.Precipitation.NONE;
        }
        return getBiome(pos).value().getPrecipitationAt(pos, getSeaLevel());
    }
    public MapItemSavedData getMapData(MapId id) {
        return net.minecraft.world.level.saveddata.maps.MapItemSavedData.fromNative(id.id());
    }
    public void globalLevelEvent(int type, BlockPos pos, int data) {

    }
    public abstract CrashReportCategory fillReportDetails(CrashReport report);
    public void destroyBlockProgress(int id, BlockPos blockPos, int progress) {

    }
    public void createFireworks(double x, double y, double z, double xd, double yd, double zd, List<FireworkExplosion> explosions, boolean playSound) {
        return;
    }
    public Scoreboard getScoreboard() {
        return MinecraftServer.getServer().getScoreboard();
    }
    public int getSkyDarken() {
        double rain = 1.0 - getRainLevel(1.0F) * 5.0F / 16.0;
        double thunder = 1.0 - getThunderLevel(1.0F) * 5.0F / 16.0;
        double phase = getWorld().getFullTime() / 24000.0 - 0.25;
        phase -= Math.floor(phase);
        double curved = 0.5 - Math.cos(phase * Math.PI) / 2.0;
        double sunAngle = (phase * 2.0 + curved) / 3.0 * 2.0 * Math.PI;
        double sun = 0.5 + 2.0 * Math.clamp(Math.cos(sunAngle), -0.25, 0.25);
        return (int) ((1.0 - sun * rain * thunder) * 11.0);
    }
    public void setSkyFlashTime(int skyFlashTime) {
        return;
    }
    public void sendPacketToServer(Packet<?> packet) {
        return;
    }
    public DimensionType dimensionType() {
        return dimensionTypeRegistration().value();
    }
    public abstract Holder<DimensionType> dimensionTypeRegistration();
    public abstract ResourceKey<Level> dimension();
    public RandomSource getRandom() {
        return kRandom;
    }
    public boolean isStateAtPosition(BlockPos pos, Predicate<BlockState> predicate) {
        return predicate.test(getBlockState(pos));
    }
    public boolean isFluidAtPosition(BlockPos pos, Predicate<FluidState> predicate) {
        return predicate.test(getFluidState(pos));
    }
    public RecipeAccess recipeAccess() {
        return null;
    }
    public BlockPos getBlockRandomPos(int xo, int yo, int zo, int yMask) {
        randValue = randValue * 3 + 1013904223;
        int packed = randValue >> 2;
        return new BlockPos(xo + (packed & 15), yo + (packed >> 16 & yMask), zo + (packed >> 8 & 15));
    }
    public boolean noSave() {
        return false;
    }
    public BiomeManager getBiomeManager() {
        throw Unsupported.hit("net.minecraft.world.level.Level.getBiomeManager");
    }
    public boolean isDebug() {
        return false;
    }
    protected abstract LevelEntityGetter<Entity> getEntities();
    public long nextSubTickCount() {
        return kSubTickCount.getAndIncrement();
    }
    public abstract RegistryAccess registryAccess();
    private final DamageSources damageSources = new DamageSources(null);
    public DamageSources damageSources() {
        return damageSources;
    }
    public abstract ClockManager clockManager();
    public abstract EnvironmentAttributeSystem environmentAttributes();
    public int getClientLeafTintColor(BlockPos pos) {
        return 0xFFFFFF;
    }
    public abstract PalettedContainerFactory palettedContainerFactory();
    public static enum ExplosionInteraction implements StringRepresentable {
        NONE, BLOCK, MOB, TNT, TRIGGER;
        public static final Codec<ExplosionInteraction> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.level.Level.ExplosionInteraction.class, "CODEC");
        public String getSerializedName() {
            return name().toLowerCase(java.util.Locale.ROOT);
        }
    }
}
