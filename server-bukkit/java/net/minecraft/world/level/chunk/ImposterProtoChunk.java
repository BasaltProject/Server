// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.chunk;
import net.minecraft.world.ticks.BlackholeTickAccess;
import it.unimi.dsi.fastutil.longs.LongSet;
import java.util.Map;
import java.util.function.BiConsumer;
import java.util.function.Predicate;
import net.minecraft.core.BlockPos;
import net.minecraft.core.Holder;
import net.minecraft.core.HolderLookup;
import net.minecraft.nbt.CompoundTag;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.level.ChunkPos;
import net.minecraft.world.level.biome.Biome;
import net.minecraft.world.level.biome.BiomeResolver;
import net.minecraft.world.level.block.Block;
import net.minecraft.world.level.block.entity.BlockEntity;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.chunk.status.ChunkStatus;
import net.minecraft.world.level.levelgen.Heightmap;
import net.minecraft.world.level.levelgen.blending.BlendingData;
import net.minecraft.world.level.levelgen.structure.Structure;
import net.minecraft.world.level.levelgen.structure.StructureStart;
import net.minecraft.world.level.lighting.ChunkSkyLightSources;
import net.minecraft.world.level.material.Fluid;
import net.minecraft.world.level.material.FluidState;
import net.minecraft.world.ticks.TickContainerAccess;
public class ImposterProtoChunk extends ProtoChunk {
    private final LevelChunk wrapped;
    private final boolean allowWrites;
    public ImposterProtoChunk(LevelChunk wrapped, boolean allowWrites) {
        super(wrapped.getPos(), UpgradeData.EMPTY, wrapped.getLevel(), wrapped.getLevel().palettedContainerFactory(), wrapped.getBlendingData());
        this.wrapped = wrapped;
        this.allowWrites = allowWrites;
    }
    public BlockEntity getBlockEntity(BlockPos pos) {
        return wrapped.getBlockEntity(pos);
    }
    public BlockState getBlockState(BlockPos pos) {
        return wrapped.getBlockState(pos);
    }
    public FluidState getFluidState(BlockPos pos) {
        return wrapped.getFluidState(pos);
    }
    public LevelChunkSection getSection(int sectionIndex) {
        return wrapped.getSection(sectionIndex);
    }
    public BlockState setBlockState(BlockPos pos, BlockState state, int flags) {
        return allowWrites ? wrapped.setBlockState(pos, state, flags) : null;
    }
    public void setBlockEntity(BlockEntity blockEntity) {
        if (allowWrites) {
            wrapped.setBlockEntity(blockEntity);
        }
    }
    public void addEntity(Entity entity) {
        if (allowWrites) {
            wrapped.addEntity(entity);
        }
    }
    public void setPersistedStatus(ChunkStatus status) {
        if (allowWrites) {
            super.setPersistedStatus(status);
        }
    }
    public LevelChunkSection[] getSections() {
        return wrapped.getSections();
    }
    public void setHeightmap(Heightmap.Types key, long[] data) {
        if (allowWrites) {
            wrapped.setHeightmap(key, data);
        }
    }
    public Heightmap getOrCreateHeightmapUnprimed(Heightmap.Types type) {
        return wrapped.getOrCreateHeightmapUnprimed(type);
    }
    public int getHeight(Heightmap.Types type, int x, int z) {
        return wrapped.getHeight(type, x, z);
    }
    public Holder<Biome> getNoiseBiome(int quartX, int quartY, int quartZ) {
        return wrapped.getNoiseBiome(quartX, quartY, quartZ);
    }
    public ChunkPos getPos() {
        return wrapped.getPos();
    }
    public StructureStart getStartForStructure(Structure structure) {
        return wrapped.getStartForStructure(structure);
    }
    public void setStartForStructure(Structure structure, StructureStart structureStart) {
    }
    public Map<Structure, StructureStart> getAllStarts() {
        return wrapped.getAllStarts();
    }
    public void setAllStarts(Map<Structure, StructureStart> starts) {
        if (allowWrites) {
            wrapped.setAllStarts(starts);
        }
    }
    public LongSet getReferencesForStructure(Structure structure) {
        return wrapped.getReferencesForStructure(structure);
    }
    public void addReferenceForStructure(Structure structure, long reference) {
    }
    public Map<Structure, LongSet> getAllReferences() {
        return wrapped.getAllReferences();
    }
    public void setAllReferences(Map<Structure, LongSet> data) {
        if (allowWrites) {
            wrapped.setAllReferences(data);
        }
    }
    public void markUnsaved() {
        wrapped.markUnsaved();
    }
    public boolean canBeSerialized() {
        return false;
    }
    public boolean tryMarkSaved() {
        return false;
    }
    public boolean isUnsaved() {
        return false;
    }
    public ChunkStatus getPersistedStatus() {
        return wrapped.getPersistedStatus();
    }
    public void removeBlockEntity(BlockPos pos) {
        if (allowWrites) {
            wrapped.removeBlockEntity(pos);
        }
    }
    public void markPosForPostProcessing(BlockPos blockPos) {
    }
    public void setBlockEntityNbt(CompoundTag entityTag) {
    }
    public CompoundTag getBlockEntityNbt(BlockPos blockPos) {
        return wrapped.getBlockEntityNbt(blockPos);
    }
    public CompoundTag getBlockEntityNbtForSaving(BlockPos blockPos, HolderLookup.Provider registryAccess) {
        return wrapped.getBlockEntityNbtForSaving(blockPos, registryAccess);
    }
    public void findBlocks(Predicate<BlockState> predicate, BiConsumer<BlockPos, BlockState> consumer) {
        wrapped.findBlocks(predicate, consumer);
    }
    public TickContainerAccess<Block> getBlockTicks() {
        return allowWrites ? wrapped.getBlockTicks() : BlackholeTickAccess.emptyContainer();
    }
    public TickContainerAccess<Fluid> getFluidTicks() {
        return allowWrites ? wrapped.getFluidTicks() : BlackholeTickAccess.emptyContainer();
    }
    public ChunkAccess.PackedTicks getTicksForSerialization(long currentTick) {
        return wrapped.getTicksForSerialization(currentTick);
    }
    public BlendingData getBlendingData() {
        return wrapped.getBlendingData();
    }
    public LevelChunk getWrapped() {
        return wrapped;
    }
    public boolean isLightCorrect() {
        return wrapped.isLightCorrect();
    }
    public void setLightCorrect(boolean isLightCorrect) {
        wrapped.setLightCorrect(isLightCorrect);
    }
    public void fillBiomesFromNoise(BiomeResolver biomeResolver) {
        if (allowWrites) {
            wrapped.fillBiomesFromNoise(biomeResolver);
        }
    }
    public void initializeLightSources() {
        wrapped.initializeLightSources();
    }
    public ChunkSkyLightSources getSkyLightSources() {
        return wrapped.getSkyLightSources();


    }
}
