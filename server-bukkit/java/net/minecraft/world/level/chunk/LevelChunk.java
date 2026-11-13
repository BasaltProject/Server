// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.chunk;

import java.util.Arrays;
import java.util.EnumMap;
import java.util.Map;
import java.util.Set;
import java.util.concurrent.ConcurrentHashMap;
import net.minecraft.core.BlockPos;
import net.minecraft.core.Holder;
import net.minecraft.core.HolderLookup;
import net.minecraft.nbt.CompoundTag;
import net.minecraft.server.level.FullChunkStatus;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.util.debug.DebugValueSource;
import net.minecraft.world.level.ChunkPos;
import net.minecraft.world.level.Level;
import net.minecraft.world.level.LevelHeightAccessor;
import net.minecraft.world.level.biome.Biome;
import net.minecraft.world.level.block.entity.BlockEntity;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.levelgen.Heightmap;
import net.minecraft.world.level.levelgen.blending.BlendingData;
import org.basaltmc.bukkit.WorldNatives;
import org.basaltmc.bukkit.facade.Chunks;
import org.basaltmc.bukkit.facade.NbtCodec;
import org.basaltmc.bukkit.facade.Effects;
import java.util.function.Supplier;
import net.minecraft.network.FriendlyByteBuf;
import net.minecraft.network.protocol.game.ClientboundLevelChunkPacketData;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.level.block.Block;
import net.minecraft.world.level.block.entity.BlockEntityTicker;
import net.minecraft.world.level.block.entity.TickingBlockEntity;
import net.minecraft.world.level.chunk.status.ChunkStatus;
import net.minecraft.world.level.gameevent.GameEventListenerRegistry;
import net.minecraft.world.level.material.Fluid;
import net.minecraft.world.level.material.FluidState;
import net.minecraft.world.ticks.LevelChunkTicks;
import net.minecraft.world.ticks.TickContainerAccess;

public class LevelChunk extends ChunkAccess implements DebugValueSource {
    public Map blockEntities = new java.util.AbstractMap<BlockPos, BlockEntity>() {
        @Override
        public java.util.Set<Map.Entry<BlockPos, BlockEntity>> entrySet() {
            Map<BlockPos, BlockEntity> live = getBlockEntities();
            return live.entrySet();
        }

        @Override
        public BlockEntity get(Object key) {
            return key instanceof BlockPos pos ? getBlockEntity(pos, EntityCreationType.CHECK) : null;
        }
    };
    public Map heightmaps = new EnumMap<Heightmap.Types, Heightmap>(Heightmap.Types.class);
    public int locX = 0;
    public int locZ = 0;
    public boolean mustNotSave = false;

    private final ServerLevel level;
    private final java.util.Map<Integer, GameEventListenerRegistry> sectionListeners = new java.util.concurrent.ConcurrentHashMap<>();
    private UnsavedListener unsavedListener;
    private final String world;
    private final int handle;
    private final int minSection;
    private final LevelChunkSection[] baseline;
    private final long[] hashes;
    private long syncedVersion = -1;
    private volatile long dirtySince;

    protected LevelChunk() {
        super((ChunkPos) null, (UpgradeData) null, (LevelHeightAccessor) null, (PalettedContainerFactory) null, 0L, (LevelChunkSection[]) null, (BlendingData) null);
        this.level = null;
        this.world = null;
        this.handle = -1;
        this.minSection = 0;
        this.baseline = null;
        this.hashes = null;
    }

    public LevelChunk(ServerLevel level, int chunkX, int chunkZ) {
        super(new ChunkPos(chunkX, chunkZ), null, level, null, 0L,
            new LevelChunkSection[level.getSectionsCount()], null);
        this.level = level;
        this.world = level.getWorld().getName();
        this.handle = WorldNatives.worldHandle(world);
        this.minSection = level.getMinY() >> 4;
        this.locX = chunkX;
        this.locZ = chunkZ;
        this.baseline = new LevelChunkSection[sections.length];
        this.hashes = new long[sections.length];
        for (Heightmap.Types type : Heightmap.Types.values()) {
            heightmaps.put(type, new Heightmap(this, level, type));
        }
    }

