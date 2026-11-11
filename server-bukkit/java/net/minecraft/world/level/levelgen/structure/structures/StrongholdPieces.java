// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.structure.structures;

import com.mojang.serialization.Codec;
import java.util.ArrayList;
import java.util.List;
import net.minecraft.core.BlockPos;
import net.minecraft.core.Direction;
import net.minecraft.nbt.CompoundTag;
import net.minecraft.util.RandomSource;
import net.minecraft.world.entity.EntityTypes;
import net.minecraft.world.level.ChunkPos;
import net.minecraft.world.level.StructureManager;
import net.minecraft.world.level.WorldGenLevel;
import net.minecraft.world.level.block.Block;
import net.minecraft.world.level.block.Blocks;
import net.minecraft.world.level.block.entity.BlockEntity;
import net.minecraft.world.level.block.entity.SpawnerBlockEntity;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.block.state.properties.BlockStateProperties;
import net.minecraft.world.level.block.state.properties.DoubleBlockHalf;
import net.minecraft.world.level.block.state.properties.SlabType;
import net.minecraft.world.level.chunk.ChunkGenerator;
import net.minecraft.world.level.levelgen.structure.BoundingBox;
import net.minecraft.world.level.levelgen.structure.StructurePiece;
import net.minecraft.world.level.levelgen.structure.pieces.StructurePieceSerializationContext;
import net.minecraft.world.level.levelgen.structure.pieces.StructurePieceType;
import net.minecraft.world.level.levelgen.structure.pieces.StructurePiecesBuilder;
import net.minecraft.world.level.storage.loot.BuiltInLootTables;

public class StrongholdPieces {
    public static final int MAGIC_START_Y = 64;
    private static final int kMaxDepth = 50;
    private static final int kMaxReach = 112;
    private static final int kLowestY = 10;

    private static final PieceWeight[] kWeights = {new PieceWeight(Straight.class, 40, 0),
        new PieceWeight(PrisonHall.class, 5, 5), new PieceWeight(LeftTurn.class, 20, 0),
        new PieceWeight(RightTurn.class, 20, 0), new PieceWeight(RoomCrossing.class, 10, 6),
        new PieceWeight(StraightStairsDown.class, 5, 5), new PieceWeight(StairsDown.class, 5, 5),
        new PieceWeight(FiveCrossing.class, 5, 4), new PieceWeight(ChestCorridor.class, 5, 4),
        new PieceWeight(Library.class, 10, 2) {
            public boolean doPlace(int depth) {
                return super.doPlace(depth) && depth > 4;
            }
        }, new PieceWeight(PortalRoom.class, 20, 1) {
            public boolean doPlace(int depth) {
                return super.doPlace(depth) && depth > 5;
            }
        }};
    private static List<PieceWeight> currentPieces;
    private static Class<? extends StrongholdPiece> imposedPiece;
    private static int totalWeight;
    private static final SmoothStoneSelector kSelector = new SmoothStoneSelector();

    private static BlockState st(Block block) {
        return block.defaultBlockState();
    }

    private static BlockState torch(Direction facing) {
        return st(Blocks.WALL_TORCH).setValue(BlockStateProperties.HORIZONTAL_FACING, facing);
    }

    private static BlockState bars(boolean n, boolean s, boolean e, boolean w) {
        return st(Blocks.IRON_BARS).setValue(BlockStateProperties.NORTH, n).setValue(BlockStateProperties.SOUTH, s)
            .setValue(BlockStateProperties.EAST, e).setValue(BlockStateProperties.WEST, w);
    }

    private static BlockState fence(boolean n, boolean s, boolean e, boolean w) {
        return st(Blocks.OAK_FENCE).setValue(BlockStateProperties.NORTH, n).setValue(BlockStateProperties.SOUTH, s)
            .setValue(BlockStateProperties.EAST, e).setValue(BlockStateProperties.WEST, w);
    }

    public static void resetPieces() {
        currentPieces = new ArrayList<>();
        for (PieceWeight weight : kWeights) {
            weight.placeCount = 0;
            currentPieces.add(weight);
        }
        imposedPiece = null;
    }

    private static boolean updatePieceWeight() {
        boolean anyLimited = false;
        totalWeight = 0;
        for (PieceWeight weight : currentPieces) {
            if (weight.maxPlaceCount > 0 && weight.placeCount < weight.maxPlaceCount) {
                anyLimited = true;
            }
            totalWeight += weight.weight;
        }
        return anyLimited;
    }

    private static StrongholdPiece findAndCreatePieceFactory(Class<? extends StrongholdPiece> pieceClass,
            StructurePiecesBuilder builder, RandomSource random, int x, int y, int z, Direction dir, int depth) {
        if (pieceClass == Straight.class) {
            return Straight.createPiece(builder, random, x, y, z, dir, depth);
        } else if (pieceClass == PrisonHall.class) {
            return PrisonHall.createPiece(builder, random, x, y, z, dir, depth);
        } else if (pieceClass == LeftTurn.class) {
            return LeftTurn.createPiece(builder, random, x, y, z, dir, depth);
        } else if (pieceClass == RightTurn.class) {
            return RightTurn.createPiece(builder, random, x, y, z, dir, depth);
        } else if (pieceClass == RoomCrossing.class) {
            return RoomCrossing.createPiece(builder, random, x, y, z, dir, depth);
        } else if (pieceClass == StraightStairsDown.class) {
            return StraightStairsDown.createPiece(builder, random, x, y, z, dir, depth);
        } else if (pieceClass == StairsDown.class) {
            return StairsDown.createPiece(builder, random, x, y, z, dir, depth);
        } else if (pieceClass == FiveCrossing.class) {
            return FiveCrossing.createPiece(builder, random, x, y, z, dir, depth);
        } else if (pieceClass == ChestCorridor.class) {
            return ChestCorridor.createPiece(builder, random, x, y, z, dir, depth);
        } else if (pieceClass == Library.class) {
            return Library.createPiece(builder, random, x, y, z, dir, depth);
        } else if (pieceClass == PortalRoom.class) {
            return PortalRoom.createPiece(builder, x, y, z, dir, depth);
        }
        return null;
    }

    private static StrongholdPiece generatePieceFromSmallDoor(StartPiece start, StructurePiecesBuilder builder,
            RandomSource random, int x, int y, int z, Direction dir, int depth) {
        if (!updatePieceWeight()) {
            return null;
        }
        if (imposedPiece != null) {
            StrongholdPiece forced = findAndCreatePieceFactory(imposedPiece, builder, random, x, y, z, dir, depth);
            imposedPiece = null;
            if (forced != null) {
                return forced;
            }
        }
        for (int attempt = 0; attempt < 5; attempt++) {
            int roll = random.nextInt(totalWeight);
            for (PieceWeight weight : currentPieces) {
                roll -= weight.weight;
                if (roll >= 0) {
                    continue;
                }
                if (!weight.doPlace(depth) || weight == start.previousPiece) {
                    break;
                }
                StrongholdPiece made = findAndCreatePieceFactory(weight.pieceClass, builder, random, x, y, z, dir, depth);
                if (made != null) {
                    weight.placeCount++;
                    start.previousPiece = weight;
                    if (!weight.isValid()) {
                        currentPieces.remove(weight);
                    }
                    return made;
                }
            }
        }
        BoundingBox filler = FillerCorridor.findPieceBox(builder, random, x, y, z, dir);
        return filler != null && filler.minY() > 1 ? new FillerCorridor(depth, filler, dir) : null;
    }

    private static StructurePiece generateAndAddPiece(StartPiece start, StructurePiecesBuilder builder,
            RandomSource random, int x, int y, int z, Direction dir, int depth) {
        if (depth > kMaxDepth) {
            return null;
        }
        BoundingBox origin = start.getBoundingBox();
        if (Math.abs(x - origin.minX()) > kMaxReach || Math.abs(z - origin.minZ()) > kMaxReach) {
            return null;
        }
        StructurePiece piece = generatePieceFromSmallDoor(start, builder, random, x, y, z, dir, depth + 1);
        if (piece != null) {
            builder.addPiece(piece);
            start.pendingChildren.add(piece);
        }
        return piece;
    }

