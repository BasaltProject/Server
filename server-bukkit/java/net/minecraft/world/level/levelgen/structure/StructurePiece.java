// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.structure;

import java.util.List;
import java.util.Objects;
import java.util.stream.Stream;
import net.minecraft.core.BlockPos;
import net.minecraft.core.Direction;
import net.minecraft.nbt.CompoundTag;
import net.minecraft.resources.ResourceKey;
import net.minecraft.util.RandomSource;
import net.minecraft.world.level.BlockGetter;
import net.minecraft.world.level.ChunkPos;
import net.minecraft.world.level.LevelReader;
import net.minecraft.world.level.ServerLevelAccessor;
import net.minecraft.world.level.StructureManager;
import net.minecraft.world.level.WorldGenLevel;
import net.minecraft.world.level.block.Blocks;
import net.minecraft.world.level.block.Mirror;
import net.minecraft.world.level.block.Rotation;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.chunk.ChunkGenerator;
import net.minecraft.world.level.levelgen.structure.pieces.StructurePieceSerializationContext;
import net.minecraft.world.level.levelgen.structure.pieces.StructurePieceType;
import net.minecraft.world.level.levelgen.structure.pieces.StructurePiecesBuilder;
import net.minecraft.world.level.storage.loot.LootTable;

public abstract class StructurePiece {
    protected static final BlockState CAVE_AIR = Blocks.CAVE_AIR.defaultBlockState();
    private final StructurePieceType type;
    protected BoundingBox boundingBox;
    protected int genDepth;
    private Direction orientation;
    private Mirror mirror = Mirror.NONE;
    private Rotation rotation = Rotation.NONE;

    protected StructurePiece(StructurePieceType type, int genDepth, BoundingBox boundingBox) {
        this.type = type;
        this.genDepth = genDepth;
        this.boundingBox = boundingBox;
    }

    public StructurePiece(StructurePieceType type, CompoundTag tag) {
        this(type, tag.getIntOr("GD", 0), BoundingBox.fromCorners(new BlockPos(tag.getIntArray("BB").map(box -> box.length == 6 ? box[0] : 0).orElse(0),
            tag.getIntArray("BB").map(box -> box.length == 6 ? box[1] : 0).orElse(0), tag.getIntArray("BB").map(box -> box.length == 6 ? box[2] : 0).orElse(0)),
            new BlockPos(tag.getIntArray("BB").map(box -> box.length == 6 ? box[3] : 0).orElse(0),
                tag.getIntArray("BB").map(box -> box.length == 6 ? box[4] : 0).orElse(0), tag.getIntArray("BB").map(box -> box.length == 6 ? box[5] : 0).orElse(0))));
        int facing = tag.getIntOr("O", -1);
        setOrientation(facing == -1 ? null : Direction.from2DDataValue(facing));
    }

    protected static BoundingBox makeBoundingBox(int x, int y, int z, Direction direction, int width, int height, int depth) {
        boolean alongX = direction.getAxis() == Direction.Axis.Z;
        return alongX ? new BoundingBox(x, y, z, x + width - 1, y + height - 1, z + depth - 1)
            : new BoundingBox(x, y, z, x + depth - 1, y + height - 1, z + width - 1);
    }

    protected static Direction getRandomHorizontalDirection(RandomSource random) {
        return Direction.Plane.HORIZONTAL.getRandomDirection(random);
    }

    public final CompoundTag createTag(StructurePieceSerializationContext context) {
        CompoundTag tag = new CompoundTag();
        tag.putInt("GD", genDepth);
        tag.putIntArray("BB", new int[] {boundingBox.minX(), boundingBox.minY(), boundingBox.minZ(), boundingBox.maxX(), boundingBox.maxY(), boundingBox.maxZ()});
        tag.putInt("O", orientation == null ? -1 : orientation.get2DDataValue());
        addAdditionalSaveData(context, tag);
        return tag;
    }

    protected abstract void addAdditionalSaveData(StructurePieceSerializationContext context, CompoundTag tag);

    public void addChildren(StructurePiece startPiece, StructurePiecesBuilder builder, RandomSource random) {
        return;
    }

    public abstract void postProcess(WorldGenLevel level, StructureManager structureManager, ChunkGenerator generator, RandomSource random, BoundingBox chunkBB, ChunkPos chunkPos, BlockPos referencePos);

    public BoundingBox getBoundingBox() {
        return boundingBox;
    }

    public int getGenDepth() {
        return genDepth;
    }

