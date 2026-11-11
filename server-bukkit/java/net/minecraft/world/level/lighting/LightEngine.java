// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.lighting;
import net.minecraft.world.phys.shapes.Shapes;
import net.minecraft.core.BlockPos;
import net.minecraft.core.Direction;
import net.minecraft.core.SectionPos;
import net.minecraft.world.level.ChunkPos;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.chunk.DataLayer;
import net.minecraft.world.level.chunk.LightChunk;
import net.minecraft.world.level.chunk.LightChunkGetter;
import net.minecraft.world.phys.shapes.VoxelShape;
public abstract class LightEngine<M extends DataLayerStorageMap<M>, S extends LayerLightSectionStorage<M>> implements LayerLightEventListener {
    public static final int MAX_LEVEL = 15;
    protected static final int MIN_OPACITY = 1;
    protected static final long PULL_LIGHT_IN_ENTRY = 0;
    protected static final Direction[] PROPAGATION_DIRECTIONS = Direction.values();
    protected final LightChunkGetter chunkSource;
    protected final S storage;
    private String worldName() {
        return chunkSource != null && chunkSource.getLevel() instanceof net.minecraft.world.level.Level level ? level.getWorld().getName() : null;
    }
    private int nativeLayer() {
        return this instanceof SkyLightEngine ? 1 : 0;
    }
    protected LightEngine(LightChunkGetter chunkSource, S storage) {
        this.chunkSource = chunkSource;
        this.storage = storage;
    }
    public static boolean hasDifferentLightProperties(BlockState oldState, BlockState newState) {
        return oldState != newState && (oldState.getLightDampening() != newState.getLightDampening()
            || oldState.getLightEmission() != newState.getLightEmission()
            || oldState.useShapeForLightOcclusion() || newState.useShapeForLightOcclusion());
    }
    public static int getLightDampeningInto(BlockState fromState, BlockState toState, Direction direction, int simpleOpacity) {
        boolean shaped = fromState.useShapeForLightOcclusion() || toState.useShapeForLightOcclusion();
        if (!shaped) {
            return simpleOpacity;
        }
        VoxelShape out = getOcclusionShape(fromState, direction);
        VoxelShape in = getOcclusionShape(toState, direction.getOpposite());
        return Shapes.faceShapeOccludes(out, in) ? 15 : simpleOpacity;
    }
    public static VoxelShape getOcclusionShape(BlockState state, Direction direction) {
        return state.canOcclude() ? state.getFaceOcclusionShape(direction) : Shapes.empty();
    }
    protected static boolean isEmptyShape(BlockState state) {
        return !state.canOcclude() || !state.useShapeForLightOcclusion();
    }
    protected BlockState getState(BlockPos pos) {
        return chunkSource.getLevel().getBlockState(pos);
    }
    protected int getOpacity(BlockState state) {
        return Math.max(MIN_OPACITY, state.getLightDampening());
    }
    protected boolean shapeOccludes(BlockState fromState, BlockState toState, Direction direction) {
        return Shapes.faceShapeOccludes(getOcclusionShape(fromState, direction), getOcclusionShape(toState, direction.getOpposite()));
    }
    protected LightChunk getChunk(int chunkX, int chunkZ) {
        return chunkSource.getChunkForLighting(chunkX, chunkZ);
    }
    public void checkBlock(BlockPos pos) {
        String world = worldName();
        if (world != null) {
            org.basaltmc.bukkit.WorldNatives.relightChunks(world, new int[] {pos.getX() >> 4, pos.getZ() >> 4});
        }
    }
    public void queueSectionData(long pos, DataLayer data) {
    }
    public void retainData(ChunkPos pos, boolean retain) {
    }
    public void updateSectionStatus(SectionPos pos, boolean sectionEmpty) {
    }
    public void setLightEnabled(ChunkPos pos, boolean enable) {
    }
    public int runLightUpdates() {
        return 0;
    }
    protected void enqueueDecrease(long fromNode, long decreaseData) {
    }
    protected void enqueueIncrease(long fromNode, long increaseData) {
    }
    public boolean hasLightWork() {
        return false;
    }
    public DataLayer getDataLayerData(SectionPos pos) {
        String world = worldName();
        byte[] bytes = world == null ? null
            : org.basaltmc.bukkit.WorldNatives.readLight(world, pos.x(), pos.y(), pos.z(), nativeLayer(), false);
        return bytes == null ? null : new DataLayer(bytes);
    }
    public int getLightValue(BlockPos pos) {
        String world = worldName();
        return world == null ? 0 : Math.max(0, org.basaltmc.bukkit.WorldNatives.lightAt(world, pos.getX(), pos.getY(), pos.getZ(), nativeLayer()));
    }
    public String getDebugData(long sectionNode) {
        return "n/a";
    }
    public LayerLightSectionStorage.SectionType getDebugSectionType(long sectionNode) {
        return LayerLightSectionStorage.SectionType.EMPTY;
    }
    protected abstract void checkNode(long blockNode);
    protected abstract void propagateIncrease(long fromNode, long increaseData, int fromLevel);
    protected abstract void propagateDecrease(long fromNode, long decreaseData);
    public static class QueueEntry {
        private static final long kEmpty = 1L << 4;
        private static final long kEmission = 1L << 5;
        private static final long kAllDirections = 63L << 6;
        private static long level(int value) {
            return value & 15L;
        }
        private static long skip(Direction direction) {
            return kAllDirections & ~(1L << (6 + direction.ordinal()));
        }
        public static long decreaseSkipOneDirection(int oldFromLevel, Direction skipDirection) {
            return level(oldFromLevel) | skip(skipDirection);
        }
        public static long decreaseAllDirections(int oldFromLevel) {
            return level(oldFromLevel) | kAllDirections;
        }
        public static long increaseLightFromEmission(int newFromLevel, boolean fromEmptyShape) {
            return level(newFromLevel) | kEmission | (fromEmptyShape ? kEmpty : 0L);
        }
        public static long increaseSkipOneDirection(int newFromLevel, boolean fromEmptyShape, Direction skipDirection) {
            return level(newFromLevel) | (fromEmptyShape ? kEmpty : 0L) | (kAllDirections & ~(1L << (6 + skipDirection.ordinal())));
        }
        public static long increaseOnlyOneDirection(int newFromLevel, boolean fromEmptyShape, Direction direction) {
            return level(newFromLevel) | (fromEmptyShape ? kEmpty : 0L) | (1L << (6 + direction.ordinal()));
        }
        public static long increaseSkySourceInDirections(boolean down, boolean north, boolean south, boolean west, boolean east) {
            long mask = 0L;
            mask |= down ? 1L << (6 + Direction.DOWN.ordinal()) : 0L;
            mask |= north ? 1L << (6 + Direction.NORTH.ordinal()) : 0L;
            mask |= south ? 1L << (6 + Direction.SOUTH.ordinal()) : 0L;
            mask |= west ? 1L << (6 + Direction.WEST.ordinal()) : 0L;
            mask |= east ? 1L << (6 + Direction.EAST.ordinal()) : 0L;
            return level(15) | mask;
        }
        public static int getFromLevel(long entry) {
            return (int) (entry & 15L);
        }
        public static boolean isFromEmptyShape(long entry) {
            return (entry & kEmpty) != 0L;
        }
        public static boolean isIncreaseFromEmission(long entry) {
            return (entry & kEmission) != 0L;
        }
        public static boolean shouldPropagateInDirection(long entry, Direction direction) {
            return (entry & (1L << (6 + direction.ordinal()))) != 0L;
        }
    }
}