    static class PieceWeight {
        public final Class<? extends StrongholdPiece> pieceClass;
        public final int weight;
        public int placeCount;
        public final int maxPlaceCount;

        public PieceWeight(Class<? extends StrongholdPiece> pieceClass, int weight, int maxPlaceCount) {
            this.pieceClass = pieceClass;
            this.weight = weight;
            this.maxPlaceCount = maxPlaceCount;
        }

        public boolean doPlace(int depth) {
            return maxPlaceCount == 0 || placeCount < maxPlaceCount;
        }

        public boolean isValid() {
            return maxPlaceCount == 0 || placeCount < maxPlaceCount;
        }
    }

    static abstract class StrongholdPiece extends StructurePiece {
        protected SmallDoorType entryDoor = SmallDoorType.OPENING;

        protected StrongholdPiece(StructurePieceType type, int genDepth, BoundingBox boundingBox) {
            super(type, genDepth, boundingBox);
        }

        public StrongholdPiece(StructurePieceType type, CompoundTag tag) {
            super(type, tag);
            String name = tag.getStringOr("EntryDoor", "OPENING");
            try {
                entryDoor = SmallDoorType.valueOf(name);
            } catch (IllegalArgumentException ignored) {
                entryDoor = SmallDoorType.OPENING;
            }
        }

        protected void addAdditionalSaveData(StructurePieceSerializationContext context, CompoundTag tag) {
            tag.putString("EntryDoor", entryDoor.name());
        }

        protected void generateSmallDoor(WorldGenLevel level, RandomSource random, BoundingBox chunkBB,
                SmallDoorType doorType, int x, int y, int z) {
            BlockState brick = st(Blocks.STONE_BRICKS);
            switch (doorType) {
                case OPENING -> generateBox(level, chunkBB, x, y, z, x + 2, y + 2, z, CAVE_AIR, CAVE_AIR, false);
                case WOOD_DOOR, IRON_DOOR -> {
                    boolean iron = doorType == SmallDoorType.IRON_DOOR;
                    placeFrame(level, chunkBB, brick, x, y, z);
                    BlockState door = st(iron ? Blocks.IRON_DOOR : Blocks.OAK_DOOR);
                    placeBlock(level, door, x + 1, y, z, chunkBB);
                    placeBlock(level, door.setValue(BlockStateProperties.DOUBLE_BLOCK_HALF, DoubleBlockHalf.UPPER),
                        x + 1, y + 1, z, chunkBB);
                    if (iron) {
                        BlockState button = st(Blocks.STONE_BUTTON);
                        placeBlock(level, button.setValue(BlockStateProperties.HORIZONTAL_FACING, Direction.NORTH),
                            x + 2, y + 1, z + 1, chunkBB);
                        placeBlock(level, button.setValue(BlockStateProperties.HORIZONTAL_FACING, Direction.SOUTH),
                            x + 2, y + 1, z - 1, chunkBB);
                    }
                }
                case GRATES -> {
                    placeBlock(level, CAVE_AIR, x + 1, y, z, chunkBB);
                    placeBlock(level, CAVE_AIR, x + 1, y + 1, z, chunkBB);
                    placeBlock(level, bars(false, false, false, true), x, y, z, chunkBB);
                    placeBlock(level, bars(false, false, false, true), x, y + 1, z, chunkBB);
                    placeBlock(level, bars(false, false, true, true), x, y + 2, z, chunkBB);
                    placeBlock(level, bars(false, false, true, true), x + 1, y + 2, z, chunkBB);
                    placeBlock(level, bars(false, false, true, true), x + 2, y + 2, z, chunkBB);
                    placeBlock(level, bars(false, false, true, false), x + 2, y + 1, z, chunkBB);
                    placeBlock(level, bars(false, false, true, false), x + 2, y, z, chunkBB);
                }
            }
        }

        private void placeFrame(WorldGenLevel level, BoundingBox chunkBB, BlockState brick, int x, int y, int z) {
            int[][] cells = {{0, 0}, {0, 1}, {0, 2}, {1, 2}, {2, 2}, {2, 1}, {2, 0}};
            for (int[] c : cells) {
                placeBlock(level, brick, x + c[0], y + c[1], z, chunkBB);
            }
        }

        protected SmallDoorType randomSmallDoor(RandomSource random) {
            return switch (random.nextInt(5)) {
                case 2 -> SmallDoorType.WOOD_DOOR;
                case 3 -> SmallDoorType.GRATES;
                case 4 -> SmallDoorType.IRON_DOOR;
                default -> SmallDoorType.OPENING;
            };
        }

        protected StructurePiece generateSmallDoorChildForward(StartPiece startPiece, StructurePiecesBuilder builder,
                RandomSource random, int xOff, int yOff) {
            Direction facing = getOrientation();
            if (facing == null) {
                return null;
            }
            BoundingBox bb = boundingBox;
            int y = bb.minY() + yOff;
            return switch (facing) {
                case NORTH -> generateAndAddPiece(startPiece, builder, random, bb.minX() + xOff, y, bb.minZ() - 1,
                    facing, getGenDepth());
                case SOUTH -> generateAndAddPiece(startPiece, builder, random, bb.minX() + xOff, y, bb.maxZ() + 1,
                    facing, getGenDepth());
                case WEST -> generateAndAddPiece(startPiece, builder, random, bb.minX() - 1, y, bb.minZ() + xOff,
                    facing, getGenDepth());
                case EAST -> generateAndAddPiece(startPiece, builder, random, bb.maxX() + 1, y, bb.minZ() + xOff,
                    facing, getGenDepth());
                default -> null;
            };
        }

        protected StructurePiece generateSmallDoorChildLeft(StartPiece startPiece, StructurePiecesBuilder builder,
                RandomSource random, int yOff, int zOff) {
            Direction facing = getOrientation();
            if (facing == null) {
                return null;
            }
            BoundingBox bb = boundingBox;
            int y = bb.minY() + yOff;
            return switch (facing) {
                case NORTH, SOUTH -> generateAndAddPiece(startPiece, builder, random, bb.minX() - 1, y,
                    bb.minZ() + zOff, Direction.WEST, getGenDepth());
                case WEST, EAST -> generateAndAddPiece(startPiece, builder, random, bb.minX() + zOff, y,
                    bb.minZ() - 1, Direction.NORTH, getGenDepth());
                default -> null;
            };
        }

        protected StructurePiece generateSmallDoorChildRight(StartPiece startPiece, StructurePiecesBuilder builder,
                RandomSource random, int yOff, int zOff) {
            Direction facing = getOrientation();
            if (facing == null) {
                return null;
            }
            BoundingBox bb = boundingBox;
            int y = bb.minY() + yOff;
            return switch (facing) {
                case NORTH, SOUTH -> generateAndAddPiece(startPiece, builder, random, bb.maxX() + 1, y,
                    bb.minZ() + zOff, Direction.EAST, getGenDepth());
                case WEST, EAST -> generateAndAddPiece(startPiece, builder, random, bb.minX() + zOff, y,
                    bb.maxZ() + 1, Direction.SOUTH, getGenDepth());
                default -> null;
            };
        }

        protected static boolean isOkBox(BoundingBox box) {
            return box.minY() > kLowestY;
        }

        protected static boolean fits(StructurePiecesBuilder builder, BoundingBox box) {
            return isOkBox(box) && builder.findCollisionPiece(box) == null;
        }

        protected static enum SmallDoorType {
            OPENING, WOOD_DOOR, GRATES, IRON_DOOR;

            public static final Codec<SmallDoorType> LEGACY_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(
                SmallDoorType.class, "LEGACY_CODEC");
        }
    }

