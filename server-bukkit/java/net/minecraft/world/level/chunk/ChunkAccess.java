// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
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
import net.minecraft.world.level.biome.Biome;
import net.minecraft.world.level.material.Fluid;
import net.minecraft.world.ticks.SavedTick;
public abstract class ChunkAccess implements LightChunk, StructureAccess, BiomeResolver {
    private final Map<Structure, it.unimi.dsi.fastutil.longs.LongSet> kReferences = new java.util.concurrent.ConcurrentHashMap<>();
    private final Map<Structure, StructureStart> kStarts = new java.util.concurrent.ConcurrentHashMap<>();
    private final ShortList[] kPostProcessing = new ShortList[32];

    protected final ChunkPos chunkPos;
    protected final LevelChunkSection[] sections;
    protected volatile boolean unsaved;
    protected volatile boolean lightCorrect;
    private final LevelHeightAccessor heightAccessor;
    protected UpgradeData upgradeData;
    private long inhabitedTime;

    public ChunkAccess(ChunkPos chunkPos, UpgradeData upgradeData, LevelHeightAccessor levelHeightAccessor, PalettedContainerFactory palettedContainerFactory, long value, LevelChunkSection[] levelChunkSections, BlendingData blendingData) {
        this.chunkPos = chunkPos;
        this.sections = levelChunkSections;
        this.heightAccessor = levelHeightAccessor;
        this.upgradeData = upgradeData;
        this.inhabitedTime = value;
    }
    public void addEntity(Entity entity) {
        throw Unsupported.hit("net.minecraft.world.level.chunk.ChunkAccess.addEntity");
    }
    public void addPackedPostProcess(ShortList packedOffsets, int sectionIndex) {
        getOrCreateOffsetList(kPostProcessing, sectionIndex).addAll(packedOffsets);
    }
    public void addReferenceForStructure(Structure structure, long reference) {
        getReferencesForStructure(structure).add(reference);
    }
    public boolean canBeSerialized() {
        return true;
    }
    public BiomeGenerationSettings carverBiome(Supplier source) {
        throw Unsupported.hit("net.minecraft.world.level.chunk.ChunkAccess.carverBiome");
    }
    public BlockHitResult clip(ClipContext clipContext) {
        return heightAccessor instanceof net.minecraft.world.level.BlockGetter getter ? getter.clip(clipContext)
            : BlockHitResult.miss(clipContext.getTo(), net.minecraft.core.Direction.UP, BlockPos.containing(clipContext.getTo()));
    }
    public BlockHitResult clip(ClipContext clipContext, Predicate predicate) {
        return clip(clipContext);
    }
    public BlockHitResult clip(ClipContext clipContext, BlockPos blockPos) {
        return clip(clipContext);
    }
    public BlockHitResult clip(ClipContext clipContext, BlockPos blockPos, Predicate predicate) {
        return clip(clipContext);
    }
    public BlockHitResult clipWithInteractionOverride(Vec3 vec3, Vec3 vec32, BlockPos blockPos, VoxelShape voxelShape, BlockState blockState) {
        return voxelShape == null ? null : voxelShape.clip(vec3, vec32, blockPos);
    }
    public void fillBiomesFromNoise(BiomeResolver biomeResolver) {
        throw Unsupported.hit("net.minecraft.world.level.chunk.ChunkAccess.fillBiomesFromNoise");
    }
    public void findBlocks(Predicate<net.minecraft.world.level.block.state.BlockState> predicate, BiConsumer<net.minecraft.core.BlockPos, net.minecraft.world.level.block.state.BlockState> consumer) {
        for (int y = getMinY(); y <= getMaxY(); y++) {
            for (int z = 0; z < 16; z++) {
                for (int x = 0; x < 16; x++) {
                    BlockPos at = new BlockPos(getPos().getMinBlockX() + x, y, getPos().getMinBlockZ() + z);
                    net.minecraft.world.level.block.state.BlockState state = getBlockState(at);
                    if (predicate.test(state)) {
                        consumer.accept(at, state);
                    }
                }
            }
        }
    }
    public Map<net.minecraft.world.level.levelgen.structure.Structure, it.unimi.dsi.fastutil.longs.LongSet> getAllReferences() {
        return kReferences;
    }
    public Map<net.minecraft.world.level.levelgen.structure.Structure, net.minecraft.world.level.levelgen.structure.StructureStart> getAllStarts() {
        return java.util.Collections.unmodifiableMap(kStarts);
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
        BlockEntity found = getBlockEntity(blockPos);
        return found != null && found.getType() == blockEntityType ? Optional.of(found) : Optional.empty();
    }
    public CompoundTag getBlockEntityNbt(BlockPos blockPos) {
        throw Unsupported.hit("net.minecraft.world.level.chunk.ChunkAccess.getBlockEntityNbt");
    }
    public CompoundTag getBlockEntityNbtForSaving(BlockPos blockPos, HolderLookup.Provider registryAccess) {
        throw Unsupported.hit("net.minecraft.world.level.chunk.ChunkAccess.getBlockEntityNbtForSaving");
    }
    public double getBlockFloorHeight(BlockPos blockPos) {
        return getBlockFloorHeight(getBlockState(blockPos).getCollisionShape(this, blockPos),
            () -> getBlockState(blockPos.below()).getCollisionShape(this, blockPos.below()));
    }
    public double getBlockFloorHeight(VoxelShape voxelShape, Supplier supplier) {
        if (!voxelShape.isEmpty()) {
            return voxelShape.max(net.minecraft.core.Direction.Axis.Y);
        }
        VoxelShape below = (VoxelShape) supplier.get();
        return below.isEmpty() ? Double.NEGATIVE_INFINITY : below.max(net.minecraft.core.Direction.Axis.Y) - 1.0;
    }
    public Block getBlockIfLoaded(BlockPos blockPos) {
        return getBlockState(blockPos).getBlock();
    }
    public BlockState getBlockState(int value, int value2, int value3) {
        return getBlockState(new BlockPos(value, value2, value3));
    }
    public BlockState getBlockState(BlockPos blockPos) {
        throw Unsupported.hit("net.minecraft.world.level.chunk.ChunkAccess.getBlockState");
    }
    public BlockState getBlockStateIfLoaded(BlockPos blockPos) {
        return getBlockState(blockPos);
    }
    public Stream getBlockStates(AABB aABB) {
        java.util.List<BlockState> found = new java.util.ArrayList<>();
        for (int x = (int) Math.floor(aABB.minX); x <= (int) Math.floor(aABB.maxX); x++) {
            for (int y = (int) Math.floor(aABB.minY); y <= (int) Math.floor(aABB.maxY); y++) {
                for (int z = (int) Math.floor(aABB.minZ); z <= (int) Math.floor(aABB.maxZ); z++) {
                    found.add(getBlockState(new BlockPos(x, y, z)));
                }
            }
        }
        return found.stream();
    }
    public TickContainerAccess getBlockTicks() {
        throw Unsupported.hit("net.minecraft.world.level.chunk.ChunkAccess.getBlockTicks");
    }
    public FluidState getFluidIfLoaded(BlockPos blockPos) {
        return getBlockState(blockPos).getFluidState();
    }
    public FluidState getFluidState(BlockPos blockPos) {
        return getBlockState(blockPos).getFluidState();
    }
    public TickContainerAccess getFluidTicks() {
        throw Unsupported.hit("net.minecraft.world.level.chunk.ChunkAccess.getFluidTicks");
    }
    public int getHeight() {
        return heightAccessor != null ? heightAccessor.getHeight() : sections.length * 16;
    }
    public int getHeight(Heightmap.Types type, int x, int z) {
        throw Unsupported.hit("net.minecraft.world.level.chunk.ChunkAccess.getHeight");
    }
    public LevelHeightAccessor getHeightAccessorForGeneration() {
        return this;
    }
    public Collection getHeightmaps() {
        java.util.List<Heightmap> maps = new java.util.ArrayList<>();
        for (Heightmap.Types type : Heightmap.Types.values()) {
            maps.add(getOrCreateHeightmapUnprimed(type));
        }
        return maps;
    }
    public int getHighestFilledSectionIndex() {
        for (int i = sections.length - 1; i >= 0; i--) {
            if (sections[i] != null && !sections[i].hasOnlyAir()) {
                return i;
            }
        }
        return -1;
    }
    public ChunkStatus getHighestGeneratedStatus() {
        return getPersistedStatus();
    }
    public int getHighestSectionPosition() {
        int index = getHighestFilledSectionIndex();
        return index == -1 ? getMinY() : getSectionYFromSectionIndex(index) << 4;
    }
    public long getInhabitedTime() {
        return inhabitedTime;
    }
    public int getLightEmission(BlockPos blockPos) {
        return getBlockState(blockPos).getLightEmission();
    }
    public GameEventListenerRegistry getListenerRegistry(int section) {
        throw Unsupported.hit("net.minecraft.world.level.chunk.ChunkAccess.getListenerRegistry");
    }
    public int getMaxSectionY() {
        return getMaxY() >> 4;
    }
    public int getMaxY() {
        return getMinY() + getHeight() - 1;
    }
    public int getMinSectionY() {
        return getMinY() >> 4;
    }
    public int getMinY() {
        return heightAccessor != null ? heightAccessor.getMinY() : 0;
    }
    public Holder getNoiseBiome(int quartX, int quartY, int quartZ) {
        if (heightAccessor instanceof net.minecraft.world.level.LevelReader reader) {
            return reader.getNoiseBiome(quartX, quartY, quartZ);
        }
        throw Unsupported.hit("net.minecraft.world.level.chunk.ChunkAccess.getNoiseBiome");
    }
    public Heightmap getOrCreateHeightmapUnprimed(Heightmap.Types type) {
        if (this instanceof LevelChunk chunk) {
            return (Heightmap) chunk.heightmaps.get(type);
        }
        throw new UnsupportedOperationException("Heightmaps are bound to a level chunk");
    }
    public ChunkStatus getPersistedStatus() {
        throw Unsupported.hit("net.minecraft.world.level.chunk.ChunkAccess.getPersistedStatus");
    }
    public ChunkPos getPos() {
        return chunkPos;
    }
    public ShortList[] getPostProcessing() {
        return kPostProcessing;
    }
    public LongSet getReferencesForStructure(Structure structure) {
        return kReferences.computeIfAbsent(structure, ignored -> new it.unimi.dsi.fastutil.longs.LongOpenHashSet());
    }
    public LevelChunkSection getSection(int sectionIndex) {
        return getSections()[sectionIndex];
    }
    public int getSectionIndex(int value) {
        return getSectionIndexFromSectionY(value >> 4);
    }
    public int getSectionIndexFromSectionY(int value) {
        return value - getMinSectionY();
    }
    public int getSectionYFromSectionIndex(int value) {
        return value + getMinSectionY();
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
        return kStarts.get(structure);
    }
    public ChunkAccess.PackedTicks getTicksForSerialization(long currentTick) {
        throw Unsupported.hit("net.minecraft.world.level.chunk.ChunkAccess.getTicksForSerialization");
    }
    public UpgradeData getUpgradeData() {
        return upgradeData;
    }
    public boolean hasPrimedHeightmap(Heightmap.Types type) {
        return this instanceof LevelChunk;
    }
    public void incrementInhabitedTime() {
        inhabitedTime++;
    }
    public void initializeLightSources() {
        return;
    }
    public BlockHitResult isBlockInLine(ClipBlockStateContext clipBlockStateContext) {
        throw Unsupported.hit("net.minecraft.world.level.chunk.ChunkAccess.isBlockInLine");
    }
    public boolean isInsideBuildHeight(int value) {
        return value >= getMinY() && value <= getMaxY();
    }
    public boolean isLightCorrect() {
        return lightCorrect;
    }
    public boolean isOldNoiseGeneration() {
        return false;
    }
    public boolean isOutsideBuildHeight(int value) {
        return !isInsideBuildHeight(value);
    }
    public boolean isOutsideBuildHeight(BlockPos blockPos) {
        return isOutsideBuildHeight(blockPos.getY());
    }
    public boolean isUnsaved() {
        return unsaved;
    }
    public boolean isUpgrading() {
        return false;
    }
    public boolean isYSpaceEmpty(int yStartInclusive, int yEndInclusive) {
        int from = getSectionIndex(yStartInclusive);
        int to = getSectionIndex(yEndInclusive);
        for (int i = Math.max(0, from); i <= Math.min(sections.length - 1, to); i++) {
            if (sections[i] != null && !sections[i].hasOnlyAir()) {
                return false;
            }
        }
        return true;
    }
    public void markUnsaved() {
        unsaved = true;
    }
    public ProblemReporter.PathElement problemPath() {
        return problemPath(chunkPos);
    }
    public void removeBlockEntity(BlockPos pos) {

    }
    public void setAllReferences(Map<net.minecraft.world.level.levelgen.structure.Structure, it.unimi.dsi.fastutil.longs.LongSet> data) {
        kReferences.clear();
        kReferences.putAll(data);
    }
    public void setAllStarts(Map<net.minecraft.world.level.levelgen.structure.Structure, net.minecraft.world.level.levelgen.structure.StructureStart> starts) {
        kStarts.clear();
        kStarts.putAll(starts);
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
        this.inhabitedTime = inhabitedTime;
    }
    public void setLightCorrect(boolean isLightCorrect) {
        lightCorrect = isLightCorrect;
    }
    public void setStartForStructure(Structure structure, StructureStart structureStart) {
        kStarts.put(structure, structureStart);
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
        return true;
    }