    private synchronized void refresh() {
        long version = WorldNatives.blockVersion(handle);
        if (version == syncedVersion) {
            return;
        }
        int[] all = Chunks.readAll(world, locX, locZ, minSection, sections.length);
        if (all == null) {
            return;
        }
        PalettedContainerFactory factory = level.palettedContainerFactory();
        int[] biomeCells = null;
        for (int i = 0; i < sections.length; i++) {
            LevelChunkSection held = sections[i];
            if (held != null && (held != baseline[i] || held.statesEdited())) {
                continue;
            }
            long hash = Chunks.hash(all, i << 12);
            if (held != null && hashes[i] == hash) {
                continue;
            }
            if (held == null && biomeCells == null) {
                biomeCells = Chunks.readBiomes(world, locX, locZ, sections.length * 64);
            }
            int uniform = Chunks.uniform(all, i << 12);
            LevelChunkSection built = new LevelChunkSection(factory,
                uniform >= 0 ? null : java.util.Arrays.copyOfRange(all, i << 12, (i + 1) << 12), uniform,
                held != null ? held.getBiomes() : null, biomeCells, i);
            if (held != null) {
                built.inheritBiomeState(held);
            }
            sections[i] = built;
            baseline[i] = built;
            hashes[i] = hash;
        }
        syncedVersion = version;
    }

    /** Only the tick thread writes; other threads leave the sections for the next tick. */
    public boolean flush() {
        if (sections == null) {
            return false;
        }
        if (!org.basaltmc.bukkit.Natives.onPluginThread()) {
            level.getChunkSource().noteHanded(this);
            return false;
        }
        return writeBack();
    }

    /** What building a chunk packet does to the engine's view, without reading the chunk back. */
    public void syncForSend() {
        level.getChunkSource().noteHanded(this);
        flush();
    }

    private synchronized boolean writeBack() {
        int changed = 0;
        boolean anyBiomes = false;
        for (int i = 0; i < sections.length; i++) {
            if (blocksDirty(i)) {
                changed++;
            }
            if (sections[i] != null && sections[i].biomesChanged(baseline[i])) {
                anyBiomes = true;
            }
        }
        if (changed == 0 && !anyBiomes) {
            return false;
        }
        long started = System.nanoTime();
        if (changed > 0) {
            int[] ys = new int[changed];
            int[] states = new int[changed << 12];
            int at = 0;
            for (int i = 0; i < sections.length; i++) {
                if (!blocksDirty(i)) {
                    continue;
                }
                ys[at] = minSection + i;
                int[] ids = Chunks.ids(sections[i]);
                System.arraycopy(ids, 0, states, at << 12, 4096);
                hashes[i] = Chunks.hash(ids, 0);
                at++;
            }
            WorldNatives.applyChunks(world, new int[] {locX, locZ, changed}, ys, states, false, null);
        }
        if (anyBiomes) {
            writeBiomes();
        }
        for (int i = 0; i < sections.length; i++) {
            baseline[i] = sections[i];
            if (sections[i] != null) {
                sections[i].markBiomesSaved();
                sections[i].markStatesSaved();
            }
        }
        syncedVersion = WorldNatives.blockVersion(handle);
        unsaved = true;
        level.getChunkSource().noteChanged(locX, locZ);
        org.basaltmc.bukkit.Natives.recordSection("bukkit.java.writeBack", System.nanoTime() - started);
        return true;
    }

    private boolean blocksDirty(int i) {
        LevelChunkSection section = sections[i];
        return section != null && (section != baseline[i] || section.statesEdited());
    }

    private void writeBiomes() {
        net.minecraft.core.IdMap<Holder<Biome>> map = level.palettedContainerFactory().biomeStrategy().globalMap();
        int[] cells = new int[sections.length * 64];
        java.util.Arrays.fill(cells, -1);
        for (int i = 0; i < sections.length; i++) {
            LevelChunkSection section = sections[i];
            if (section == null || !section.biomesChanged(baseline[i])) {
                continue;
            }
            int[] ids = ((PalettedContainer<Holder<Biome>>) section.getBiomes()).toIds(map::getId);
            System.arraycopy(ids, 0, cells, i * 64, 64);
            section.markBiomesSaved();
        }
        WorldNatives.writeBiomes(world, locX, locZ, cells);
    }