    public static class FillerCorridor extends StrongholdPiece {
        private final int steps;

        public FillerCorridor(int genDepth, BoundingBox boundingBox, Direction direction) {
            super(StructurePieceType.STRONGHOLD_FILLER_CORRIDOR, genDepth, boundingBox);
            setOrientation(direction);
            steps = direction != Direction.NORTH && direction != Direction.SOUTH ? boundingBox.getXSpan()
                : boundingBox.getZSpan();
        }

        public FillerCorridor(CompoundTag tag) {
            super(StructurePieceType.STRONGHOLD_FILLER_CORRIDOR, tag);
            steps = tag.getIntOr("Steps", 0);
        }

        protected void addAdditionalSaveData(StructurePieceSerializationContext context, CompoundTag tag) {
            super.addAdditionalSaveData(context, tag);
            tag.putInt("Steps", steps);
        }

        public static BoundingBox findPieceBox(StructurePiecesBuilder builder, RandomSource random, int footX,
                int footY, int footZ, Direction direction) {
            BoundingBox box = BoundingBox.orientBox(footX, footY, footZ, -1, -1, 0, 5, 5, 4, direction);
            StructurePiece hit = builder.findCollisionPiece(box);
            if (hit == null || hit.getBoundingBox().minY() != box.minY()) {
                return null;
            }
            for (int depth = 2; depth >= 1; depth--) {
                BoundingBox shorter = BoundingBox.orientBox(footX, footY, footZ, -1, -1, 0, 5, 5, depth, direction);
                if (!hit.getBoundingBox().intersects(shorter)) {
                    return BoundingBox.orientBox(footX, footY, footZ, -1, -1, 0, 5, 5, depth + 1, direction);
                }
            }
            return null;
        }

        public void postProcess(WorldGenLevel level, StructureManager structureManager, ChunkGenerator generator,
                RandomSource random, BoundingBox chunkBB, ChunkPos chunkPos, BlockPos referencePos) {
            BlockState brick = st(Blocks.STONE_BRICKS);
            for (int i = 0; i < steps; i++) {
                for (int x = 0; x <= 4; x++) {
                    placeBlock(level, brick, x, 0, i, chunkBB);
                    placeBlock(level, brick, x, 4, i, chunkBB);
                }
                for (int y = 1; y <= 3; y++) {
                    placeBlock(level, brick, 0, y, i, chunkBB);
                    placeBlock(level, CAVE_AIR, 1, y, i, chunkBB);
                    placeBlock(level, CAVE_AIR, 2, y, i, chunkBB);
                    placeBlock(level, CAVE_AIR, 3, y, i, chunkBB);
                    placeBlock(level, brick, 4, y, i, chunkBB);
                }
            }
        }
    }

    public static class StairsDown extends StrongholdPiece {
        private boolean isSource;

        public StairsDown(StructurePieceType type, int genDepth, int west, int north, Direction direction) {
            super(type, genDepth, makeBoundingBox(west, 64, north, direction, 5, 11, 5));
            isSource = true;
            setOrientation(direction);
            entryDoor = SmallDoorType.OPENING;
        }

        public StairsDown(int genDepth, RandomSource random, BoundingBox boundingBox, Direction direction) {
            super(StructurePieceType.STRONGHOLD_STAIRS_DOWN, genDepth, boundingBox);
            isSource = false;
            setOrientation(direction);
            entryDoor = randomSmallDoor(random);
        }

        public StairsDown(StructurePieceType type, CompoundTag tag) {
            super(type, tag);
            isSource = tag.getBooleanOr("Source", false);
        }

        public StairsDown(CompoundTag tag) {
            this(StructurePieceType.STRONGHOLD_STAIRS_DOWN, tag);
        }

        protected void addAdditionalSaveData(StructurePieceSerializationContext context, CompoundTag tag) {
            super.addAdditionalSaveData(context, tag);
            tag.putBoolean("Source", isSource);
        }

        public void addChildren(StructurePiece startPiece, StructurePiecesBuilder builder, RandomSource random) {
            if (isSource) {
                imposedPiece = FiveCrossing.class;
            }
            generateSmallDoorChildForward((StartPiece) startPiece, builder, random, 1, 1);
        }

        public static StairsDown createPiece(StructurePiecesBuilder builder, RandomSource random, int footX, int footY,
                int footZ, Direction direction, int genDepth) {
            BoundingBox box = BoundingBox.orientBox(footX, footY, footZ, -1, -7, 0, 5, 11, 5, direction);
            return fits(builder, box) ? new StairsDown(genDepth, random, box, direction) : null;
        }

        public void postProcess(WorldGenLevel level, StructureManager structureManager, ChunkGenerator generator,
                RandomSource random, BoundingBox chunkBB, ChunkPos chunkPos, BlockPos referencePos) {
            generateBox(level, chunkBB, 0, 0, 0, 4, 10, 4, true, random, kSelector);
            generateSmallDoor(level, random, chunkBB, entryDoor, 1, 7, 0);
            generateSmallDoor(level, random, chunkBB, SmallDoorType.OPENING, 1, 1, 4);
            BlockState brick = st(Blocks.STONE_BRICKS);
            BlockState slab = st(Blocks.SMOOTH_STONE_SLAB);
            int[][] bricks = {{2, 6, 1}, {1, 5, 1}, {1, 5, 2}, {1, 4, 3}, {2, 4, 3}, {3, 3, 3}, {3, 3, 2}, {3, 2, 1},
                {2, 2, 1}, {1, 1, 1}, {1, 1, 2}};
            int[][] slabs = {{1, 6, 1}, {1, 5, 3}, {3, 4, 3}, {3, 3, 1}, {1, 2, 1}, {1, 1, 3}};
            for (int[] c : bricks) {
                placeBlock(level, brick, c[0], c[1], c[2], chunkBB);
            }
            for (int[] c : slabs) {
                placeBlock(level, slab, c[0], c[1], c[2], chunkBB);
            }
        }
    }

    public static class StartPiece extends StairsDown {
        public PieceWeight previousPiece;
        public PortalRoom portalRoomPiece;
        public final List<StructurePiece> pendingChildren = new ArrayList<>();

        public StartPiece(RandomSource random, int west, int north) {
            super(StructurePieceType.STRONGHOLD_START, 0, west, north, getRandomHorizontalDirection(random));
        }

        public StartPiece(CompoundTag tag) {
            super(StructurePieceType.STRONGHOLD_START, tag);
        }

        public BlockPos getLocatorPosition() {
            return portalRoomPiece != null ? portalRoomPiece.getLocatorPosition() : super.getLocatorPosition();
        }
    }

    public static class Straight extends StrongholdPiece {
        private boolean leftChild;
        private boolean rightChild;

        public Straight(int genDepth, RandomSource random, BoundingBox boundingBox, Direction direction) {
            super(StructurePieceType.STRONGHOLD_STRAIGHT, genDepth, boundingBox);
            setOrientation(direction);
            entryDoor = randomSmallDoor(random);
            leftChild = random.nextInt(2) == 0;
            rightChild = random.nextInt(2) == 0;
        }

        public Straight(CompoundTag tag) {
            super(StructurePieceType.STRONGHOLD_STRAIGHT, tag);
            leftChild = tag.getBooleanOr("Left", false);
            rightChild = tag.getBooleanOr("Right", false);
        }

        protected void addAdditionalSaveData(StructurePieceSerializationContext context, CompoundTag tag) {
            super.addAdditionalSaveData(context, tag);
            tag.putBoolean("Left", leftChild);
            tag.putBoolean("Right", rightChild);
        }

        public void addChildren(StructurePiece startPiece, StructurePiecesBuilder builder, RandomSource random) {
            StartPiece start = (StartPiece) startPiece;
            generateSmallDoorChildForward(start, builder, random, 1, 1);
            if (leftChild) {
                generateSmallDoorChildLeft(start, builder, random, 1, 2);
            }
            if (rightChild) {
                generateSmallDoorChildRight(start, builder, random, 1, 2);
            }
        }