    public void setGenDepth(int genDepth) {
        this.genDepth = genDepth;
    }

    public boolean isCloseToChunk(ChunkPos pos, int distance) {
        return boundingBox.intersects(pos.getMinBlockX() - distance, pos.getMinBlockZ() - distance, pos.getMaxBlockX() + distance, pos.getMaxBlockZ() + distance);
    }

    public BlockPos getLocatorPosition() {
        return boundingBox.getCenter();
    }

    protected BlockPos.MutableBlockPos getWorldPos(int x, int y, int z) {
        return new BlockPos.MutableBlockPos(getWorldX(x, z), getWorldY(y), getWorldZ(x, z));
    }

    protected int getWorldX(int x, int z) {
        if (orientation == null) {
            return x;
        }
        return switch (orientation) {
            case NORTH, SOUTH -> boundingBox.minX() + x;
            case WEST -> boundingBox.maxX() - z;
            case EAST -> boundingBox.minX() + z;
            default -> x;
        };
    }

    protected int getWorldY(int y) {
        return orientation == null ? y : y + boundingBox.minY();
    }

    protected int getWorldZ(int x, int z) {
        if (orientation == null) {
            return z;
        }
        return switch (orientation) {
            case NORTH -> boundingBox.maxZ() - z;
            case SOUTH -> boundingBox.minZ() + z;
            case WEST, EAST -> boundingBox.minZ() + x;
            default -> z;
        };
    }

    protected void placeBlock(WorldGenLevel level, BlockState blockState, int x, int y, int z, BoundingBox chunkBB) {
        BlockPos pos = getWorldPos(x, y, z);
        if (!chunkBB.isInside(pos)) {
            return;
        }
        BlockState placed = blockState;
        if (mirror != Mirror.NONE) {
            placed = placed.mirror(mirror);
        }
        if (rotation != Rotation.NONE) {
            placed = placed.rotate(rotation);
        }
        level.setBlock(pos, placed, 2);
    }

    protected boolean canBeReplaced(LevelReader level, int x, int y, int z, BoundingBox chunkBB) {
        return true;
    }

    protected BlockState getBlock(BlockGetter level, int x, int y, int z, BoundingBox chunkBB) {
        BlockPos pos = getWorldPos(x, y, z);
        return chunkBB.isInside(pos) ? level.getBlockState(pos) : Blocks.AIR.defaultBlockState();
    }

    protected boolean isInterior(LevelReader level, int x, int y, int z, BoundingBox chunkBB) {
        BlockPos pos = getWorldPos(x, y + 1, z);
        return chunkBB.isInside(pos) && pos.getY() < level.getHeight(net.minecraft.world.level.levelgen.Heightmap.Types.OCEAN_FLOOR_WG, pos.getX(), pos.getZ());
    }

    protected void generateAirBox(WorldGenLevel level, BoundingBox chunkBB, int x0, int y0, int z0, int x1, int y1, int z1) {
        generateBox(level, chunkBB, x0, y0, z0, x1, y1, z1, Blocks.AIR.defaultBlockState(), Blocks.AIR.defaultBlockState(), false);
    }

    protected void generateBox(WorldGenLevel level, BoundingBox chunkBB, int x0, int y0, int z0, int x1, int y1, int z1, BlockState edgeBlock, BlockState fillBlock, boolean skipAir) {
        for (int y = y0; y <= y1; y++) {
            for (int x = x0; x <= x1; x++) {
                for (int z = z0; z <= z1; z++) {
                    if (skipAir && getBlock(level, x, y, z, chunkBB).isAir()) {
                        continue;
                    }
                    boolean onEdge = y == y0 || y == y1 || x == x0 || x == x1 || z == z0 || z == z1;
                    placeBlock(level, onEdge ? edgeBlock : fillBlock, x, y, z, chunkBB);
                }
            }
        }
    }

    protected void generateBox(WorldGenLevel level, BoundingBox chunkBB, BoundingBox boxBB, BlockState edgeBlock, BlockState fillBlock, boolean skipAir) {
        generateBox(level, chunkBB, boxBB.minX(), boxBB.minY(), boxBB.minZ(), boxBB.maxX(), boxBB.maxY(), boxBB.maxZ(), edgeBlock, fillBlock, skipAir);
    }

