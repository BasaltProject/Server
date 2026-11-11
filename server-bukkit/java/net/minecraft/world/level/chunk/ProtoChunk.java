// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.chunk;
import net.minecraft.world.level.block.Blocks;
import net.minecraft.world.entity.EntityType;
import it.unimi.dsi.fastutil.shorts.ShortList;
import java.util.List;
import java.util.Map;
import net.minecraft.core.BlockPos;
import net.minecraft.core.Holder;
import net.minecraft.core.HolderLookup;
import net.minecraft.nbt.CompoundTag;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.level.ChunkPos;
import net.minecraft.world.level.LevelHeightAccessor;
import net.minecraft.world.level.biome.Biome;
import net.minecraft.world.level.block.Block;
import net.minecraft.world.level.block.entity.BlockEntity;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.chunk.status.ChunkStatus;
import net.minecraft.world.level.levelgen.BelowZeroRetrogen;
import net.minecraft.world.level.levelgen.blending.BlendingData;
import net.minecraft.world.level.levelgen.structure.Structure;
import net.minecraft.world.level.levelgen.structure.StructureStart;
import net.minecraft.world.level.lighting.LevelLightEngine;
import net.minecraft.world.level.material.Fluid;
import net.minecraft.world.level.material.FluidState;
import net.minecraft.world.ticks.LevelChunkTicks;
import net.minecraft.world.ticks.ProtoChunkTicks;
import net.minecraft.world.ticks.TickContainerAccess;
public class ProtoChunk extends ChunkAccess {
    private final Map<BlockPos, BlockEntity> tiles = new java.util.concurrent.ConcurrentHashMap<>();
    private final Map<BlockPos, CompoundTag> tileTags = new java.util.concurrent.ConcurrentHashMap<>();
    private final List<CompoundTag> entityTags = new java.util.ArrayList<>();
    private ProtoChunkTicks<Block> blockTicksIn = new ProtoChunkTicks<>();
    private ProtoChunkTicks<Fluid> fluidTicksIn = new ProtoChunkTicks<>();
    private ChunkStatus status = ChunkStatus.EMPTY;
    private LevelLightEngine lightEngine;
    private BelowZeroRetrogen retrogen;
    private static LevelChunkSection[] blankSections(LevelChunkSection[] given, LevelHeightAccessor heights, PalettedContainerFactory factory) {
        LevelChunkSection[] result = given != null && given.length == heights.getSectionsCount() ? given : new LevelChunkSection[heights.getSectionsCount()];
        for (int i = 0; i < result.length; i++) {
            if (result[i] == null) {
                result[i] = new LevelChunkSection(factory);
            }
        }
        return result;
    }
    public ProtoChunk(ChunkPos chunkPos, UpgradeData upgradeData, LevelHeightAccessor levelHeightAccessor, PalettedContainerFactory containerFactory, BlendingData blendingData) {
        super(chunkPos, upgradeData, levelHeightAccessor, containerFactory, 0L, blankSections(null, levelHeightAccessor, containerFactory), blendingData);
        
    }
    public ProtoChunk(ChunkPos chunkPos, UpgradeData upgradeData, LevelChunkSection [] sections, ProtoChunkTicks<Block> blockTicks, ProtoChunkTicks<Fluid> fluidTicks, LevelHeightAccessor levelHeightAccessor, PalettedContainerFactory containerFactory, BlendingData blendingData) {
        super(chunkPos, upgradeData, levelHeightAccessor, containerFactory, 0L, blankSections(sections, levelHeightAccessor, containerFactory), blendingData);
        this.blockTicksIn = blockTicks;
        this.fluidTicksIn = fluidTicks;
        
    }
    public TickContainerAccess<Block> getBlockTicks() {
        return blockTicksIn;
    }
    public TickContainerAccess<Fluid> getFluidTicks() {
        return fluidTicksIn;
    }
    public ChunkAccess.PackedTicks getTicksForSerialization(long currentTick) {
        return new ChunkAccess.PackedTicks(List.of(), List.of());
    }
    public BlockState getBlockState(BlockPos pos) {
        int index = getSectionIndex(pos.getY());
        if (index < 0 || index >= sections.length) {
            return Blocks.VOID_AIR.defaultBlockState();
        }
        return sections[index].getBlockState(pos.getX() & 15, pos.getY() & 15, pos.getZ() & 15);
    }
    public FluidState getFluidState(BlockPos pos) {
        return getBlockState(pos).getFluidState();
    }
    public BlockState setBlockState(BlockPos pos, BlockState state, int flags) {
        int index = getSectionIndex(pos.getY());
        if (index < 0 || index >= sections.length) {
            return Blocks.VOID_AIR.defaultBlockState();
        }
        BlockState previous = sections[index].setBlockState(pos.getX() & 15, pos.getY() & 15, pos.getZ() & 15, state);
        if (previous != state) {
            tiles.remove(pos);
            tileTags.remove(pos);
            this.unsaved = true;
        }
        return previous;
    }
    public void setBlockEntity(BlockEntity blockEntity) {
        tiles.put(blockEntity.getBlockPos().immutable(), blockEntity);
    }
    public BlockEntity getBlockEntity(BlockPos pos) {
        return tiles.get(pos);
    }
    public Map<BlockPos, BlockEntity> getBlockEntities() {
        return tiles;
    }
    public void addEntity(CompoundTag tag) {
        entityTags.add(tag);
    }
    public void addEntity(Entity entity) {
        if (!entity.isPassenger()) {
            CompoundTag tag = new CompoundTag();
            tag.putString("id", EntityType.getKey(entity.getType()).toString());
            entityTags.add(tag);
        }
    }
    public void setStartForStructure(Structure structure, StructureStart structureStart) {
        super.setStartForStructure(structure, structureStart);
    }
    public List<CompoundTag> getEntities() {
        return entityTags;
    }
    public ChunkStatus getPersistedStatus() {
        return status;
    }
    public void setPersistedStatus(ChunkStatus status) {
        this.status = status;
        this.unsaved = true;
    }
    public Holder<Biome> getNoiseBiome(int quartX, int quartY, int quartZ) {
        return super.getNoiseBiome(quartX, quartY, quartZ);
    }
    public static short packOffsetCoordinates(BlockPos pos) {
        return (short) ((pos.getX() & 15) | ((pos.getY() & 15) << 4) | ((pos.getZ() & 15) << 8));
    }
    public static BlockPos unpackOffsetCoordinates(short packedData, int sectionY, ChunkPos chunkPos) {
        return new BlockPos((packedData & 15) + chunkPos.getMinBlockX(), ((packedData >> 4) & 15) + (sectionY << 4),
            ((packedData >> 8) & 15) + chunkPos.getMinBlockZ());
    }
    public void markPosForPostProcessing(BlockPos blockPos) {
        super.markPosForPostProcessing(blockPos);
    }
    public void addPackedPostProcess(ShortList packedOffsets, int sectionIndex) {
        super.addPackedPostProcess(packedOffsets, sectionIndex);
    }
    public Map<BlockPos, CompoundTag> getBlockEntityNbts() {
        return tileTags;
    }
    public CompoundTag getBlockEntityNbtForSaving(BlockPos blockPos, HolderLookup.Provider registryAccess) {
        BlockEntity live = getBlockEntity(blockPos);
        return live != null ? live.saveWithFullMetadata(registryAccess) : tileTags.get(blockPos);
    }
    public void removeBlockEntity(BlockPos pos) {
        tiles.remove(pos);
        tileTags.remove(pos);
    }
    public void setLightEngine(LevelLightEngine lightEngine) {
        this.lightEngine = lightEngine;
    }
    public void setBelowZeroRetrogen(BelowZeroRetrogen belowZeroRetrogen) {
        this.retrogen = belowZeroRetrogen;
    }
    public BelowZeroRetrogen getBelowZeroRetrogen() {
        return retrogen;
    }
    public LevelChunkTicks<Block> unpackBlockTicks() {
        return new LevelChunkTicks<>();
    }
    public LevelChunkTicks<Fluid> unpackFluidTicks() {
        return new LevelChunkTicks<>();
    }
    public LevelHeightAccessor getHeightAccessorForGeneration() {
        return this;


    }
}