        public static Straight createPiece(StructurePiecesBuilder builder, RandomSource random, int footX, int footY,
                int footZ, Direction direction, int genDepth) {
            BoundingBox box = BoundingBox.orientBox(footX, footY, footZ, -1, -1, 0, 5, 5, 7, direction);
            return fits(builder, box) ? new Straight(genDepth, random, box, direction) : null;
        }

        public void postProcess(WorldGenLevel level, StructureManager structureManager, ChunkGenerator generator,
                RandomSource random, BoundingBox chunkBB, ChunkPos chunkPos, BlockPos referencePos) {
            generateBox(level, chunkBB, 0, 0, 0, 4, 4, 6, true, random, kSelector);
            generateSmallDoor(level, random, chunkBB, entryDoor, 1, 1, 0);
            generateSmallDoor(level, random, chunkBB, SmallDoorType.OPENING, 1, 1, 6);
            BlockState east = torch(Direction.EAST);
            BlockState west = torch(Direction.WEST);
            maybeGenerateBlock(level, chunkBB, random, 0.1F, 1, 2, 1, east);
            maybeGenerateBlock(level, chunkBB, random, 0.1F, 3, 2, 1, west);
            maybeGenerateBlock(level, chunkBB, random, 0.1F, 1, 2, 5, east);
            maybeGenerateBlock(level, chunkBB, random, 0.1F, 3, 2, 5, west);
            if (leftChild) {
                generateBox(level, chunkBB, 0, 1, 2, 0, 3, 4, CAVE_AIR, CAVE_AIR, false);
            }
            if (rightChild) {
                generateBox(level, chunkBB, 4, 1, 2, 4, 3, 4, CAVE_AIR, CAVE_AIR, false);
            }
        }
    }

    public static class ChestCorridor extends StrongholdPiece {
        private boolean hasPlacedChest;

        public ChestCorridor(int genDepth, RandomSource random, BoundingBox boundingBox, Direction direction) {
            super(StructurePieceType.STRONGHOLD_CHEST_CORRIDOR, genDepth, boundingBox);
            setOrientation(direction);
            entryDoor = randomSmallDoor(random);
        }

        public ChestCorridor(CompoundTag tag) {
            super(StructurePieceType.STRONGHOLD_CHEST_CORRIDOR, tag);
            hasPlacedChest = tag.getBooleanOr("Chest", false);
        }

        protected void addAdditionalSaveData(StructurePieceSerializationContext context, CompoundTag tag) {
            super.addAdditionalSaveData(context, tag);
            tag.putBoolean("Chest", hasPlacedChest);
        }

        public void addChildren(StructurePiece startPiece, StructurePiecesBuilder builder, RandomSource random) {
            generateSmallDoorChildForward((StartPiece) startPiece, builder, random, 1, 1);
        }

        public static ChestCorridor createPiece(StructurePiecesBuilder builder, RandomSource random, int footX,
                int footY, int footZ, Direction direction, int genDepth) {
            BoundingBox box = BoundingBox.orientBox(footX, footY, footZ, -1, -1, 0, 5, 5, 7, direction);
            return fits(builder, box) ? new ChestCorridor(genDepth, random, box, direction) : null;
        }

        public void postProcess(WorldGenLevel level, StructureManager structureManager, ChunkGenerator generator,
                RandomSource random, BoundingBox chunkBB, ChunkPos chunkPos, BlockPos referencePos) {
            generateBox(level, chunkBB, 0, 0, 0, 4, 4, 6, true, random, kSelector);
            generateSmallDoor(level, random, chunkBB, entryDoor, 1, 1, 0);
            generateSmallDoor(level, random, chunkBB, SmallDoorType.OPENING, 1, 1, 6);
            BlockState brick = st(Blocks.STONE_BRICKS);
            BlockState slab = st(Blocks.STONE_BRICK_SLAB);
            generateBox(level, chunkBB, 3, 1, 2, 3, 1, 4, brick, brick, false);
            placeBlock(level, slab, 3, 1, 1, chunkBB);
            placeBlock(level, slab, 3, 1, 5, chunkBB);
            placeBlock(level, slab, 3, 2, 2, chunkBB);
            placeBlock(level, slab, 3, 2, 4, chunkBB);
            for (int z = 2; z <= 4; z++) {
                placeBlock(level, slab, 2, 1, z, chunkBB);
            }
            if (!hasPlacedChest && chunkBB.isInside(getWorldPos(3, 2, 3))) {
                hasPlacedChest = true;
                createChest(level, chunkBB, random, 3, 2, 3, BuiltInLootTables.STRONGHOLD_CORRIDOR);
            }
        }
    }

    public static class StraightStairsDown extends StrongholdPiece {
        public StraightStairsDown(int genDepth, RandomSource random, BoundingBox boundingBox, Direction direction) {
            super(StructurePieceType.STRONGHOLD_STRAIGHT_STAIRS_DOWN, genDepth, boundingBox);
            setOrientation(direction);
            entryDoor = randomSmallDoor(random);
        }

        public StraightStairsDown(CompoundTag tag) {
            super(StructurePieceType.STRONGHOLD_STRAIGHT_STAIRS_DOWN, tag);
        }

        public void addChildren(StructurePiece startPiece, StructurePiecesBuilder builder, RandomSource random) {
            generateSmallDoorChildForward((StartPiece) startPiece, builder, random, 1, 1);
        }

        public static StraightStairsDown createPiece(StructurePiecesBuilder builder, RandomSource random, int footX,
                int footY, int footZ, Direction direction, int genDepth) {
            BoundingBox box = BoundingBox.orientBox(footX, footY, footZ, -1, -7, 0, 5, 11, 8, direction);
            return fits(builder, box) ? new StraightStairsDown(genDepth, random, box, direction) : null;
        }

        public void postProcess(WorldGenLevel level, StructureManager structureManager, ChunkGenerator generator,
                RandomSource random, BoundingBox chunkBB, ChunkPos chunkPos, BlockPos referencePos) {
            generateBox(level, chunkBB, 0, 0, 0, 4, 10, 7, true, random, kSelector);
            generateSmallDoor(level, random, chunkBB, entryDoor, 1, 7, 0);
            generateSmallDoor(level, random, chunkBB, SmallDoorType.OPENING, 1, 1, 7);
            BlockState stairs = st(Blocks.COBBLESTONE_STAIRS).setValue(BlockStateProperties.HORIZONTAL_FACING,
                Direction.SOUTH);
            BlockState brick = st(Blocks.STONE_BRICKS);
            for (int i = 0; i < 6; i++) {
                for (int x = 1; x <= 3; x++) {
                    placeBlock(level, stairs, x, 6 - i, 1 + i, chunkBB);
                    if (i < 5) {
                        placeBlock(level, brick, x, 5 - i, 1 + i, chunkBB);
                    }
                }
            }
        }
    }

    public static abstract class Turn extends StrongholdPiece {
        protected Turn(StructurePieceType type, int genDepth, BoundingBox boundingBox) {
            super(type, genDepth, boundingBox);
        }

        public Turn(StructurePieceType type, CompoundTag tag) {
            super(type, tag);
        }

        protected boolean doorOnLowSide() {
            Direction facing = getOrientation();
            return facing == Direction.NORTH || facing == Direction.EAST;
        }
    }

    public static class LeftTurn extends Turn {
        public LeftTurn(int genDepth, RandomSource random, BoundingBox boundingBox, Direction direction) {
            super(StructurePieceType.STRONGHOLD_LEFT_TURN, genDepth, boundingBox);
            setOrientation(direction);
            entryDoor = randomSmallDoor(random);
        }

        public LeftTurn(CompoundTag tag) {
            super(StructurePieceType.STRONGHOLD_LEFT_TURN, tag);
        }

