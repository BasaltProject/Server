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
public abstract class Level implements LevelAccessor {
    protected Level() {
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
        throw Unsupported.hit("net.minecraft.world.level.Level.getSectionIndex");
    }
    public CraftWorld getWorld() {
        throw Unsupported.hit("net.minecraft.world.level.Level.getWorld");
    }
    public boolean isClientSide() {
        throw Unsupported.hit("net.minecraft.world.level.Level.isClientSide");
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
}