    protected void generateBox(WorldGenLevel level, BoundingBox chunkBB, int x0, int y0, int z0, int x1, int y1, int z1, boolean skipAir, RandomSource random, BlockSelector selector) {
        for (int y = y0; y <= y1; y++) {
            for (int x = x0; x <= x1; x++) {
                for (int z = z0; z <= z1; z++) {
                    if (skipAir && getBlock(level, x, y, z, chunkBB).isAir()) {
                        continue;
                    }
                    selector.next(random, x, y, z, y == y0 || y == y1 || x == x0 || x == x1 || z == z0 || z == z1);
                    placeBlock(level, selector.getNext(), x, y, z, chunkBB);
                }
            }
        }
    }

    protected void generateBox(WorldGenLevel level, BoundingBox chunkBB, BoundingBox boxBB, boolean skipAir, RandomSource random, BlockSelector selector) {
        generateBox(level, chunkBB, boxBB.minX(), boxBB.minY(), boxBB.minZ(), boxBB.maxX(), boxBB.maxY(), boxBB.maxZ(), skipAir, random, selector);
    }

    protected void generateMaybeBox(WorldGenLevel level, BoundingBox chunkBB, RandomSource random, float probability, int x0, int y0, int z0, int x1, int y1, int z1, BlockState edgeBlock, BlockState fillBlock, boolean skipAir, boolean hasToBeInside) {
        for (int y = y0; y <= y1; y++) {
            for (int x = x0; x <= x1; x++) {
                for (int z = z0; z <= z1; z++) {
                    if (random.nextFloat() > probability || skipAir && getBlock(level, x, y, z, chunkBB).isAir()
                        || hasToBeInside && !isInterior(level, x, y, z, chunkBB)) {
                        continue;
                    }
                    placeBlock(level, y == y0 || y == y1 || x == x0 || x == x1 || z == z0 || z == z1 ? edgeBlock : fillBlock, x, y, z, chunkBB);
                }
            }
        }
    }

    protected void maybeGenerateBlock(WorldGenLevel level, BoundingBox chunkBB, RandomSource random, float probability, int x, int y, int z, BlockState blockState) {
        if (random.nextFloat() < probability) {
            placeBlock(level, blockState, x, y, z, chunkBB);
        }
    }

    protected void generateUpperHalfSphere(WorldGenLevel level, BoundingBox chunkBB, int x0, int y0, int z0, int x1, int y1, int z1, BlockState fillBlock, boolean skipAir) {
        float spanX = x1 - x0 + 1;
        float spanY = y1 - y0 + 1;
        float spanZ = z1 - z0 + 1;
        float centerX = x0 + spanX / 2.0F;
        float centerZ = z0 + spanZ / 2.0F;
        for (int y = y0; y <= y1; y++) {
            float dy = (y - y0) / spanY;
            for (int x = x0; x <= x1; x++) {
                float dx = (x - centerX) / (spanX * 0.5F);
                for (int z = z0; z <= z1; z++) {
                    float dz = (z - centerZ) / (spanZ * 0.5F);
                    if ((!skipAir || !getBlock(level, x, y, z, chunkBB).isAir()) && dx * dx + dy * dy + dz * dz <= 1.05F) {
                        placeBlock(level, fillBlock, x, y, z, chunkBB);
                    }
                }
            }
        }
    }

    protected void fillColumnDown(WorldGenLevel level, BlockState blockState, int x, int startY, int z, BoundingBox chunkBB) {
        BlockPos.MutableBlockPos pos = getWorldPos(x, startY, z);
        if (!chunkBB.isInside(pos)) {
            return;
        }
        while (isReplaceableByStructures(level.getBlockState(pos)) && pos.getY() > level.getMinY() + 1) {
            level.setBlock(pos, blockState, 2);
            pos.move(Direction.DOWN);
        }
    }

    protected boolean isReplaceableByStructures(BlockState state) {
        return state.isAir() || state.liquid() || state.is(Blocks.GLOW_LICHEN) || state.is(Blocks.SEAGRASS) || state.is(Blocks.TALL_SEAGRASS);
    }

    protected boolean createChest(WorldGenLevel level, BoundingBox chunkBB, RandomSource random, int x, int y, int z, ResourceKey<LootTable> lootTable) {
        return createChest(level, chunkBB, random, getWorldPos(x, y, z), lootTable, null);
    }