        public void addChildren(StructurePiece startPiece, StructurePiecesBuilder builder, RandomSource random) {
            if (doorOnLowSide()) {
                generateSmallDoorChildLeft((StartPiece) startPiece, builder, random, 1, 1);
            } else {
                generateSmallDoorChildRight((StartPiece) startPiece, builder, random, 1, 1);
            }
        }

        public static LeftTurn createPiece(StructurePiecesBuilder builder, RandomSource random, int footX, int footY,
                int footZ, Direction direction, int genDepth) {
            BoundingBox box = BoundingBox.orientBox(footX, footY, footZ, -1, -1, 0, 5, 5, 5, direction);
            return fits(builder, box) ? new LeftTurn(genDepth, random, box, direction) : null;
        }

        public void postProcess(WorldGenLevel level, StructureManager structureManager, ChunkGenerator generator,
                RandomSource random, BoundingBox chunkBB, ChunkPos chunkPos, BlockPos referencePos) {
            generateBox(level, chunkBB, 0, 0, 0, 4, 4, 4, true, random, kSelector);
            generateSmallDoor(level, random, chunkBB, entryDoor, 1, 1, 0);
            int wallX = doorOnLowSide() ? 0 : 4;
            generateBox(level, chunkBB, wallX, 1, 1, wallX, 3, 3, CAVE_AIR, CAVE_AIR, false);
        }
    }

    public static class RightTurn extends Turn {
        public RightTurn(int genDepth, RandomSource random, BoundingBox boundingBox, Direction direction) {
            super(StructurePieceType.STRONGHOLD_RIGHT_TURN, genDepth, boundingBox);
            setOrientation(direction);
            entryDoor = randomSmallDoor(random);
        }

        public RightTurn(CompoundTag tag) {
            super(StructurePieceType.STRONGHOLD_RIGHT_TURN, tag);
        }

        public void addChildren(StructurePiece startPiece, StructurePiecesBuilder builder, RandomSource random) {
            if (doorOnLowSide()) {
                generateSmallDoorChildRight((StartPiece) startPiece, builder, random, 1, 1);
            } else {
                generateSmallDoorChildLeft((StartPiece) startPiece, builder, random, 1, 1);
            }
        }

        public static RightTurn createPiece(StructurePiecesBuilder builder, RandomSource random, int footX, int footY,
                int footZ, Direction direction, int genDepth) {
            BoundingBox box = BoundingBox.orientBox(footX, footY, footZ, -1, -1, 0, 5, 5, 5, direction);
            return fits(builder, box) ? new RightTurn(genDepth, random, box, direction) : null;
        }

        public void postProcess(WorldGenLevel level, StructureManager structureManager, ChunkGenerator generator,
                RandomSource random, BoundingBox chunkBB, ChunkPos chunkPos, BlockPos referencePos) {
            generateBox(level, chunkBB, 0, 0, 0, 4, 4, 4, true, random, kSelector);
            generateSmallDoor(level, random, chunkBB, entryDoor, 1, 1, 0);
            int wallX = doorOnLowSide() ? 4 : 0;
            generateBox(level, chunkBB, wallX, 1, 1, wallX, 3, 3, CAVE_AIR, CAVE_AIR, false);
        }
    }

    public static class RoomCrossing extends StrongholdPiece {
        protected int type;

        public RoomCrossing(int genDepth, RandomSource random, BoundingBox boundingBox, Direction direction) {
            super(StructurePieceType.STRONGHOLD_ROOM_CROSSING, genDepth, boundingBox);
            setOrientation(direction);
            entryDoor = randomSmallDoor(random);
            type = random.nextInt(5);
        }

        public RoomCrossing(CompoundTag tag) {
            super(StructurePieceType.STRONGHOLD_ROOM_CROSSING, tag);
            type = tag.getIntOr("Type", 0);
        }

        protected void addAdditionalSaveData(StructurePieceSerializationContext context, CompoundTag tag) {
            super.addAdditionalSaveData(context, tag);
            tag.putInt("Type", type);
        }

        public void addChildren(StructurePiece startPiece, StructurePiecesBuilder builder, RandomSource random) {
            StartPiece start = (StartPiece) startPiece;
            generateSmallDoorChildForward(start, builder, random, 4, 1);
            generateSmallDoorChildLeft(start, builder, random, 1, 4);
            generateSmallDoorChildRight(start, builder, random, 1, 4);
        }

        public static RoomCrossing createPiece(StructurePiecesBuilder builder, RandomSource random, int footX,
                int footY, int footZ, Direction direction, int genDepth) {
            BoundingBox box = BoundingBox.orientBox(footX, footY, footZ, -4, -1, 0, 11, 7, 11, direction);
            return fits(builder, box) ? new RoomCrossing(genDepth, random, box, direction) : null;
        }

        private void put(WorldGenLevel level, BoundingBox chunkBB, BlockState state, int[][] cells) {
            for (int[] c : cells) {
                placeBlock(level, state, c[0], c[1], c[2], chunkBB);
            }
        }

        public void postProcess(WorldGenLevel level, StructureManager structureManager, ChunkGenerator generator,
                RandomSource random, BoundingBox chunkBB, ChunkPos chunkPos, BlockPos referencePos) {
            generateBox(level, chunkBB, 0, 0, 0, 10, 6, 10, true, random, kSelector);
            generateSmallDoor(level, random, chunkBB, entryDoor, 4, 1, 0);
            generateBox(level, chunkBB, 4, 1, 10, 6, 3, 10, CAVE_AIR, CAVE_AIR, false);
            generateBox(level, chunkBB, 0, 1, 4, 0, 3, 6, CAVE_AIR, CAVE_AIR, false);
            generateBox(level, chunkBB, 10, 1, 4, 10, 3, 6, CAVE_AIR, CAVE_AIR, false);
            BlockState brick = st(Blocks.STONE_BRICKS);
            BlockState cobble = st(Blocks.COBBLESTONE);
            BlockState planks = st(Blocks.OAK_PLANKS);
            switch (type) {
                case 0 -> {
                    put(level, chunkBB, brick, new int[][] {{5, 1, 5}, {5, 2, 5}, {5, 3, 5}});
                    placeBlock(level, torch(Direction.WEST), 4, 3, 5, chunkBB);
                    placeBlock(level, torch(Direction.EAST), 6, 3, 5, chunkBB);
                    placeBlock(level, torch(Direction.SOUTH), 5, 3, 4, chunkBB);
                    placeBlock(level, torch(Direction.NORTH), 5, 3, 6, chunkBB);
                    put(level, chunkBB, st(Blocks.SMOOTH_STONE_SLAB), new int[][] {{4, 1, 4}, {4, 1, 5}, {4, 1, 6},
                        {6, 1, 4}, {6, 1, 5}, {6, 1, 6}, {5, 1, 4}, {5, 1, 6}});
                }
                case 1 -> {
                    for (int i = 0; i < 5; i++) {
                        put(level, chunkBB, brick, new int[][] {{3, 1, 3 + i}, {7, 1, 3 + i}, {3 + i, 1, 3},
                            {3 + i, 1, 7}});
                    }
                    put(level, chunkBB, brick, new int[][] {{5, 1, 5}, {5, 2, 5}, {5, 3, 5}});
                    placeBlock(level, st(Blocks.WATER), 5, 4, 5, chunkBB);
                }
                case 2 -> {
                    for (int i = 1; i <= 9; i++) {
                        put(level, chunkBB, cobble, new int[][] {{1, 3, i}, {9, 3, i}, {i, 3, 1}, {i, 3, 9}});
                    }
                    put(level, chunkBB, cobble, new int[][] {{5, 1, 4}, {5, 1, 6}, {5, 3, 4}, {5, 3, 6}, {4, 1, 5},
                        {6, 1, 5}, {4, 3, 5}, {6, 3, 5}});
                    for (int y = 1; y <= 3; y++) {
                        put(level, chunkBB, cobble, new int[][] {{4, y, 4}, {6, y, 4}, {4, y, 6}, {6, y, 6}});
                    }
                    placeBlock(level, st(Blocks.WALL_TORCH), 5, 3, 5, chunkBB);
                    for (int z = 2; z <= 8; z++) {
                        put(level, chunkBB, planks, new int[][] {{2, 3, z}, {3, 3, z}});
                        if (z <= 3 || z >= 7) {
                            put(level, chunkBB, planks, new int[][] {{4, 3, z}, {5, 3, z}, {6, 3, z}});
                        }
                        put(level, chunkBB, planks, new int[][] {{7, 3, z}, {8, 3, z}});
                    }
                    BlockState ladder = st(Blocks.LADDER).setValue(BlockStateProperties.HORIZONTAL_FACING,
                        Direction.WEST);
                    put(level, chunkBB, ladder, new int[][] {{9, 1, 3}, {9, 2, 3}, {9, 3, 3}});
                    createChest(level, chunkBB, random, 3, 4, 8, BuiltInLootTables.STRONGHOLD_CROSSING);
                }
                default -> {
                }
            }
        }
    }