    public Map getBlockEntities() {
        Map<BlockPos, BlockEntity> found = new java.util.HashMap<>();
        if (onTickThread()) {
            flush();
        }
        int[] where = WorldNatives.blockEntityPositions(world, locX, locZ);
        for (int i = 0; i + 2 < where.length; i += 3) {
            BlockPos pos = new BlockPos(where[i], where[i + 1], where[i + 2]);
            BlockEntity view = viewAt(pos);
            if (view != null) {
                found.put(pos, view);
            }
        }
        return found;
    }

    public Set getBlockEntitiesPos() {
        return getBlockEntities().keySet();
    }

    public BlockEntity getBlockEntity(BlockPos pos) {
        return getBlockEntity(pos, EntityCreationType.IMMEDIATE);
    }

    public BlockEntity getBlockEntity(BlockPos pos, LevelChunk.EntityCreationType creationType) {
        if (onTickThread()) {
            flush();
        }
        BlockEntity view = viewAt(pos);
        if (view == null && creationType == EntityCreationType.IMMEDIATE
            && WorldNatives.createBlockEntity(world, pos.getX(), pos.getY(), pos.getZ(), new byte[0])) {
            view = viewAt(pos);
        }
        return view;
    }

    public CompoundTag getBlockEntityNbt(BlockPos pos) {
        return NbtCodec.fromBytes(WorldNatives.blockEntityNbt(world, pos.getX(), pos.getY(), pos.getZ()));
    }

    public CompoundTag getBlockEntityNbtForSaving(BlockPos pos, HolderLookup.Provider registryAccess) {
        return getBlockEntityNbt(pos);
    }

    public void setBlockEntityNbt(CompoundTag tag) {
        int x = tag.getIntOr("x", 0);
        int y = tag.getIntOr("y", 0);
        int z = tag.getIntOr("z", 0);
        byte[] bytes = NbtCodec.toBytes(tag);
        if (!WorldNatives.setBlockEntityNbt(world, x, y, z, bytes)) {
            WorldNatives.createBlockEntity(world, x, y, z, bytes);
        }
    }

    public void setBlockEntity(BlockEntity blockEntity) {
        BlockPos pos = blockEntity.getBlockPos();
        WorldNatives.createBlockEntity(world, pos.getX(), pos.getY(), pos.getZ(), NbtCodec.toBytes(blockEntity.nbt()));
    }

    private BlockEntity viewAt(BlockPos pos) {
        CompoundTag nbt = getBlockEntityNbt(pos);
        return nbt == null ? null : org.basaltmc.bukkit.facade.BlockEntityTypes.view(level, pos, nbt);
    }

    private static boolean onTickThread() {
        return org.basaltmc.bukkit.Natives.onPluginThread();
    }

    public BlockState getBlockState(BlockPos pos) {
        int index = (pos.getY() >> 4) - minSection;
        if (index < 0 || index >= sections.length) {
            return net.minecraft.world.level.block.Blocks.AIR.defaultBlockState();
        }
        synchronized (this) {
            LevelChunkSection section = sections[index];
            if (section != null && (section != baseline[index] || section.statesEdited())) {
                return section.getBlockState(pos.getX() & 15, pos.getY() & 15, pos.getZ() & 15);
            }
        }
        int id = WorldNatives.state(world, pos.getX(), pos.getY(), pos.getZ(), false);
        return id < 0 ? net.minecraft.world.level.block.Blocks.AIR.defaultBlockState()
            : net.minecraft.world.level.block.Block.stateById(id);
    }

    public FullChunkStatus getFullStatus() {
        return switch (WorldNatives.chunkStatus(world, locX, locZ)) {
            case 3 -> FullChunkStatus.ENTITY_TICKING;
            case 2 -> FullChunkStatus.FULL;
            default -> FullChunkStatus.INACCESSIBLE;
        };
    }

    public int getHeight() {
        return level.getHeight();
    }

