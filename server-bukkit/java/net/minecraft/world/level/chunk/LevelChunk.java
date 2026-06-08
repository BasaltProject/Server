package net.minecraft.world.level.chunk;

import java.util.Arrays;
import java.util.EnumMap;
import java.util.Map;
import java.util.concurrent.ConcurrentHashMap;
import net.minecraft.core.BlockPos;
import net.minecraft.core.Holder;
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

public class LevelChunk extends ChunkAccess implements DebugValueSource {
    public Map blockEntities = new ConcurrentHashMap<BlockPos, BlockEntity>();
    public Map heightmaps = new EnumMap<Heightmap.Types, Heightmap>(Heightmap.Types.class);
    public int locX = 0;
    public int locZ = 0;
    public boolean mustNotSave = false;

    private final ServerLevel level;
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
        super(new ChunkPos(chunkX, chunkZ), null, null, null, 0L,
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
            if (held != null && held != baseline[i]) {
                continue;
            }
            long hash = Chunks.hash(all, i << 12);
            if (held != null && hashes[i] == hash) {
                continue;
            }
            if (biomeCells == null && held == null) {
                biomeCells = Chunks.readBiomes(world, locX, locZ, sections.length * 64);
            }
            PalettedContainerRO<Holder<Biome>> biomes = held != null ? held.getBiomes()
                : Chunks.biomes(factory, biomeCells, i);
            LevelChunkSection built = new LevelChunkSection(
                Chunks.states(factory, all, i << 12), biomes);
            sections[i] = built;
            baseline[i] = built;
            hashes[i] = hash;
        }
        syncedVersion = version;
    }

    /** Writes every section a plugin replaced back to the world in one call. */
    public synchronized boolean flush() {
        if (sections == null) {
            return false;
        }
        int changed = 0;
        for (int i = 0; i < sections.length; i++) {
            if (sections[i] != baseline[i]) {
                changed++;
            }
        }
        if (changed == 0) {
            return false;
        }
        int[] ys = new int[changed];
        int[] states = new int[changed << 12];
        int at = 0;
        for (int i = 0; i < sections.length; i++) {
            if (sections[i] == baseline[i]) {
                continue;
            }
            ys[at] = minSection + i;
            int[] ids = Chunks.ids(sections[i]);
            System.arraycopy(ids, 0, states, at << 12, 4096);
            hashes[i] = Chunks.hash(ids, 0);
            baseline[i] = sections[i];
            at++;
        }
        WorldNatives.applyChunks(world, new int[] {locX, locZ, changed}, ys, states, false, null);
        syncedVersion = WorldNatives.blockVersion(handle);
        unsaved = true;
        level.getChunkSource().noteChanged(locX, locZ);
        return true;
    }

    public Map getBlockEntities() {
        return blockEntities;
    }

    public BlockEntity getBlockEntity(BlockPos pos) {
        return null;
    }

    public BlockEntity getBlockEntity(BlockPos pos, LevelChunk.EntityCreationType creationType) {
        return null;
    }

    public BlockState getBlockState(BlockPos pos) {
        int index = (pos.getY() >> 4) - minSection;
        if (index < 0 || index >= sections.length) {
            return net.minecraft.world.level.block.Blocks.AIR.defaultBlockState();
        }
        synchronized (this) {
            LevelChunkSection section = sections[index];
            if (section != null && section != baseline[index]) {
                return section.getBlockState(pos.getX() & 15, pos.getY() & 15, pos.getZ() & 15);
            }
        }
        int id = WorldNatives.state(world, pos.getX(), pos.getY(), pos.getZ(), false);
        return id < 0 ? net.minecraft.world.level.block.Blocks.AIR.defaultBlockState()
            : net.minecraft.world.level.block.Block.stateById(id);
    }

    public FullChunkStatus getFullStatus() {
        return WorldNatives.chunkLoaded(world, locX, locZ) ? FullChunkStatus.BLOCK_TICKING : FullChunkStatus.INACCESSIBLE;
    }

    public int getHeight() {
        return level.getHeight();
    }

    public Level getLevel() {
        return level;
    }

    public Holder getNoiseBiome(int x, int y, int z) {
        int index = (y >> 2 >> 2) - minSection;
        LevelChunkSection section = getSections()[Math.max(0, Math.min(index, sections.length - 1))];
        return section.getNoiseBiome(x & 3, y & 3, z & 3);
    }

    public ChunkPos getPos() {
        return chunkPos;
    }

    public LevelChunkSection getSection(int index) {
        return getSections()[index];
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
    }

    private void removeGameEventListener(BlockEntity blockEntity, ServerLevel serverLevel) {}

    private void removeBlockEntityTicker(BlockPos pos) {}

    public void removeBlockEntity(BlockPos pos) {
        blockEntities.remove(pos);
    }

    public BlockState setBlockState(BlockPos pos, BlockState state, int flags) {
        BlockState previous = getBlockState(pos);
        WorldNatives.setState(world, pos.getX(), pos.getY(), pos.getZ(), state.stateId(), (flags & 2) != 0);
        int index = (pos.getY() >> 4) - minSection;
        synchronized (this) {
            if (index >= 0 && index < sections.length && sections[index] == baseline[index]) {
                hashes[index] = 0;
                syncedVersion = -1;
            }
        }
        return previous;
    }

    @Override
    public void setLightCorrect(boolean flag) {
        flush();
        lightCorrect = flag;
    }

    public boolean hasUnflushed() {
        synchronized (this) {
            for (int i = 0; i < sections.length; i++) {
                if (sections[i] != baseline[i]) {
                    return true;
                }
            }
            return false;
        }
    }

    public enum EntityCreationType {
        IMMEDIATE, QUEUED, CHECK;
    }
}