    public static class PrisonHall extends StrongholdPiece {
        public PrisonHall(int genDepth, RandomSource random, BoundingBox boundingBox, Direction direction) {
            super(StructurePieceType.STRONGHOLD_PRISON_HALL, genDepth, boundingBox);
            setOrientation(direction);
            entryDoor = randomSmallDoor(random);
        }

        public PrisonHall(CompoundTag tag) {
            super(StructurePieceType.STRONGHOLD_PRISON_HALL, tag);
        }

        public void addChildren(StructurePiece startPiece, StructurePiecesBuilder builder, RandomSource random) {
            generateSmallDoorChildForward((StartPiece) startPiece, builder, random, 1, 1);
        }

        public static PrisonHall createPiece(StructurePiecesBuilder builder, RandomSource random, int footX,
                int footY, int footZ, Direction direction, int genDepth) {
            BoundingBox box = BoundingBox.orientBox(footX, footY, footZ, -1, -1, 0, 9, 5, 11, direction);
            return fits(builder, box) ? new PrisonHall(genDepth, random, box, direction) : null;
        }

        public void postProcess(WorldGenLevel level, StructureManager structureManager, ChunkGenerator generator,
                RandomSource random, BoundingBox chunkBB, ChunkPos chunkPos, BlockPos referencePos) {
            generateBox(level, chunkBB, 0, 0, 0, 8, 4, 10, true, random, kSelector);
            generateSmallDoor(level, random, chunkBB, entryDoor, 1, 1, 0);
            generateBox(level, chunkBB, 1, 1, 10, 3, 3, 10, CAVE_AIR, CAVE_AIR, false);
            for (int z : new int[] {1, 3, 7, 9}) {
                generateBox(level, chunkBB, 4, 1, z, 4, 3, z, false, random, kSelector);
            }
            BlockState ns = bars(true, true, false, false);
            BlockState nse = bars(true, true, true, false);
            BlockState we = bars(false, false, true, true);
            for (int y = 1; y <= 3; y++) {
                placeBlock(level, ns, 4, y, 4, chunkBB);
                placeBlock(level, nse, 4, y, 5, chunkBB);
                placeBlock(level, ns, 4, y, 6, chunkBB);
                placeBlock(level, we, 5, y, 5, chunkBB);
                placeBlock(level, we, 6, y, 5, chunkBB);
                placeBlock(level, we, 7, y, 5, chunkBB);
            }
            placeBlock(level, ns, 4, 3, 2, chunkBB);
            placeBlock(level, ns, 4, 3, 8, chunkBB);
            BlockState lower = st(Blocks.IRON_DOOR).setValue(BlockStateProperties.HORIZONTAL_FACING, Direction.WEST);
            BlockState upper = lower.setValue(BlockStateProperties.DOUBLE_BLOCK_HALF, DoubleBlockHalf.UPPER);
            placeBlock(level, lower, 4, 1, 2, chunkBB);
            placeBlock(level, upper, 4, 2, 2, chunkBB);
            placeBlock(level, lower, 4, 1, 8, chunkBB);
            placeBlock(level, upper, 4, 2, 8, chunkBB);
        }
    }

    public static class Library extends StrongholdPiece {
        private boolean isTall;

        public Library(int genDepth, RandomSource random, BoundingBox boundingBox, Direction direction) {
            super(StructurePieceType.STRONGHOLD_LIBRARY, genDepth, boundingBox);
            setOrientation(direction);
            entryDoor = randomSmallDoor(random);
            isTall = boundingBox.getYSpan() > 6;
        }

        public Library(CompoundTag tag) {
            super(StructurePieceType.STRONGHOLD_LIBRARY, tag);
            isTall = tag.getBooleanOr("Tall", false);
        }

        protected void addAdditionalSaveData(StructurePieceSerializationContext context, CompoundTag tag) {
            super.addAdditionalSaveData(context, tag);
            tag.putBoolean("Tall", isTall);
        }

        public static Library createPiece(StructurePiecesBuilder builder, RandomSource random, int footX, int footY,
                int footZ, Direction direction, int genDepth) {
            BoundingBox box = BoundingBox.orientBox(footX, footY, footZ, -4, -1, 0, 14, 11, 15, direction);
            if (!fits(builder, box)) {
                box = BoundingBox.orientBox(footX, footY, footZ, -4, -1, 0, 14, 6, 15, direction);
                if (!fits(builder, box)) {
                    return null;
                }
            }
            return new Library(genDepth, random, box, direction);
        }

        private void shelf(WorldGenLevel level, BoundingBox chunkBB, int x0, int y0, int z0, int x1, int y1, int z1,
                BlockState state) {
            generateBox(level, chunkBB, x0, y0, z0, x1, y1, z1, state, state, false);
        }

        public void postProcess(WorldGenLevel level, StructureManager structureManager, ChunkGenerator generator,
                RandomSource random, BoundingBox chunkBB, ChunkPos chunkPos, BlockPos referencePos) {
            int height = isTall ? 11 : 6;
            generateBox(level, chunkBB, 0, 0, 0, 13, height - 1, 14, true, random, kSelector);
            generateSmallDoor(level, random, chunkBB, entryDoor, 4, 1, 0);
            BlockState web = st(Blocks.COBWEB);
            generateMaybeBox(level, chunkBB, random, 0.07F, 2, 1, 1, 11, 4, 13, web, web, false, false);
            BlockState planks = st(Blocks.OAK_PLANKS);
            BlockState books = st(Blocks.BOOKSHELF);
            for (int d = 1; d <= 13; d++) {
                if ((d - 1) % 4 == 0) {
                    shelf(level, chunkBB, 1, 1, d, 1, 4, d, planks);
                    shelf(level, chunkBB, 12, 1, d, 12, 4, d, planks);
                    placeBlock(level, torch(Direction.EAST), 2, 3, d, chunkBB);
                    placeBlock(level, torch(Direction.WEST), 11, 3, d, chunkBB);
                    if (isTall) {
                        shelf(level, chunkBB, 1, 6, d, 1, 9, d, planks);
                        shelf(level, chunkBB, 12, 6, d, 12, 9, d, planks);
                    }
                } else {
                    shelf(level, chunkBB, 1, 1, d, 1, 4, d, books);
                    shelf(level, chunkBB, 12, 1, d, 12, 4, d, books);
                    if (isTall) {
                        shelf(level, chunkBB, 1, 6, d, 1, 9, d, books);
                        shelf(level, chunkBB, 12, 6, d, 12, 9, d, books);
                    }
                }
            }
            for (int d = 3; d < 12; d += 2) {
                shelf(level, chunkBB, 3, 1, d, 4, 3, d, books);
                shelf(level, chunkBB, 6, 1, d, 7, 3, d, books);
                shelf(level, chunkBB, 9, 1, d, 10, 3, d, books);
            }
            if (isTall) {
                buildGallery(level, chunkBB, planks);
            }
            createChest(level, chunkBB, random, 3, 3, 5, BuiltInLootTables.STRONGHOLD_LIBRARY);
            if (isTall) {
                placeBlock(level, CAVE_AIR, 12, 9, 1, chunkBB);
                createChest(level, chunkBB, random, 12, 8, 1, BuiltInLootTables.STRONGHOLD_LIBRARY);
            }
        }