    public Level getLevel() {
        return level;
    }

    public Holder getNoiseBiome(int x, int y, int z) {
        int index = (y >> 2) - minSection;
        LevelChunkSection section = getSections()[Math.max(0, Math.min(index, sections.length - 1))];
        if (section == null) {
            throw new IllegalStateException("The chunk " + locX + "," + locZ + " of " + world + " is not loaded");
        }
        return section.getNoiseBiome(x & 3, y & 3, z & 3);
    }

    public ChunkPos getPos() {
        return chunkPos;
    }

    public LevelChunkSection getSection(int index) {
        LevelChunkSection section = getSections()[index];
        if (section == null) {
            throw new IllegalStateException("The chunk " + locX + "," + locZ + " of " + world + " is not loaded");
        }
        return section;
    }

    public int getSectionIndex(int y) {
        return (y >> 4) - minSection;
    }

    @Override
    public LevelChunkSection[] getSections() {
        refresh();
        level.getChunkSource().noteHanded(this);
        return sections;
    }

    public void markUnsaved() {
        flush();
        unsaved = true;
        if (unsavedListener != null) {
            unsavedListener.setUnsaved(getPos());
        }
    }

    private void removeGameEventListener(BlockEntity blockEntity, ServerLevel serverLevel) {}

    private void removeBlockEntityTicker(BlockPos pos) {}

    public void removeBlockEntity(BlockPos pos) {
        WorldNatives.removeBlockEntity(world, pos.getX(), pos.getY(), pos.getZ());
    }

    public BlockState setBlockState(BlockPos pos, BlockState state, int flags) {
        BlockState previous = getBlockState(pos);
        if (previous.stateId() == state.stateId()) {
            return null;
        }
        int applied = WorldNatives.applyChanges(world, new int[] {pos.getX(), pos.getY(), pos.getZ()},
            new int[] {state.stateId()}, Effects.ofChunkFlags(flags));
        int index = (pos.getY() >> 4) - minSection;
        synchronized (this) {
            if (index >= 0 && index < sections.length) {
                if (blocksDirty(index)) {
                    sections[index].setBlockState(pos.getX() & 15, pos.getY() & 15, pos.getZ() & 15, state);
                } else {
                    hashes[index] = 0;
                    syncedVersion = -1;
                }
            }
        }
        return applied > 0 ? previous : null;
    }

    @Override
    public void setLightCorrect(boolean flag) {
        flush();
        lightCorrect = flag;
    }

    public boolean hasUnflushed() {
        synchronized (this) {
            for (int i = 0; i < sections.length; i++) {
                if (blocksDirty(i) || (sections[i] != null && sections[i].biomesChanged(baseline[i]))) {
                    return true;
                }
            }
            return false;
        }
    }

