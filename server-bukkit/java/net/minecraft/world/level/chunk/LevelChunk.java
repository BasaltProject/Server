package net.minecraft.world.level.chunk;

import java.util.Map;
import net.minecraft.core.BlockPos;
import net.minecraft.core.Holder;
import net.minecraft.server.level.FullChunkStatus;
import net.minecraft.util.debug.DebugValueSource;
import net.minecraft.world.level.ChunkPos;
import net.minecraft.world.level.Level;
import net.minecraft.world.level.LevelHeightAccessor;
import net.minecraft.world.level.block.entity.BlockEntity;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.levelgen.blending.BlendingData;
import org.basaltmc.bukkit.Unsupported;
public class LevelChunk extends ChunkAccess implements DebugValueSource {
    protected LevelChunk() {
        super((ChunkPos) null, (UpgradeData) null, (LevelHeightAccessor) null, (PalettedContainerFactory) null, 0L, (LevelChunkSection[]) null, (BlendingData) null);
    }
    public Map blockEntities;
    public Map heightmaps;
    public int locX = 0;
    public int locZ = 0;
    public boolean mustNotSave = false;
    public Map getBlockEntities() {
        throw Unsupported.hit("net.minecraft.world.level.chunk.LevelChunk.getBlockEntities");
    }
    public BlockEntity getBlockEntity(BlockPos pos) {
        throw Unsupported.hit("net.minecraft.world.level.chunk.LevelChunk.getBlockEntity");
    }
    public BlockEntity getBlockEntity(BlockPos pos, LevelChunk.EntityCreationType creationType) {
        throw Unsupported.hit("net.minecraft.world.level.chunk.LevelChunk.getBlockEntity");
    }
    public BlockState getBlockState(BlockPos pos) {
        throw Unsupported.hit("net.minecraft.world.level.chunk.LevelChunk.getBlockState");
    }
    public FullChunkStatus getFullStatus() {
        throw Unsupported.hit("net.minecraft.world.level.chunk.LevelChunk.getFullStatus");
    }
    public int getHeight() {
        throw Unsupported.hit("net.minecraft.world.level.chunk.LevelChunk.getHeight");
    }
    public Level getLevel() {
        throw Unsupported.hit("net.minecraft.world.level.chunk.LevelChunk.getLevel");
    }
    public Holder getNoiseBiome(int value, int value2, int value3) {
        throw Unsupported.hit("net.minecraft.world.level.chunk.LevelChunk.getNoiseBiome");
    }
    public ChunkPos getPos() {
        throw Unsupported.hit("net.minecraft.world.level.chunk.LevelChunk.getPos");
    }
    public LevelChunkSection getSection(int value) {
        throw Unsupported.hit("net.minecraft.world.level.chunk.LevelChunk.getSection");
    }
    public int getSectionIndex(int value) {
        throw Unsupported.hit("net.minecraft.world.level.chunk.LevelChunk.getSectionIndex");
    }
    public LevelChunkSection[] getSections() {
        throw Unsupported.hit("net.minecraft.world.level.chunk.LevelChunk.getSections");
    }
    public void markUnsaved() {
        throw Unsupported.hit("net.minecraft.world.level.chunk.LevelChunk.markUnsaved");
    }
    public void removeBlockEntity(BlockPos pos) {
        throw Unsupported.hit("net.minecraft.world.level.chunk.LevelChunk.removeBlockEntity");
    }
    public BlockState setBlockState(BlockPos blockPos, BlockState blockState, int value) {
        throw Unsupported.hit("net.minecraft.world.level.chunk.LevelChunk.setBlockState");
    }
    public void setLightCorrect(boolean flag) {
        throw Unsupported.hit("net.minecraft.world.level.chunk.LevelChunk.setLightCorrect");
    }

    public enum EntityCreationType {
        IMMEDIATE, QUEUED, CHECK;
    }
}
