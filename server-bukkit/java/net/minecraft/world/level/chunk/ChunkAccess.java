package net.minecraft.world.level.chunk;

import ca.spottedleaf.moonrise.patches.starlight.light.SWMRNibbleArray;
import it.unimi.dsi.fastutil.longs.LongSet;
import it.unimi.dsi.fastutil.shorts.ShortList;
import java.util.Collection;
import java.util.List;
import java.util.Map;
import java.util.Optional;
import java.util.Set;
import java.util.function.BiConsumer;
import java.util.function.Predicate;
import java.util.function.Supplier;
import java.util.stream.Stream;
import net.minecraft.core.BlockPos;
import net.minecraft.core.Holder;
import net.minecraft.core.HolderLookup;
import net.minecraft.nbt.CompoundTag;
import net.minecraft.util.ProblemReporter;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.level.ChunkPos;
import net.minecraft.world.level.ClipBlockStateContext;
import net.minecraft.world.level.ClipContext;
import net.minecraft.world.level.LevelHeightAccessor;
import net.minecraft.world.level.biome.BiomeGenerationSettings;
import net.minecraft.world.level.biome.BiomeResolver;
import net.minecraft.world.level.block.Block;
import net.minecraft.world.level.block.entity.BlockEntity;
import net.minecraft.world.level.block.entity.BlockEntityType;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.chunk.status.ChunkStatus;
import net.minecraft.world.level.gameevent.GameEventListenerRegistry;
import net.minecraft.world.level.levelgen.BelowZeroRetrogen;
import net.minecraft.world.level.levelgen.Heightmap;
import net.minecraft.world.level.levelgen.blending.BlendingData;
import net.minecraft.world.level.levelgen.structure.Structure;
import net.minecraft.world.level.levelgen.structure.StructureStart;
import net.minecraft.world.level.lighting.ChunkSkyLightSources;
import net.minecraft.world.level.material.FluidState;
import net.minecraft.world.phys.AABB;
import net.minecraft.world.phys.BlockHitResult;
import net.minecraft.world.phys.Vec3;
import net.minecraft.world.phys.shapes.VoxelShape;
import net.minecraft.world.ticks.TickContainerAccess;
import org.basaltmc.bukkit.Unsupported;
public abstract class ChunkAccess implements LightChunk, StructureAccess, BiomeResolver {
    protected final ChunkPos chunkPos;
    protected final LevelChunkSection[] sections;
    protected volatile boolean unsaved;
    protected volatile boolean lightCorrect;