    public static BlockState reorient(BlockGetter level, BlockPos blockPos, BlockState blockState) {
        Direction facing = null;
        for (Direction side : Direction.Plane.HORIZONTAL) {
            BlockPos neighbour = blockPos.relative(side);
            BlockState there = level.getBlockState(neighbour);
            if (there.is(Blocks.CHEST)) {
                return blockState;
            }
            if (there.isSolidRender()) {
                if (facing != null) {
                    facing = null;
                    break;
                }
                facing = side;
            }
        }
        if (facing != null) {
            return blockState.setValue(net.minecraft.world.level.block.HorizontalDirectionalBlock.FACING, facing.getOpposite());
        }
        Direction current = blockState.getValue(net.minecraft.world.level.block.HorizontalDirectionalBlock.FACING);
        BlockPos ahead = blockPos.relative(current);
        if (level.getBlockState(ahead).isSolidRender()) {
            current = current.getOpposite();
            ahead = blockPos.relative(current);
        }
        if (level.getBlockState(ahead).isSolidRender()) {
            current = current.getClockWise();
            ahead = blockPos.relative(current);
        }
        if (level.getBlockState(ahead).isSolidRender()) {
            current = current.getOpposite();
        }
        return blockState.setValue(net.minecraft.world.level.block.HorizontalDirectionalBlock.FACING, current);
    }

    protected boolean createChest(ServerLevelAccessor level, BoundingBox chunkBB, RandomSource random, BlockPos pos, ResourceKey<LootTable> lootTable, BlockState blockState) {
        if (!chunkBB.isInside(pos) || level.getBlockState(pos).is(Blocks.CHEST)) {
            return false;
        }
        BlockState chest = blockState == null ? reorient(level, pos, Blocks.CHEST.defaultBlockState()) : blockState;
        level.setBlock(pos, chest, 2);
        if (level.getBlockEntity(pos) instanceof net.minecraft.world.level.block.entity.RandomizableContainerBlockEntity container) {
            container.setLootTable(lootTable, random.nextLong());
        }
        return true;
    }

    protected boolean createDispenser(WorldGenLevel level, BoundingBox chunkBB, RandomSource random, int x, int y, int z, Direction facing, ResourceKey<LootTable> lootTable) {
        BlockPos pos = getWorldPos(x, y, z);
        if (!chunkBB.isInside(pos) || level.getBlockState(pos).is(Blocks.DISPENSER)) {
            return false;
        }
        level.setBlock(pos, Blocks.DISPENSER.defaultBlockState().setValue(net.minecraft.world.level.block.DispenserBlock.FACING, facing), 2);
        if (level.getBlockEntity(pos) instanceof net.minecraft.world.level.block.entity.RandomizableContainerBlockEntity container) {
            container.setLootTable(lootTable, random.nextLong());
        }
        return true;
    }

    public void move(int dx, int dy, int dz) {
        boundingBox.move(dx, dy, dz);
    }

    public static BoundingBox createBoundingBox(Stream<StructurePiece> pieces) {
        return BoundingBox.encapsulatingBoxes(pieces.map(StructurePiece::getBoundingBox).toList()).orElseThrow(() -> new IllegalStateException("Unable to calculate boundingbox without pieces"));
    }

    public static StructurePiece findCollisionPiece(List<StructurePiece> pieces, BoundingBox box) {
        for (StructurePiece piece : pieces) {
            if (piece.getBoundingBox().intersects(box)) {
                return piece;
            }
        }
        return null;
    }

    public Direction getOrientation() {
        return orientation;
    }

    public void setOrientation(Direction orientation) {
        this.orientation = orientation;
        if (orientation == null) {
            rotation = Rotation.NONE;
            mirror = Mirror.NONE;
            return;
        }
        switch (orientation) {
            case SOUTH -> {
                mirror = Mirror.LEFT_RIGHT;
                rotation = Rotation.NONE;
            }
            case WEST -> {
                mirror = Mirror.LEFT_RIGHT;
                rotation = Rotation.CLOCKWISE_90;
            }
            case EAST -> {
                mirror = Mirror.NONE;
                rotation = Rotation.CLOCKWISE_90;
            }
            default -> {
                mirror = Mirror.NONE;
                rotation = Rotation.NONE;
            }
        }
    }

    public Rotation getRotation() {
        return rotation;
    }

    public Mirror getMirror() {
        return mirror;
    }

    public StructurePieceType getType() {
        return type;
    }

    public abstract static class BlockSelector {
        protected BlockState next = Blocks.AIR.defaultBlockState();

        public BlockSelector() {}

        public abstract void next(RandomSource random, int worldX, int worldY, int worldZ, boolean isEdge);

        public BlockState getNext() {
            return next;
        }
    }
}