    public record PackedTicks(List blocks, List fluids) {
    }
    public static final int NO_FILLED_SECTION = -1;
    protected final ShortList[] postProcessing = null;
    protected final BlendingData blendingData = null;
    protected final Map<Heightmap.Types, Heightmap> heightmaps = null;
    protected ChunkSkyLightSources skyLightSources;
    protected final Map<BlockPos, CompoundTag> pendingBlockEntities = null;
    protected final Map<BlockPos, BlockEntity> blockEntities = null;
    protected final LevelHeightAccessor levelHeightAccessor = null;
    public void markPosForPostProcessing(BlockPos blockPos) {
        int section = (blockPos.getY() - getMinY()) >> 4;
        getOrCreateOffsetList(kPostProcessing, section).add((short) ((blockPos.getX() & 15) | ((blockPos.getY() & 15) << 4)
            | ((blockPos.getZ() & 15) << 8)));
    }
    public void findBlockLightSources(BiConsumer<BlockPos, BlockState> consumer) {
        findBlocks(state -> state.getLightEmission() > 0, consumer);
    }
    public void collectBiomesInPalette(Set<Holder<Biome>> output) {
        int minQuartY = net.minecraft.core.QuartPos.fromBlock(getMinY());
        int maxQuartY = net.minecraft.core.QuartPos.fromBlock(getMaxY());
        for (int qx = 0; qx < 4; qx++) {
            for (int qz = 0; qz < 4; qz++) {
                for (int qy = minQuartY; qy <= maxQuartY; qy++) {
                    output.add(getNoiseBiome(chunkPos.x() * 4 + qx, qy, chunkPos.z() * 4 + qz));
                }
            }
        }
    }
    public static ShortList getOrCreateOffsetList(ShortList[] list, int sectionIndex) {
        if (list[sectionIndex] == null) {
            list[sectionIndex] = new it.unimi.dsi.fastutil.shorts.ShortArrayList();
        }
        return list[sectionIndex];
    }
    public static ProblemReporter.PathElement problemPath(ChunkPos pos) {
        return new ChunkPathElement(pos);
    }
    static record ChunkPathElement(ChunkPos pos) implements ProblemReporter.PathElement {
        public String get() {
            return "chunk@" + pos;
        }
    }
}
