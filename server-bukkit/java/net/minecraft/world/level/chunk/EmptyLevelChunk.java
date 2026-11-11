// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.chunk;
import net.minecraft.world.level.block.Blocks;
import net.minecraft.world.level.material.Fluids;
import net.minecraft.core.BlockPos;
import net.minecraft.core.Holder;
import net.minecraft.server.level.FullChunkStatus;
import net.minecraft.world.level.ChunkPos;
import net.minecraft.world.level.Level;
import net.minecraft.world.level.biome.Biome;
import net.minecraft.world.level.block.entity.BlockEntity;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.material.FluidState;
public class EmptyLevelChunk extends LevelChunk {
    private final Holder<Biome> biome;
    public EmptyLevelChunk(Level level, ChunkPos pos, Holder<Biome> biome) {
        super(level, pos);
        this.biome = biome;
        
    }
    public BlockState getBlockState(BlockPos pos) {
        return Blocks.VOID_AIR.defaultBlockState();
    }
    public BlockState setBlockState(BlockPos pos, BlockState state, int flags) {
        return null;
    }
    public FluidState getFluidState(BlockPos pos) {
        return Fluids.EMPTY.defaultFluidState();
    }
    public int getLightEmission(BlockPos pos) {
        return 0;
    }
    public BlockEntity getBlockEntity(BlockPos pos, LevelChunk.EntityCreationType creationType) {
        return null;
    }
    public void addAndRegisterBlockEntity(BlockEntity blockEntity) {
    }
    public void setBlockEntity(BlockEntity blockEntity) {
    }
    public void removeBlockEntity(BlockPos pos) {
    }
    public boolean isEmpty() {
        return true;
    }
    public boolean isYSpaceEmpty(int yStartInclusive, int yEndInclusive) {
        return true;
    }
    public FullChunkStatus getFullStatus() {
        return FullChunkStatus.FULL;
    }
    public Holder<Biome> getNoiseBiome(int quartX, int quartY, int quartZ) {
        return biome;
    }
}