    public ChunkAccess(ChunkPos chunkPos, UpgradeData upgradeData, LevelHeightAccessor levelHeightAccessor, PalettedContainerFactory palettedContainerFactory, long value, LevelChunkSection[] levelChunkSections, BlendingData blendingData) {
        this.chunkPos = chunkPos;
        this.sections = levelChunkSections;
    }
    public void addEntity(Entity entity) {
        throw Unsupported.hit("net.minecraft.world.level.chunk.ChunkAccess.addEntity");
    }
    public void addPackedPostProcess(ShortList packedOffsets, int sectionIndex) {
        throw Unsupported.hit("net.minecraft.world.level.chunk.ChunkAccess.addPackedPostProcess");
    }
    public void addReferenceForStructure(Structure structure, long reference) {
        throw Unsupported.hit("net.minecraft.world.level.chunk.ChunkAccess.addReferenceForStructure");
    }
    public boolean canBeSerialized() {
        throw Unsupported.hit("net.minecraft.world.level.chunk.ChunkAccess.canBeSerialized");
    }
    public BiomeGenerationSettings carverBiome(Supplier source) {
        throw Unsupported.hit("net.minecraft.world.level.chunk.ChunkAccess.carverBiome");
    }
    public BlockHitResult clip(ClipContext clipContext) {
        throw Unsupported.hit("net.minecraft.world.level.chunk.ChunkAccess.clip");
    }
    public BlockHitResult clip(ClipContext clipContext, Predicate predicate) {
        throw Unsupported.hit("net.minecraft.world.level.chunk.ChunkAccess.clip");
    }
    public BlockHitResult clip(ClipContext clipContext, BlockPos blockPos) {
        throw Unsupported.hit("net.minecraft.world.level.chunk.ChunkAccess.clip");
    }
    public BlockHitResult clip(ClipContext clipContext, BlockPos blockPos, Predicate predicate) {
        throw Unsupported.hit("net.minecraft.world.level.chunk.ChunkAccess.clip");
    }
    public BlockHitResult clipWithInteractionOverride(Vec3 vec3, Vec3 vec32, BlockPos blockPos, VoxelShape voxelShape, BlockState blockState) {
        throw Unsupported.hit("net.minecraft.world.level.chunk.ChunkAccess.clipWithInteractionOverride");
    }
    public void fillBiomesFromNoise(BiomeResolver biomeResolver) {
        throw Unsupported.hit("net.minecraft.world.level.chunk.ChunkAccess.fillBiomesFromNoise");
    }
    public void findBlocks(Predicate predicate, BiConsumer consumer) {
        throw Unsupported.hit("net.minecraft.world.level.chunk.ChunkAccess.findBlocks");
    }
    public Map getAllReferences() {
        throw Unsupported.hit("net.minecraft.world.level.chunk.ChunkAccess.getAllReferences");
    }
    public Map getAllStarts() {
        throw Unsupported.hit("net.minecraft.world.level.chunk.ChunkAccess.getAllStarts");
    }
    public BelowZeroRetrogen getBelowZeroRetrogen() {
        throw Unsupported.hit("net.minecraft.world.level.chunk.ChunkAccess.getBelowZeroRetrogen");
    }
    public BlendingData getBlendingData() {
        throw Unsupported.hit("net.minecraft.world.level.chunk.ChunkAccess.getBlendingData");
    }
    public Set getBlockEntitiesPos() {
        throw Unsupported.hit("net.minecraft.world.level.chunk.ChunkAccess.getBlockEntitiesPos");
    }
    public BlockEntity getBlockEntity(BlockPos blockPos) {
        throw Unsupported.hit("net.minecraft.world.level.chunk.ChunkAccess.getBlockEntity");
    }
    public Optional getBlockEntity(BlockPos blockPos, BlockEntityType blockEntityType) {
        throw Unsupported.hit("net.minecraft.world.level.chunk.ChunkAccess.getBlockEntity");
    }
    public CompoundTag getBlockEntityNbt(BlockPos blockPos) {
        throw Unsupported.hit("net.minecraft.world.level.chunk.ChunkAccess.getBlockEntityNbt");
    }
    public CompoundTag getBlockEntityNbtForSaving(BlockPos blockPos, HolderLookup.Provider registryAccess) {
        throw Unsupported.hit("net.minecraft.world.level.chunk.ChunkAccess.getBlockEntityNbtForSaving");
    }
    public double getBlockFloorHeight(BlockPos blockPos) {
        throw Unsupported.hit("net.minecraft.world.level.chunk.ChunkAccess.getBlockFloorHeight");
    }
    public double getBlockFloorHeight(VoxelShape voxelShape, Supplier supplier) {
        throw Unsupported.hit("net.minecraft.world.level.chunk.ChunkAccess.getBlockFloorHeight");
    }
    public Block getBlockIfLoaded(BlockPos blockPos) {
        throw Unsupported.hit("net.minecraft.world.level.chunk.ChunkAccess.getBlockIfLoaded");
    }
    public BlockState getBlockState(int value, int value2, int value3) {
        throw Unsupported.hit("net.minecraft.world.level.chunk.ChunkAccess.getBlockState");
    }
    public BlockState getBlockState(BlockPos blockPos) {
        throw Unsupported.hit("net.minecraft.world.level.chunk.ChunkAccess.getBlockState");
    }
    public BlockState getBlockStateIfLoaded(BlockPos blockPos) {
        throw Unsupported.hit("net.minecraft.world.level.chunk.ChunkAccess.getBlockStateIfLoaded");
    }
    public Stream getBlockStates(AABB aABB) {
        throw Unsupported.hit("net.minecraft.world.level.chunk.ChunkAccess.getBlockStates");
    }
    public TickContainerAccess getBlockTicks() {
        throw Unsupported.hit("net.minecraft.world.level.chunk.ChunkAccess.getBlockTicks");
    }
    public FluidState getFluidIfLoaded(BlockPos blockPos) {
        throw Unsupported.hit("net.minecraft.world.level.chunk.ChunkAccess.getFluidIfLoaded");
    }
    public FluidState getFluidState(BlockPos blockPos) {
        throw Unsupported.hit("net.minecraft.world.level.chunk.ChunkAccess.getFluidState");
    }
    public TickContainerAccess getFluidTicks() {
        throw Unsupported.hit("net.minecraft.world.level.chunk.ChunkAccess.getFluidTicks");
    }
    public int getHeight() {
        throw Unsupported.hit("net.minecraft.world.level.chunk.ChunkAccess.getHeight");
    }
    public int getHeight(Heightmap.Types type, int x, int z) {
        throw Unsupported.hit("net.minecraft.world.level.chunk.ChunkAccess.getHeight");
    }
    public LevelHeightAccessor getHeightAccessorForGeneration() {
        throw Unsupported.hit("net.minecraft.world.level.chunk.ChunkAccess.getHeightAccessorForGeneration");
    }
    public Collection getHeightmaps() {
        throw Unsupported.hit("net.minecraft.world.level.chunk.ChunkAccess.getHeightmaps");
    }
    public int getHighestFilledSectionIndex() {
        throw Unsupported.hit("net.minecraft.world.level.chunk.ChunkAccess.getHighestFilledSectionIndex");
    }
    public ChunkStatus getHighestGeneratedStatus() {
        throw Unsupported.hit("net.minecraft.world.level.chunk.ChunkAccess.getHighestGeneratedStatus");
    }
    public int getHighestSectionPosition() {
        throw Unsupported.hit("net.minecraft.world.level.chunk.ChunkAccess.getHighestSectionPosition");
    }
    public long getInhabitedTime() {
        throw Unsupported.hit("net.minecraft.world.level.chunk.ChunkAccess.getInhabitedTime");
    }
    public int getLightEmission(BlockPos blockPos) {
        throw Unsupported.hit("net.minecraft.world.level.chunk.ChunkAccess.getLightEmission");
    }
    public GameEventListenerRegistry getListenerRegistry(int section) {
        throw Unsupported.hit("net.minecraft.world.level.chunk.ChunkAccess.getListenerRegistry");
    }
    public int getMaxSectionY() {
        throw Unsupported.hit("net.minecraft.world.level.chunk.ChunkAccess.getMaxSectionY");
    }
    public int getMaxY() {
        throw Unsupported.hit("net.minecraft.world.level.chunk.ChunkAccess.getMaxY");
    }
    public int getMinSectionY() {
        throw Unsupported.hit("net.minecraft.world.level.chunk.ChunkAccess.getMinSectionY");
    }
    public int getMinY() {
        throw Unsupported.hit("net.minecraft.world.level.chunk.ChunkAccess.getMinY");
    }
    public Holder getNoiseBiome(int quartX, int quartY, int quartZ) {
        throw Unsupported.hit("net.minecraft.world.level.chunk.ChunkAccess.getNoiseBiome");
    }
    public Heightmap getOrCreateHeightmapUnprimed(Heightmap.Types type) {
        throw Unsupported.hit("net.minecraft.world.level.chunk.ChunkAccess.getOrCreateHeightmapUnprimed");
    }
    public ChunkStatus getPersistedStatus() {
        throw Unsupported.hit("net.minecraft.world.level.chunk.ChunkAccess.getPersistedStatus");
    }
    public ChunkPos getPos() {
        return chunkPos;
    }
    public ShortList[] getPostProcessing() {
        throw Unsupported.hit("net.minecraft.world.level.chunk.ChunkAccess.getPostProcessing");
    }
    public LongSet getReferencesForStructure(Structure structure) {
        throw Unsupported.hit("net.minecraft.world.level.chunk.ChunkAccess.getReferencesForStructure");
    }
    public LevelChunkSection getSection(int sectionIndex) {
        return getSections()[sectionIndex];
    }
    public int getSectionIndex(int value) {
        throw Unsupported.hit("net.minecraft.world.level.chunk.ChunkAccess.getSectionIndex");
    }
    public int getSectionIndexFromSectionY(int value) {
        throw Unsupported.hit("net.minecraft.world.level.chunk.ChunkAccess.getSectionIndexFromSectionY");
    }
    public int getSectionYFromSectionIndex(int value) {
        throw Unsupported.hit("net.minecraft.world.level.chunk.ChunkAccess.getSectionYFromSectionIndex");
    }
    public LevelChunkSection[] getSections() {
        return sections;
    }
    public int getSectionsCount() {
        return getSections().length;
    }
    public ChunkSkyLightSources getSkyLightSources() {
        throw Unsupported.hit("net.minecraft.world.level.chunk.ChunkAccess.getSkyLightSources");
    }
    public StructureStart getStartForStructure(Structure structure) {
        throw Unsupported.hit("net.minecraft.world.level.chunk.ChunkAccess.getStartForStructure");
    }
    public ChunkAccess.PackedTicks getTicksForSerialization(long currentTick) {
        throw Unsupported.hit("net.minecraft.world.level.chunk.ChunkAccess.getTicksForSerialization");
    }
    public UpgradeData getUpgradeData() {
        throw Unsupported.hit("net.minecraft.world.level.chunk.ChunkAccess.getUpgradeData");
    }
    public boolean hasPrimedHeightmap(Heightmap.Types type) {
        throw Unsupported.hit("net.minecraft.world.level.chunk.ChunkAccess.hasPrimedHeightmap");
    }
    public void incrementInhabitedTime() {
        throw Unsupported.hit("net.minecraft.world.level.chunk.ChunkAccess.incrementInhabitedTime");
    }
    public void initializeLightSources() {
        return;
    }
    public BlockHitResult isBlockInLine(ClipBlockStateContext clipBlockStateContext) {
        throw Unsupported.hit("net.minecraft.world.level.chunk.ChunkAccess.isBlockInLine");
    }
    public boolean isInsideBuildHeight(int value) {
        throw Unsupported.hit("net.minecraft.world.level.chunk.ChunkAccess.isInsideBuildHeight");
    }
    public boolean isLightCorrect() {
        return lightCorrect;
    }
    public boolean isOldNoiseGeneration() {
        throw Unsupported.hit("net.minecraft.world.level.chunk.ChunkAccess.isOldNoiseGeneration");
    }
    public boolean isOutsideBuildHeight(int value) {
        throw Unsupported.hit("net.minecraft.world.level.chunk.ChunkAccess.isOutsideBuildHeight");
    }
    public boolean isOutsideBuildHeight(BlockPos blockPos) {
        throw Unsupported.hit("net.minecraft.world.level.chunk.ChunkAccess.isOutsideBuildHeight");
    }
    public boolean isUnsaved() {
        return unsaved;
    }
    public boolean isUpgrading() {
        throw Unsupported.hit("net.minecraft.world.level.chunk.ChunkAccess.isUpgrading");
    }
    public boolean isYSpaceEmpty(int yStartInclusive, int yEndInclusive) {
        throw Unsupported.hit("net.minecraft.world.level.chunk.ChunkAccess.isYSpaceEmpty");
    }
    public void markUnsaved() {
        unsaved = true;
    }
    public ProblemReporter.PathElement problemPath() {
        throw Unsupported.hit("net.minecraft.world.level.chunk.ChunkAccess.problemPath");
    }
    public void removeBlockEntity(BlockPos pos) {
        throw Unsupported.hit("net.minecraft.world.level.chunk.ChunkAccess.removeBlockEntity");
    }
    public void setAllReferences(Map data) {
        throw Unsupported.hit("net.minecraft.world.level.chunk.ChunkAccess.setAllReferences");
    }
    public void setAllStarts(Map starts) {
        throw Unsupported.hit("net.minecraft.world.level.chunk.ChunkAccess.setAllStarts");
    }
    public void setBlockEntity(BlockEntity blockEntity) {
        throw Unsupported.hit("net.minecraft.world.level.chunk.ChunkAccess.setBlockEntity");
    }
    public void setBlockEntityNbt(CompoundTag entityTag) {
        throw Unsupported.hit("net.minecraft.world.level.chunk.ChunkAccess.setBlockEntityNbt");
    }
    public BlockState setBlockState(BlockPos pos, BlockState state) {
        throw Unsupported.hit("net.minecraft.world.level.chunk.ChunkAccess.setBlockState");
    }
    public BlockState setBlockState(BlockPos blockPos, BlockState blockState, int value) {
        throw Unsupported.hit("net.minecraft.world.level.chunk.ChunkAccess.setBlockState");
    }
    public void setHeightmap(Heightmap.Types key, long[] data) {
        throw Unsupported.hit("net.minecraft.world.level.chunk.ChunkAccess.setHeightmap");
    }
    public void setInhabitedTime(long inhabitedTime) {
        throw Unsupported.hit("net.minecraft.world.level.chunk.ChunkAccess.setInhabitedTime");
    }
    public void setLightCorrect(boolean isLightCorrect) {
        lightCorrect = isLightCorrect;
    }
    public void setStartForStructure(Structure structure, StructureStart structureStart) {
        throw Unsupported.hit("net.minecraft.world.level.chunk.ChunkAccess.setStartForStructure");
    }
    public boolean[] starlight$getBlockEmptinessMap() {
        throw Unsupported.hit("net.minecraft.world.level.chunk.ChunkAccess.starlight$getBlockEmptinessMap");
    }
    public SWMRNibbleArray[] starlight$getBlockNibbles() {
        throw Unsupported.hit("net.minecraft.world.level.chunk.ChunkAccess.starlight$getBlockNibbles");
    }
    public boolean[] starlight$getSkyEmptinessMap() {
        throw Unsupported.hit("net.minecraft.world.level.chunk.ChunkAccess.starlight$getSkyEmptinessMap");
    }
    public SWMRNibbleArray[] starlight$getSkyNibbles() {
        throw Unsupported.hit("net.minecraft.world.level.chunk.ChunkAccess.starlight$getSkyNibbles");
    }
    public void starlight$setBlockEmptinessMap(boolean[] flags) {
        throw Unsupported.hit("net.minecraft.world.level.chunk.ChunkAccess.starlight$setBlockEmptinessMap");
    }
    public void starlight$setBlockNibbles(SWMRNibbleArray[] sWMRNibbleArrays) {
        throw Unsupported.hit("net.minecraft.world.level.chunk.ChunkAccess.starlight$setBlockNibbles");
    }
    public void starlight$setSkyEmptinessMap(boolean[] flags) {
        throw Unsupported.hit("net.minecraft.world.level.chunk.ChunkAccess.starlight$setSkyEmptinessMap");
    }
    public void starlight$setSkyNibbles(SWMRNibbleArray[] sWMRNibbleArrays) {
        throw Unsupported.hit("net.minecraft.world.level.chunk.ChunkAccess.starlight$setSkyNibbles");
    }
    public boolean tryMarkSaved() {
        throw Unsupported.hit("net.minecraft.world.level.chunk.ChunkAccess.tryMarkSaved");
    }

    public record PackedTicks(List blocks, List fluids) {
    }
}