    public enum EntityCreationType {
        IMMEDIATE, QUEUED, CHECK;
    }
    public LevelChunk(Level level, ChunkPos pos) {
        this((ServerLevel) level, pos.x(), pos.z());
    }
    public LevelChunk(Level level, ChunkPos pos, UpgradeData upgradeData, LevelChunkTicks<Block> blockTicks, LevelChunkTicks<Fluid> fluidTicks, long inhabitedTime, LevelChunkSection [] sections, PostLoadProcessor postLoad, BlendingData blendingData) {
        this((ServerLevel) level, pos.x(), pos.z());
        setInhabitedTime(inhabitedTime);
    }
    public LevelChunk(ServerLevel level, ProtoChunk protoChunk, PostLoadProcessor postLoad) {
        this(level, protoChunk.getPos().x(), protoChunk.getPos().z());
        if (postLoad != null) {
            postLoad.run(this);
        }
    }
    public void setUnsavedListener(UnsavedListener unsavedListener) {
        this.unsavedListener = unsavedListener;
    }
    public TickContainerAccess<Block> getBlockTicks() {
        return new net.minecraft.world.ticks.NativeTickView<>(world, false, level::getGameTime);
    }
    public TickContainerAccess<Fluid> getFluidTicks() {
        return new net.minecraft.world.ticks.NativeTickView<>(world, true, level::getGameTime);
    }
    public ChunkAccess.PackedTicks getTicksForSerialization(long currentTick) {
        return new ChunkAccess.PackedTicks(java.util.List.of(), java.util.List.of());
    }
    public GameEventListenerRegistry getListenerRegistry(int section) {
        return sectionListeners.computeIfAbsent(section, key -> new net.minecraft.world.level.gameevent.SectionListeners(level));
    }
    public FluidState getFluidState(BlockPos pos) {
        return getBlockState(pos).getFluidState();
    }
    public FluidState getFluidState(int x, int y, int z) {
        return getBlockState(new BlockPos(x, y, z)).getFluidState();
    }
    public void addEntity(Entity entity) {
        return;
    }
    public void addAndRegisterBlockEntity(BlockEntity blockEntity) {
        setBlockEntity(blockEntity);
    }
    public void runPostLoad() {

    }
    public boolean isEmpty() {
        return false;
    }
    public void replaceWithPacketData(int chunkX, int chunkZ, ClientboundLevelChunkPacketData chunkData) {
        FriendlyByteBuf buffer = chunkData.getReadBuffer();
        clearAllBlockEntities();
        for (LevelChunkSection section : sections) {
            section.read(buffer);
        }
        chunkData.getHeightmaps().forEach(this::setHeightmap);
        initializeLightSources();
    }
    public void replaceBiomes(FriendlyByteBuf buffer) {
        for (LevelChunkSection section : sections) {
            section.readBiomes(buffer);
        }
    }
    public void setLoaded(boolean loaded) {

    }
    public void postProcessGeneration(ServerLevel level) {

    }
    public void unpackTicks(long currentTick) {

    }
    public void registerTickContainerInLevel(ServerLevel level) {

    }
    public void unregisterTickContainerFromLevel(ServerLevel level) {

    }
    public void registerDebugValues(ServerLevel level, DebugValueSource.Registration registration) {

    }
    public ChunkStatus getPersistedStatus() {
        return net.minecraft.world.level.chunk.status.ChunkStatus.FULL;
    }
    public void setFullStatus(Supplier<FullChunkStatus> fullStatus) {

    }
    public void clearAllBlockEntities() {
        for (Object pos : new java.util.ArrayList<>(getBlockEntitiesPos())) {
            removeBlockEntity((BlockPos) pos);
        }
    }
    public void registerAllBlockEntitiesAfterLevelLoad() {

    }
    class BoundTickingBlockEntity<T extends BlockEntity> implements TickingBlockEntity {
        private final T blockEntity;
        private final BlockEntityTicker<T> ticker;
        private BoundTickingBlockEntity(T blockEntity, BlockEntityTicker<T> ticker) {
            this.blockEntity = blockEntity;
            this.ticker = ticker;
        }
        public void tick() {
            if (!blockEntity.isRemoved() && blockEntity.hasLevel()) {
                ticker.tick(level, blockEntity.getBlockPos(), blockEntity.getBlockState(), blockEntity);
            }
        }
        public boolean isRemoved() {
            return blockEntity.isRemoved();
        }
        public BlockPos getPos() {
            return blockEntity.getBlockPos();
        }
        public String getType() {
            return String.valueOf(blockEntity.getType());
        }
        public String toString() {
            return super.toString();
        }
    }
    static class RebindableTickingBlockEntityWrapper implements TickingBlockEntity {
        private TickingBlockEntity ticker;
        private RebindableTickingBlockEntityWrapper(TickingBlockEntity ticker) {
            this.ticker = ticker;
        }
        void rebind(TickingBlockEntity ticker) {
            this.ticker = ticker;
        }
        public void tick() {
            ticker.tick();
        }
        public boolean isRemoved() {
            return ticker.isRemoved();
        }
        public BlockPos getPos() {
            return ticker.getPos();
        }
        public String getType() {
            return ticker.getType();
        }
        public String toString() {
            return super.toString();
        }
    }
    public interface PostLoadProcessor {
        void run(LevelChunk levelChunk);
    }
    public interface UnsavedListener {
        void setUnsaved(ChunkPos chunkPos);
    }
}