        private void buildGallery(WorldGenLevel level, BoundingBox chunkBB, BlockState planks) {
            shelf(level, chunkBB, 1, 5, 1, 3, 5, 13, planks);
            shelf(level, chunkBB, 10, 5, 1, 12, 5, 13, planks);
            shelf(level, chunkBB, 4, 5, 1, 9, 5, 2, planks);
            shelf(level, chunkBB, 4, 5, 12, 9, 5, 13, planks);
            placeBlock(level, planks, 9, 5, 11, chunkBB);
            placeBlock(level, planks, 8, 5, 11, chunkBB);
            placeBlock(level, planks, 9, 5, 10, chunkBB);
            BlockState we = fence(false, false, true, true);
            BlockState ns = fence(true, true, false, false);
            shelf(level, chunkBB, 3, 6, 3, 3, 6, 11, ns);
            shelf(level, chunkBB, 10, 6, 3, 10, 6, 9, ns);
            shelf(level, chunkBB, 4, 6, 2, 9, 6, 2, we);
            shelf(level, chunkBB, 4, 6, 12, 7, 6, 12, we);
            placeBlock(level, fence(true, false, true, false), 3, 6, 2, chunkBB);
            placeBlock(level, fence(false, true, true, false), 3, 6, 12, chunkBB);
            placeBlock(level, fence(true, false, false, true), 10, 6, 2, chunkBB);
            for (int i = 0; i <= 2; i++) {
                placeBlock(level, fence(false, true, false, true), 8 + i, 6, 12 - i, chunkBB);
                if (i != 2) {
                    placeBlock(level, fence(true, false, true, false), 8 + i, 6, 11 - i, chunkBB);
                }
            }
            BlockState ladder = st(Blocks.LADDER).setValue(BlockStateProperties.HORIZONTAL_FACING, Direction.SOUTH);
            for (int y = 1; y <= 7; y++) {
                placeBlock(level, ladder, 10, y, 13, chunkBB);
            }
            BlockState east = fence(false, false, true, false);
            BlockState west = fence(false, false, false, true);
            placeBlock(level, east, 6, 9, 7, chunkBB);
            placeBlock(level, west, 7, 9, 7, chunkBB);
            placeBlock(level, east, 6, 8, 7, chunkBB);
            placeBlock(level, west, 7, 8, 7, chunkBB);
            BlockState cross = fence(true, true, true, true);
            placeBlock(level, cross, 6, 7, 7, chunkBB);
            placeBlock(level, cross, 7, 7, 7, chunkBB);
            placeBlock(level, east, 5, 7, 7, chunkBB);
            placeBlock(level, west, 8, 7, 7, chunkBB);
            placeBlock(level, fence(true, false, true, false), 6, 7, 6, chunkBB);
            placeBlock(level, fence(false, true, true, false), 6, 7, 8, chunkBB);
            placeBlock(level, fence(true, false, false, true), 7, 7, 6, chunkBB);
            placeBlock(level, fence(false, true, false, true), 7, 7, 8, chunkBB);
            BlockState lamp = st(Blocks.TORCH);
            int[][] lamps = {{5, 8, 7}, {8, 8, 7}, {6, 8, 6}, {6, 8, 8}, {7, 8, 6}, {7, 8, 8}};
            for (int[] c : lamps) {
                placeBlock(level, lamp, c[0], c[1], c[2], chunkBB);
            }
        }
    }

    public static class FiveCrossing extends StrongholdPiece {
        private boolean leftLow;
        private boolean leftHigh;
        private boolean rightLow;
        private boolean rightHigh;

        public FiveCrossing(int genDepth, RandomSource random, BoundingBox boundingBox, Direction direction) {
            super(StructurePieceType.STRONGHOLD_FIVE_CROSSING, genDepth, boundingBox);
            setOrientation(direction);
            entryDoor = randomSmallDoor(random);
            leftLow = random.nextBoolean();
            leftHigh = random.nextBoolean();
            rightLow = random.nextBoolean();
            rightHigh = random.nextInt(3) > 0;
        }

        public FiveCrossing(CompoundTag tag) {
            super(StructurePieceType.STRONGHOLD_FIVE_CROSSING, tag);
            leftLow = tag.getBooleanOr("leftLow", false);
            leftHigh = tag.getBooleanOr("leftHigh", false);
            rightLow = tag.getBooleanOr("rightLow", false);
            rightHigh = tag.getBooleanOr("rightHigh", false);
        }

        protected void addAdditionalSaveData(StructurePieceSerializationContext context, CompoundTag tag) {
            super.addAdditionalSaveData(context, tag);
            tag.putBoolean("leftLow", leftLow);
            tag.putBoolean("leftHigh", leftHigh);
            tag.putBoolean("rightLow", rightLow);
            tag.putBoolean("rightHigh", rightHigh);
        }

        public void addChildren(StructurePiece startPiece, StructurePiecesBuilder builder, RandomSource random) {
            int low = 3;
            int high = 5;
            Direction facing = getOrientation();
            if (facing == Direction.WEST || facing == Direction.NORTH) {
                low = 8 - low;
                high = 8 - high;
            }
            StartPiece start = (StartPiece) startPiece;
            generateSmallDoorChildForward(start, builder, random, 5, 1);
            if (leftLow) {
                generateSmallDoorChildLeft(start, builder, random, low, 1);
            }
            if (leftHigh) {
                generateSmallDoorChildLeft(start, builder, random, high, 7);
            }
            if (rightLow) {
                generateSmallDoorChildRight(start, builder, random, low, 1);
            }
            if (rightHigh) {
                generateSmallDoorChildRight(start, builder, random, high, 7);
            }
        }

        public static FiveCrossing createPiece(StructurePiecesBuilder builder, RandomSource random, int footX,
                int footY, int footZ, Direction direction, int genDepth) {
            BoundingBox box = BoundingBox.orientBox(footX, footY, footZ, -4, -3, 0, 10, 9, 11, direction);
            return fits(builder, box) ? new FiveCrossing(genDepth, random, box, direction) : null;
        }

        public void postProcess(WorldGenLevel level, StructureManager structureManager, ChunkGenerator generator,
                RandomSource random, BoundingBox chunkBB, ChunkPos chunkPos, BlockPos referencePos) {
            generateBox(level, chunkBB, 0, 0, 0, 9, 8, 10, true, random, kSelector);
            generateSmallDoor(level, random, chunkBB, entryDoor, 4, 3, 0);
            if (leftLow) {
                generateBox(level, chunkBB, 0, 3, 1, 0, 5, 3, CAVE_AIR, CAVE_AIR, false);
            }
            if (rightLow) {
                generateBox(level, chunkBB, 9, 3, 1, 9, 5, 3, CAVE_AIR, CAVE_AIR, false);
            }
            if (leftHigh) {
                generateBox(level, chunkBB, 0, 5, 7, 0, 7, 9, CAVE_AIR, CAVE_AIR, false);
            }
            if (rightHigh) {
                generateBox(level, chunkBB, 9, 5, 7, 9, 7, 9, CAVE_AIR, CAVE_AIR, false);
            }
            generateBox(level, chunkBB, 5, 1, 10, 7, 3, 10, CAVE_AIR, CAVE_AIR, false);
            int[][] stoneBoxes = {{1, 2, 1, 8, 2, 6}, {4, 1, 5, 4, 4, 9}, {8, 1, 5, 8, 4, 9}, {1, 4, 7, 3, 4, 9},
                {1, 3, 5, 3, 3, 6}};
            for (int[] b : stoneBoxes) {
                generateBox(level, chunkBB, b[0], b[1], b[2], b[3], b[4], b[5], false, random, kSelector);
            }
            BlockState slab = st(Blocks.SMOOTH_STONE_SLAB);
            generateBox(level, chunkBB, 1, 3, 4, 3, 3, 4, slab, slab, false);
            generateBox(level, chunkBB, 1, 4, 6, 3, 4, 6, slab, slab, false);
            generateBox(level, chunkBB, 5, 1, 7, 7, 1, 8, false, random, kSelector);
            generateBox(level, chunkBB, 5, 1, 9, 7, 1, 9, slab, slab, false);
            generateBox(level, chunkBB, 5, 2, 7, 7, 2, 7, slab, slab, false);
            generateBox(level, chunkBB, 4, 5, 7, 4, 5, 9, slab, slab, false);
            generateBox(level, chunkBB, 8, 5, 7, 8, 5, 9, slab, slab, false);
            BlockState doubleSlab = slab.setValue(BlockStateProperties.SLAB_TYPE, SlabType.DOUBLE);
            generateBox(level, chunkBB, 5, 5, 7, 7, 5, 9, doubleSlab, doubleSlab, false);
            placeBlock(level, torch(Direction.SOUTH), 6, 5, 6, chunkBB);
        }
    }

    public static class PortalRoom extends StrongholdPiece {
        private boolean hasPlacedSpawner;

        public PortalRoom(int genDepth, BoundingBox boundingBox, Direction direction) {
            super(StructurePieceType.STRONGHOLD_PORTAL_ROOM, genDepth, boundingBox);
            setOrientation(direction);
        }

        public PortalRoom(CompoundTag tag) {
            super(StructurePieceType.STRONGHOLD_PORTAL_ROOM, tag);
            hasPlacedSpawner = tag.getBooleanOr("Mob", false);
        }

        protected void addAdditionalSaveData(StructurePieceSerializationContext context, CompoundTag tag) {
            super.addAdditionalSaveData(context, tag);
            tag.putBoolean("Mob", hasPlacedSpawner);
        }

        public void addChildren(StructurePiece startPiece, StructurePiecesBuilder builder, RandomSource random) {
            if (startPiece != null) {
                ((StartPiece) startPiece).portalRoomPiece = this;
            }
        }

        public static PortalRoom createPiece(StructurePiecesBuilder builder, int footX, int footY, int footZ,
                Direction direction, int genDepth) {
            BoundingBox box = BoundingBox.orientBox(footX, footY, footZ, -4, -1, 0, 11, 8, 16, direction);
            return fits(builder, box) ? new PortalRoom(genDepth, box, direction) : null;
        }

        public void postProcess(WorldGenLevel level, StructureManager structureManager, ChunkGenerator generator,
                RandomSource random, BoundingBox chunkBB, ChunkPos chunkPos, BlockPos referencePos) {
            generateBox(level, chunkBB, 0, 0, 0, 10, 7, 15, false, random, kSelector);
            generateSmallDoor(level, random, chunkBB, SmallDoorType.GRATES, 4, 1, 0);
            int[][] stoneBoxes = {{1, 6, 1, 1, 6, 14}, {9, 6, 1, 9, 6, 14}, {2, 6, 1, 8, 6, 2}, {2, 6, 14, 8, 6, 14},
                {1, 1, 1, 2, 1, 4}, {8, 1, 1, 9, 1, 4}};
            for (int[] b : stoneBoxes) {
                generateBox(level, chunkBB, b[0], b[1], b[2], b[3], b[4], b[5], false, random, kSelector);
            }
            BlockState lava = st(Blocks.LAVA);
            generateBox(level, chunkBB, 1, 1, 1, 1, 1, 3, lava, lava, false);
            generateBox(level, chunkBB, 9, 1, 1, 9, 1, 3, lava, lava, false);
            generateBox(level, chunkBB, 3, 1, 8, 7, 1, 12, false, random, kSelector);
            generateBox(level, chunkBB, 4, 1, 9, 6, 1, 11, lava, lava, false);
            BlockState ns = bars(true, true, false, false);
            BlockState we = bars(false, false, true, true);
            for (int z = 3; z < 14; z += 2) {
                generateBox(level, chunkBB, 0, 3, z, 0, 4, z, ns, ns, false);
                generateBox(level, chunkBB, 10, 3, z, 10, 4, z, ns, ns, false);
            }
            for (int x = 2; x < 9; x += 2) {
                generateBox(level, chunkBB, x, 3, 15, x, 4, 15, we, we, false);
            }
            BlockState stairs = st(Blocks.STONE_BRICK_STAIRS).setValue(BlockStateProperties.HORIZONTAL_FACING,
                Direction.NORTH);
            generateBox(level, chunkBB, 4, 1, 5, 6, 1, 7, false, random, kSelector);
            generateBox(level, chunkBB, 4, 2, 6, 6, 2, 7, false, random, kSelector);
            generateBox(level, chunkBB, 4, 3, 7, 6, 3, 7, false, random, kSelector);
            for (int x = 4; x <= 6; x++) {
                placeBlock(level, stairs, x, 1, 4, chunkBB);
                placeBlock(level, stairs, x, 2, 5, chunkBB);
                placeBlock(level, stairs, x, 3, 6, chunkBB);
            }
            boolean allEyes = true;
            boolean[] eyes = new boolean[12];
            for (int i = 0; i < eyes.length; i++) {
                eyes[i] = random.nextFloat() > 0.9F;
                allEyes &= eyes[i];
            }
            Direction[] faces = {Direction.NORTH, Direction.SOUTH, Direction.EAST, Direction.WEST};
            int[][] frames = {{4, 8}, {5, 8}, {6, 8}, {4, 12}, {5, 12}, {6, 12}, {3, 9}, {3, 10}, {3, 11}, {7, 9},
                {7, 10}, {7, 11}};
            for (int i = 0; i < frames.length; i++) {
                BlockState frame = st(Blocks.END_PORTAL_FRAME)
                    .setValue(BlockStateProperties.HORIZONTAL_FACING, faces[i / 3])
                    .setValue(BlockStateProperties.EYE, eyes[i]);
                placeBlock(level, frame, frames[i][0], 3, frames[i][1], chunkBB);
            }
            if (allEyes) {
                BlockState portal = st(Blocks.END_PORTAL);
                for (int x = 4; x <= 6; x++) {
                    for (int z = 9; z <= 11; z++) {
                        placeBlock(level, portal, x, 3, z, chunkBB);
                    }
                }
            }
            if (!hasPlacedSpawner) {
                BlockPos pos = getWorldPos(5, 3, 6);
                if (chunkBB.isInside(pos)) {
                    hasPlacedSpawner = true;
                    level.setBlock(pos, st(Blocks.SPAWNER), 2);
                    BlockEntity entity = level.getBlockEntity(pos);
                    if (entity instanceof SpawnerBlockEntity spawner) {
                        spawner.setEntityId(EntityTypes.SILVERFISH, random);
                    }
                }
            }
        }
    }

    static class SmoothStoneSelector extends StructurePiece.BlockSelector {
        public void next(RandomSource random, int worldX, int worldY, int worldZ, boolean isEdge) {
            if (!isEdge) {
                next = st(Blocks.CAVE_AIR);
                return;
            }
            float roll = random.nextFloat();
            next = st(roll < 0.2F ? Blocks.CRACKED_STONE_BRICKS
                : roll < 0.5F ? Blocks.MOSSY_STONE_BRICKS
                : roll < 0.55F ? Blocks.INFESTED_STONE_BRICKS : Blocks.STONE_BRICKS);
        }
    }
}
