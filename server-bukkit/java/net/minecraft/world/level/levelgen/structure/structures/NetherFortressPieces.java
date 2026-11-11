// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.structure.structures;

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
import net.minecraft.world.level.chunk.ChunkGenerator;
import net.minecraft.world.level.levelgen.structure.BoundingBox;
import net.minecraft.world.level.levelgen.structure.StructurePiece;
import net.minecraft.world.level.levelgen.structure.pieces.StructurePieceSerializationContext;
import net.minecraft.world.level.levelgen.structure.pieces.StructurePieceType;
import net.minecraft.world.level.levelgen.structure.pieces.StructurePiecesBuilder;
import net.minecraft.world.level.material.Fluids;
import net.minecraft.world.level.storage.loot.BuiltInLootTables;

public class NetherFortressPieces {
    public static final int MAGIC_START_Y = 64;
    private static final int kMaxDepth = 30;
    private static final int kLowestY = 10;
    private static final int kMaxReach = 112;

    private static final PieceWeight[] kBridgeWeights = {new PieceWeight(BridgeStraight.class, 30, 0, true),
        new PieceWeight(BridgeCrossing.class, 10, 4), new PieceWeight(RoomCrossing.class, 10, 4),
        new PieceWeight(StairsRoom.class, 10, 3), new PieceWeight(MonsterThrone.class, 5, 2),
        new PieceWeight(CastleEntrance.class, 5, 1)};
    private static final PieceWeight[] kCastleWeights = {new PieceWeight(CastleSmallCorridorPiece.class, 25, 0, true),
        new PieceWeight(CastleSmallCorridorCrossingPiece.class, 15, 5),
        new PieceWeight(CastleSmallCorridorRightTurnPiece.class, 5, 10),
        new PieceWeight(CastleSmallCorridorLeftTurnPiece.class, 5, 10),
        new PieceWeight(CastleCorridorStairsPiece.class, 10, 3, true),
        new PieceWeight(CastleCorridorTBalconyPiece.class, 7, 2), new PieceWeight(CastleStalkRoom.class, 5, 2)};

    private static BlockState st(Block block) {
        return block.defaultBlockState();
    }

    private static BlockState fence(boolean n, boolean s, boolean e, boolean w) {
        return st(Blocks.NETHER_BRICK_FENCE).setValue(BlockStateProperties.NORTH, n)
            .setValue(BlockStateProperties.SOUTH, s).setValue(BlockStateProperties.EAST, e)
            .setValue(BlockStateProperties.WEST, w);
    }

    private static NetherBridgePiece findAndCreateBridgePieceFactory(PieceWeight piece, StructurePiecesBuilder builder,
            RandomSource random, int x, int y, int z, Direction dir, int depth) {
        Class<? extends NetherBridgePiece> kind = piece.pieceClass;
        if (kind == BridgeStraight.class) {
            return BridgeStraight.createPiece(builder, random, x, y, z, dir, depth);
        } else if (kind == BridgeCrossing.class) {
            return BridgeCrossing.createPiece(builder, x, y, z, dir, depth);
        } else if (kind == RoomCrossing.class) {
            return RoomCrossing.createPiece(builder, x, y, z, dir, depth);
        } else if (kind == StairsRoom.class) {
            return StairsRoom.createPiece(builder, x, y, z, depth, dir);
        } else if (kind == MonsterThrone.class) {
            return MonsterThrone.createPiece(builder, x, y, z, depth, dir);
        } else if (kind == CastleEntrance.class) {
            return CastleEntrance.createPiece(builder, random, x, y, z, dir, depth);
        } else if (kind == CastleSmallCorridorPiece.class) {
            return CastleSmallCorridorPiece.createPiece(builder, x, y, z, dir, depth);
        } else if (kind == CastleSmallCorridorRightTurnPiece.class) {
            return CastleSmallCorridorRightTurnPiece.createPiece(builder, random, x, y, z, dir, depth);
        } else if (kind == CastleSmallCorridorLeftTurnPiece.class) {
            return CastleSmallCorridorLeftTurnPiece.createPiece(builder, random, x, y, z, dir, depth);
        } else if (kind == CastleCorridorStairsPiece.class) {
            return CastleCorridorStairsPiece.createPiece(builder, x, y, z, dir, depth);
        } else if (kind == CastleCorridorTBalconyPiece.class) {
            return CastleCorridorTBalconyPiece.createPiece(builder, x, y, z, dir, depth);
        } else if (kind == CastleSmallCorridorCrossingPiece.class) {
            return CastleSmallCorridorCrossingPiece.createPiece(builder, x, y, z, dir, depth);
        } else if (kind == CastleStalkRoom.class) {
            return CastleStalkRoom.createPiece(builder, x, y, z, dir, depth);
        }
        return null;
    }

    static class PieceWeight {
        public Class<? extends NetherBridgePiece> pieceClass;
        public int weight;
        public int placeCount;
        public int maxPlaceCount;
        public boolean allowInRow;

        public PieceWeight(Class<? extends NetherBridgePiece> pieceClass, int weight, int maxPlaceCount,
                boolean allowInRow) {
            this.pieceClass = pieceClass;
            this.weight = weight;
            this.maxPlaceCount = maxPlaceCount;
            this.allowInRow = allowInRow;
        }

        public PieceWeight(Class<? extends NetherBridgePiece> pieceClass, int weight, int maxPlaceCount) {
            this(pieceClass, weight, maxPlaceCount, false);
        }

        public boolean doPlace(int depth) {
            return maxPlaceCount == 0 || placeCount < maxPlaceCount;
        }

        public boolean isValid() {
            return maxPlaceCount == 0 || placeCount < maxPlaceCount;
        }
    }

    static abstract class NetherBridgePiece extends StructurePiece {
        protected NetherBridgePiece(StructurePieceType type, int genDepth, BoundingBox boundingBox) {
            super(type, genDepth, boundingBox);
        }

        public NetherBridgePiece(StructurePieceType type, CompoundTag tag) {
            super(type, tag);
        }

        protected void addAdditionalSaveData(StructurePieceSerializationContext context, CompoundTag tag) {
            return;
        }

        private int updatePieceWeight(List<PieceWeight> pieces) {
            boolean anyLimited = false;
            int total = 0;
            for (PieceWeight piece : pieces) {
                if (piece.maxPlaceCount > 0 && piece.placeCount < piece.maxPlaceCount) {
                    anyLimited = true;
                }
                total += piece.weight;
            }
            return anyLimited ? total : -1;
        }

        private NetherBridgePiece generatePiece(StartPiece start, List<PieceWeight> pieces,
                StructurePiecesBuilder builder, RandomSource random, int x, int y, int z, Direction dir, int depth) {
            int total = updatePieceWeight(pieces);
            boolean allowed = total > 0 && depth <= kMaxDepth;
            for (int attempt = 0; attempt < 5 && allowed; attempt++) {
                int roll = random.nextInt(total);
                for (PieceWeight weight : pieces) {
                    roll -= weight.weight;
                    if (roll >= 0) {
                        continue;
                    }
                    if (!weight.doPlace(depth) || weight == start.previousPiece && !weight.allowInRow) {
                        break;
                    }
                    NetherBridgePiece made = findAndCreateBridgePieceFactory(weight, builder, random, x, y, z, dir,
                        depth);
                    if (made != null) {
                        weight.placeCount++;
                        start.previousPiece = weight;
                        if (!weight.isValid()) {
                            pieces.remove(weight);
                        }
                        return made;
                    }
                }
            }
            return BridgeEndFiller.createPiece(builder, random, x, y, z, dir, depth);
        }

        private StructurePiece generateAndAddPiece(StartPiece start, StructurePiecesBuilder builder,
                RandomSource random, int x, int y, int z, Direction dir, int depth, boolean isCastle) {
            BoundingBox origin = start.getBoundingBox();
            if (Math.abs(x - origin.minX()) > kMaxReach || Math.abs(z - origin.minZ()) > kMaxReach) {
                return BridgeEndFiller.createPiece(builder, random, x, y, z, dir, depth);
            }
            List<PieceWeight> pool = isCastle ? start.availableCastlePieces : start.availableBridgePieces;
            StructurePiece piece = generatePiece(start, pool, builder, random, x, y, z, dir, depth + 1);
            if (piece != null) {
                builder.addPiece(piece);
                start.pendingChildren.add(piece);
            }
            return piece;
        }

        protected StructurePiece generateChildForward(StartPiece startPiece, StructurePiecesBuilder builder,
                RandomSource random, int xOff, int yOff, boolean isCastle) {
            Direction facing = getOrientation();
            if (facing == null) {
                return null;
            }
            BoundingBox bb = boundingBox;
            int y = bb.minY() + yOff;
            return switch (facing) {
                case NORTH -> generateAndAddPiece(startPiece, builder, random, bb.minX() + xOff, y, bb.minZ() - 1,
                    facing, getGenDepth(), isCastle);
                case SOUTH -> generateAndAddPiece(startPiece, builder, random, bb.minX() + xOff, y, bb.maxZ() + 1,
                    facing, getGenDepth(), isCastle);
                case WEST -> generateAndAddPiece(startPiece, builder, random, bb.minX() - 1, y, bb.minZ() + xOff,
                    facing, getGenDepth(), isCastle);
                case EAST -> generateAndAddPiece(startPiece, builder, random, bb.maxX() + 1, y, bb.minZ() + xOff,
                    facing, getGenDepth(), isCastle);
                default -> null;
            };
        }

        protected StructurePiece generateChildLeft(StartPiece startPiece, StructurePiecesBuilder builder,
                RandomSource random, int yOff, int zOff, boolean isCastle) {
            Direction facing = getOrientation();
            if (facing == null) {
                return null;
            }
            BoundingBox bb = boundingBox;
            int y = bb.minY() + yOff;
            return switch (facing) {
                case NORTH, SOUTH -> generateAndAddPiece(startPiece, builder, random, bb.minX() - 1, y,
                    bb.minZ() + zOff, Direction.WEST, getGenDepth(), isCastle);
                case WEST, EAST -> generateAndAddPiece(startPiece, builder, random, bb.minX() + zOff, y,
                    bb.minZ() - 1, Direction.NORTH, getGenDepth(), isCastle);
                default -> null;
            };
        }

        protected StructurePiece generateChildRight(StartPiece startPiece, StructurePiecesBuilder builder,
                RandomSource random, int yOff, int zOff, boolean isCastle) {
            Direction facing = getOrientation();
            if (facing == null) {
                return null;
            }
            BoundingBox bb = boundingBox;
            int y = bb.minY() + yOff;
            return switch (facing) {
                case NORTH, SOUTH -> generateAndAddPiece(startPiece, builder, random, bb.maxX() + 1, y,
                    bb.minZ() + zOff, Direction.EAST, getGenDepth(), isCastle);
                case WEST, EAST -> generateAndAddPiece(startPiece, builder, random, bb.minX() + zOff, y,
                    bb.maxZ() + 1, Direction.SOUTH, getGenDepth(), isCastle);
                default -> null;
            };
        }

        protected static boolean isOkBox(BoundingBox box) {
            return box.minY() > kLowestY;
        }

        protected static boolean fits(StructurePiecesBuilder builder, BoundingBox box) {
            return isOkBox(box) && builder.findCollisionPiece(box) == null;
        }

        protected void bricks(WorldGenLevel level, BoundingBox cb, int x0, int y0, int z0, int x1, int y1, int z1) {
            BlockState brick = st(Blocks.NETHER_BRICKS);
            generateBox(level, cb, x0, y0, z0, x1, y1, z1, brick, brick, false);
        }

        protected void hollow(WorldGenLevel level, BoundingBox cb, int x0, int y0, int z0, int x1, int y1, int z1) {
            BlockState air = st(Blocks.AIR);
            generateBox(level, cb, x0, y0, z0, x1, y1, z1, air, air, false);
        }

        protected void filled(WorldGenLevel level, BoundingBox cb, int x0, int y0, int z0, int x1, int y1, int z1,
                BlockState state) {
            generateBox(level, cb, x0, y0, z0, x1, y1, z1, state, state, false);
        }

        protected void footings(WorldGenLevel level, BoundingBox cb, int x0, int z0, int x1, int z1) {
            BlockState brick = st(Blocks.NETHER_BRICKS);
            for (int x = x0; x <= x1; x++) {
                for (int z = z0; z <= z1; z++) {
                    fillColumnDown(level, brick, x, -1, z, cb);
                }
            }
        }
    }

    public static class StartPiece extends BridgeCrossing {
        private PieceWeight previousPiece;
        private final List<PieceWeight> availableBridgePieces = new ArrayList<>();
        private final List<PieceWeight> availableCastlePieces = new ArrayList<>();
        public final List<StructurePiece> pendingChildren = new ArrayList<>();

        public StartPiece(RandomSource random, int west, int north) {
            super(west, north, getRandomHorizontalDirection(random));
            for (PieceWeight weight : kBridgeWeights) {
                weight.placeCount = 0;
                availableBridgePieces.add(weight);
            }
            for (PieceWeight weight : kCastleWeights) {
                weight.placeCount = 0;
                availableCastlePieces.add(weight);
            }
        }

        public StartPiece(CompoundTag tag) {
            super(StructurePieceType.NETHER_FORTRESS_START, tag);
        }
    }

    public static class BridgeStraight extends NetherBridgePiece {
        public BridgeStraight(int genDepth, RandomSource random, BoundingBox boundingBox, Direction direction) {
            super(StructurePieceType.NETHER_FORTRESS_BRIDGE_STRAIGHT, genDepth, boundingBox);
            setOrientation(direction);
        }

        public BridgeStraight(CompoundTag tag) {
            super(StructurePieceType.NETHER_FORTRESS_BRIDGE_STRAIGHT, tag);
        }

        public void addChildren(StructurePiece startPiece, StructurePiecesBuilder builder, RandomSource random) {
            generateChildForward((StartPiece) startPiece, builder, random, 1, 3, false);
        }

        public static BridgeStraight createPiece(StructurePiecesBuilder builder, RandomSource random, int footX,
                int footY, int footZ, Direction direction, int genDepth) {
            BoundingBox box = BoundingBox.orientBox(footX, footY, footZ, -1, -3, 0, 5, 10, 19, direction);
            return fits(builder, box) ? new BridgeStraight(genDepth, random, box, direction) : null;
        }

        public void postProcess(WorldGenLevel level, StructureManager structureManager, ChunkGenerator generator,
                RandomSource random, BoundingBox cb, ChunkPos chunkPos, BlockPos referencePos) {
            bricks(level, cb, 0, 3, 0, 4, 4, 18);
            hollow(level, cb, 1, 5, 0, 3, 7, 18);
            bricks(level, cb, 0, 5, 0, 0, 5, 18);
            bricks(level, cb, 4, 5, 0, 4, 5, 18);
            bricks(level, cb, 0, 2, 0, 4, 2, 5);
            bricks(level, cb, 0, 2, 13, 4, 2, 18);
            bricks(level, cb, 0, 0, 0, 4, 1, 3);
            bricks(level, cb, 0, 0, 15, 4, 1, 18);
            footings(level, cb, 0, 0, 4, 2);
            footings(level, cb, 0, 16, 4, 18);
            BlockState east = fence(true, true, true, false);
            BlockState west = fence(true, true, false, true);
            filled(level, cb, 0, 1, 1, 0, 4, 1, east);
            filled(level, cb, 0, 3, 4, 0, 4, 4, east);
            filled(level, cb, 0, 3, 14, 0, 4, 14, east);
            filled(level, cb, 0, 1, 17, 0, 4, 17, east);
            filled(level, cb, 4, 1, 1, 4, 4, 1, west);
            filled(level, cb, 4, 3, 4, 4, 4, 4, west);
            filled(level, cb, 4, 3, 14, 4, 4, 14, west);
            filled(level, cb, 4, 1, 17, 4, 4, 17, west);
        }
    }

    public static class BridgeEndFiller extends NetherBridgePiece {
        private final int selfSeed;

        public BridgeEndFiller(int genDepth, RandomSource random, BoundingBox boundingBox, Direction direction) {
            super(StructurePieceType.NETHER_FORTRESS_BRIDGE_END_FILLER, genDepth, boundingBox);
            setOrientation(direction);
            selfSeed = random.nextInt();
        }

        public BridgeEndFiller(CompoundTag tag) {
            super(StructurePieceType.NETHER_FORTRESS_BRIDGE_END_FILLER, tag);
            selfSeed = tag.getIntOr("Seed", 0);
        }

        public static BridgeEndFiller createPiece(StructurePiecesBuilder builder, RandomSource random, int footX,
                int footY, int footZ, Direction direction, int genDepth) {
            BoundingBox box = BoundingBox.orientBox(footX, footY, footZ, -1, -3, 0, 5, 10, 8, direction);
            return fits(builder, box) ? new BridgeEndFiller(genDepth, random, box, direction) : null;
        }

        protected void addAdditionalSaveData(StructurePieceSerializationContext context, CompoundTag tag) {
            super.addAdditionalSaveData(context, tag);
            tag.putInt("Seed", selfSeed);
        }

        public void postProcess(WorldGenLevel level, StructureManager structureManager, ChunkGenerator generator,
                RandomSource random, BoundingBox cb, ChunkPos chunkPos, BlockPos referencePos) {
            RandomSource own = RandomSource.createThreadLocalInstance(selfSeed);
            for (int x = 0; x <= 4; x++) {
                for (int y = 3; y <= 4; y++) {
                    bricks(level, cb, x, y, 0, x, y, own.nextInt(8));
                }
            }
            bricks(level, cb, 0, 5, 0, 0, 5, own.nextInt(8));
            bricks(level, cb, 4, 5, 0, 4, 5, own.nextInt(8));
            for (int x = 0; x <= 4; x++) {
                bricks(level, cb, x, 2, 0, x, 2, own.nextInt(5));
            }
            for (int x = 0; x <= 4; x++) {
                for (int y = 0; y <= 1; y++) {
                    bricks(level, cb, x, y, 0, x, y, own.nextInt(3));
                }
            }
        }
    }

    public static class BridgeCrossing extends NetherBridgePiece {
        public BridgeCrossing(int genDepth, BoundingBox boundingBox, Direction direction) {
            super(StructurePieceType.NETHER_FORTRESS_BRIDGE_CROSSING, genDepth, boundingBox);
            setOrientation(direction);
        }

        protected BridgeCrossing(int west, int north, Direction direction) {
            super(StructurePieceType.NETHER_FORTRESS_BRIDGE_CROSSING, 0,
                StructurePiece.makeBoundingBox(west, 64, north, direction, 19, 10, 19));
            setOrientation(direction);
        }

        protected BridgeCrossing(StructurePieceType type, CompoundTag tag) {
            super(type, tag);
        }

        public BridgeCrossing(CompoundTag tag) {
            this(StructurePieceType.NETHER_FORTRESS_BRIDGE_CROSSING, tag);
        }

        public void addChildren(StructurePiece startPiece, StructurePiecesBuilder builder, RandomSource random) {
            StartPiece start = (StartPiece) startPiece;
            generateChildForward(start, builder, random, 8, 3, false);
            generateChildLeft(start, builder, random, 3, 8, false);
            generateChildRight(start, builder, random, 3, 8, false);
        }

        public static BridgeCrossing createPiece(StructurePiecesBuilder builder, int footX, int footY, int footZ,
                Direction direction, int genDepth) {
            BoundingBox box = BoundingBox.orientBox(footX, footY, footZ, -8, -3, 0, 19, 10, 19, direction);
            return fits(builder, box) ? new BridgeCrossing(genDepth, box, direction) : null;
        }

        public void postProcess(WorldGenLevel level, StructureManager structureManager, ChunkGenerator generator,
                RandomSource random, BoundingBox cb, ChunkPos chunkPos, BlockPos referencePos) {
            bricks(level, cb, 7, 3, 0, 11, 4, 18);
            bricks(level, cb, 0, 3, 7, 18, 4, 11);
            hollow(level, cb, 8, 5, 0, 10, 7, 18);
            hollow(level, cb, 0, 5, 8, 18, 7, 10);
            bricks(level, cb, 7, 5, 0, 7, 5, 7);
            bricks(level, cb, 7, 5, 11, 7, 5, 18);
            bricks(level, cb, 11, 5, 0, 11, 5, 7);
            bricks(level, cb, 11, 5, 11, 11, 5, 18);
            bricks(level, cb, 0, 5, 7, 7, 5, 7);
            bricks(level, cb, 11, 5, 7, 18, 5, 7);
            bricks(level, cb, 0, 5, 11, 7, 5, 11);
            bricks(level, cb, 11, 5, 11, 18, 5, 11);
            bricks(level, cb, 7, 2, 0, 11, 2, 5);
            bricks(level, cb, 7, 2, 13, 11, 2, 18);
            bricks(level, cb, 7, 0, 0, 11, 1, 3);
            bricks(level, cb, 7, 0, 15, 11, 1, 18);
            footings(level, cb, 7, 0, 11, 2);
            footings(level, cb, 7, 16, 11, 18);
            bricks(level, cb, 0, 2, 7, 5, 2, 11);
            bricks(level, cb, 13, 2, 7, 18, 2, 11);
            bricks(level, cb, 0, 0, 7, 3, 1, 11);
            bricks(level, cb, 15, 0, 7, 18, 1, 11);
            footings(level, cb, 0, 7, 2, 11);
            footings(level, cb, 16, 7, 18, 11);
        }
    }

    public static class RoomCrossing extends NetherBridgePiece {
        public RoomCrossing(int genDepth, BoundingBox boundingBox, Direction direction) {
            super(StructurePieceType.NETHER_FORTRESS_ROOM_CROSSING, genDepth, boundingBox);
            setOrientation(direction);
        }

        public RoomCrossing(CompoundTag tag) {
            super(StructurePieceType.NETHER_FORTRESS_ROOM_CROSSING, tag);
        }

        public void addChildren(StructurePiece startPiece, StructurePiecesBuilder builder, RandomSource random) {
            StartPiece start = (StartPiece) startPiece;
            generateChildForward(start, builder, random, 2, 0, false);
            generateChildLeft(start, builder, random, 0, 2, false);
            generateChildRight(start, builder, random, 0, 2, false);
        }

        public static RoomCrossing createPiece(StructurePiecesBuilder builder, int footX, int footY, int footZ,
                Direction direction, int genDepth) {
            BoundingBox box = BoundingBox.orientBox(footX, footY, footZ, -2, 0, 0, 7, 9, 7, direction);
            return fits(builder, box) ? new RoomCrossing(genDepth, box, direction) : null;
        }

        public void postProcess(WorldGenLevel level, StructureManager structureManager, ChunkGenerator generator,
                RandomSource random, BoundingBox cb, ChunkPos chunkPos, BlockPos referencePos) {
            bricks(level, cb, 0, 0, 0, 6, 1, 6);
            hollow(level, cb, 0, 2, 0, 6, 7, 6);
            bricks(level, cb, 0, 2, 0, 1, 6, 0);
            bricks(level, cb, 0, 2, 6, 1, 6, 6);
            bricks(level, cb, 5, 2, 0, 6, 6, 0);
            bricks(level, cb, 5, 2, 6, 6, 6, 6);
            bricks(level, cb, 0, 2, 0, 0, 6, 1);
            bricks(level, cb, 0, 2, 5, 0, 6, 6);
            bricks(level, cb, 6, 2, 0, 6, 6, 1);
            bricks(level, cb, 6, 2, 5, 6, 6, 6);
            BlockState we = fence(false, false, true, true);
            BlockState ns = fence(true, true, false, false);
            bricks(level, cb, 2, 6, 0, 4, 6, 0);
            filled(level, cb, 2, 5, 0, 4, 5, 0, we);
            bricks(level, cb, 2, 6, 6, 4, 6, 6);
            filled(level, cb, 2, 5, 6, 4, 5, 6, we);
            bricks(level, cb, 0, 6, 2, 0, 6, 4);
            filled(level, cb, 0, 5, 2, 0, 5, 4, ns);
            bricks(level, cb, 6, 6, 2, 6, 6, 4);
            filled(level, cb, 6, 5, 2, 6, 5, 4, ns);
            footings(level, cb, 0, 0, 6, 6);
        }
    }

    public static class StairsRoom extends NetherBridgePiece {
        public StairsRoom(int genDepth, BoundingBox boundingBox, Direction direction) {
            super(StructurePieceType.NETHER_FORTRESS_STAIRS_ROOM, genDepth, boundingBox);
            setOrientation(direction);
        }

        public StairsRoom(CompoundTag tag) {
            super(StructurePieceType.NETHER_FORTRESS_STAIRS_ROOM, tag);
        }

        public void addChildren(StructurePiece startPiece, StructurePiecesBuilder builder, RandomSource random) {
            generateChildRight((StartPiece) startPiece, builder, random, 6, 2, false);
        }

        public static StairsRoom createPiece(StructurePiecesBuilder builder, int footX, int footY, int footZ,
                int genDepth, Direction direction) {
            BoundingBox box = BoundingBox.orientBox(footX, footY, footZ, -2, 0, 0, 7, 11, 7, direction);
            return fits(builder, box) ? new StairsRoom(genDepth, box, direction) : null;
        }

        public void postProcess(WorldGenLevel level, StructureManager structureManager, ChunkGenerator generator,
                RandomSource random, BoundingBox cb, ChunkPos chunkPos, BlockPos referencePos) {
            bricks(level, cb, 0, 0, 0, 6, 1, 6);
            hollow(level, cb, 0, 2, 0, 6, 10, 6);
            bricks(level, cb, 0, 2, 0, 1, 8, 0);
            bricks(level, cb, 5, 2, 0, 6, 8, 0);
            bricks(level, cb, 0, 2, 1, 0, 8, 6);
            bricks(level, cb, 6, 2, 1, 6, 8, 6);
            bricks(level, cb, 1, 2, 6, 5, 8, 6);
            BlockState we = fence(false, false, true, true);
            BlockState ns = fence(true, true, false, false);
            filled(level, cb, 0, 3, 2, 0, 5, 4, ns);
            filled(level, cb, 6, 3, 2, 6, 5, 2, ns);
            filled(level, cb, 6, 3, 4, 6, 5, 4, ns);
            placeBlock(level, st(Blocks.NETHER_BRICKS), 5, 2, 5, cb);
            bricks(level, cb, 4, 2, 5, 4, 3, 5);
            bricks(level, cb, 3, 2, 5, 3, 4, 5);
            bricks(level, cb, 2, 2, 5, 2, 5, 5);
            bricks(level, cb, 1, 2, 5, 1, 6, 5);
            bricks(level, cb, 1, 7, 1, 5, 7, 4);
            hollow(level, cb, 6, 8, 2, 6, 8, 4);
            bricks(level, cb, 2, 6, 0, 4, 8, 0);
            filled(level, cb, 2, 5, 0, 4, 5, 0, we);
            footings(level, cb, 0, 0, 6, 6);
        }
    }

    public static class MonsterThrone extends NetherBridgePiece {
        private boolean hasPlacedSpawner;

        public MonsterThrone(int genDepth, BoundingBox boundingBox, Direction direction) {
            super(StructurePieceType.NETHER_FORTRESS_MONSTER_THRONE, genDepth, boundingBox);
            setOrientation(direction);
        }

        public MonsterThrone(CompoundTag tag) {
            super(StructurePieceType.NETHER_FORTRESS_MONSTER_THRONE, tag);
            hasPlacedSpawner = tag.getBooleanOr("Mob", false);
        }

        protected void addAdditionalSaveData(StructurePieceSerializationContext context, CompoundTag tag) {
            super.addAdditionalSaveData(context, tag);
            tag.putBoolean("Mob", hasPlacedSpawner);
        }

        public static MonsterThrone createPiece(StructurePiecesBuilder builder, int footX, int footY, int footZ,
                int genDepth, Direction direction) {
            BoundingBox box = BoundingBox.orientBox(footX, footY, footZ, -2, 0, 0, 7, 8, 9, direction);
            return fits(builder, box) ? new MonsterThrone(genDepth, box, direction) : null;
        }

        public void postProcess(WorldGenLevel level, StructureManager structureManager, ChunkGenerator generator,
                RandomSource random, BoundingBox cb, ChunkPos chunkPos, BlockPos referencePos) {
            hollow(level, cb, 0, 2, 0, 6, 7, 7);
            bricks(level, cb, 1, 0, 0, 5, 1, 7);
            bricks(level, cb, 1, 2, 1, 5, 2, 7);
            bricks(level, cb, 1, 3, 2, 5, 3, 7);
            bricks(level, cb, 1, 4, 3, 5, 4, 7);
            bricks(level, cb, 1, 2, 0, 1, 4, 2);
            bricks(level, cb, 5, 2, 0, 5, 4, 2);
            bricks(level, cb, 1, 5, 2, 1, 5, 3);
            bricks(level, cb, 5, 5, 2, 5, 5, 3);
            bricks(level, cb, 0, 5, 3, 0, 5, 8);
            bricks(level, cb, 6, 5, 3, 6, 5, 8);
            bricks(level, cb, 1, 5, 8, 5, 5, 8);
            BlockState we = fence(false, false, true, true);
            BlockState ns = fence(true, true, false, false);
            BlockState onlyWest = fence(false, false, false, true);
            BlockState onlyEast = fence(false, false, true, false);
            placeBlock(level, onlyWest, 1, 6, 3, cb);
            placeBlock(level, onlyEast, 5, 6, 3, cb);
            placeBlock(level, fence(true, false, true, false), 0, 6, 3, cb);
            placeBlock(level, fence(true, false, false, true), 6, 6, 3, cb);
            filled(level, cb, 0, 6, 4, 0, 6, 7, ns);
            filled(level, cb, 6, 6, 4, 6, 6, 7, ns);
            placeBlock(level, fence(false, true, true, false), 0, 6, 8, cb);
            placeBlock(level, fence(false, true, false, true), 6, 6, 8, cb);
            filled(level, cb, 1, 6, 8, 5, 6, 8, we);
            placeBlock(level, onlyEast, 1, 7, 8, cb);
            filled(level, cb, 2, 7, 8, 4, 7, 8, we);
            placeBlock(level, onlyWest, 5, 7, 8, cb);
            placeBlock(level, onlyEast, 2, 8, 8, cb);
            placeBlock(level, we, 3, 8, 8, cb);
            placeBlock(level, onlyWest, 4, 8, 8, cb);
            if (!hasPlacedSpawner) {
                BlockPos pos = getWorldPos(3, 5, 5);
                if (cb.isInside(pos)) {
                    hasPlacedSpawner = true;
                    level.setBlock(pos, st(Blocks.SPAWNER), 2);
                    BlockEntity entity = level.getBlockEntity(pos);
                    if (entity instanceof SpawnerBlockEntity spawner) {
                        spawner.setEntityId(EntityTypes.BLAZE, random);
                    }
                }
            }
            footings(level, cb, 0, 0, 6, 6);
        }
    }

    abstract static class CastleHall extends NetherBridgePiece {
        CastleHall(StructurePieceType type, int genDepth, BoundingBox boundingBox, Direction direction) {
            super(type, genDepth, boundingBox);
            setOrientation(direction);
        }

        CastleHall(StructurePieceType type, CompoundTag tag) {
            super(type, tag);
        }

        protected void buildHallShell(WorldGenLevel level, BoundingBox cb) {
            bricks(level, cb, 0, 3, 0, 12, 4, 12);
            hollow(level, cb, 0, 5, 0, 12, 13, 12);
            bricks(level, cb, 0, 5, 0, 1, 12, 12);
            bricks(level, cb, 11, 5, 0, 12, 12, 12);
            bricks(level, cb, 2, 5, 11, 4, 12, 12);
            bricks(level, cb, 8, 5, 11, 10, 12, 12);
            bricks(level, cb, 5, 9, 11, 7, 12, 12);
            bricks(level, cb, 2, 5, 0, 4, 12, 1);
            bricks(level, cb, 8, 5, 0, 10, 12, 1);
            bricks(level, cb, 5, 9, 0, 7, 12, 1);
            bricks(level, cb, 2, 11, 2, 10, 12, 10);
        }

        protected void buildBattlements(WorldGenLevel level, BoundingBox cb) {
            BlockState we = fence(false, false, true, true);
            BlockState ns = fence(true, true, false, false);
            BlockState brick = st(Blocks.NETHER_BRICKS);
            for (int i = 1; i <= 11; i += 2) {
                filled(level, cb, i, 10, 0, i, 11, 0, we);
                filled(level, cb, i, 10, 12, i, 11, 12, we);
                filled(level, cb, 0, 10, i, 0, 11, i, ns);
                filled(level, cb, 12, 10, i, 12, 11, i, ns);
                placeBlock(level, brick, i, 13, 0, cb);
                placeBlock(level, brick, i, 13, 12, cb);
                placeBlock(level, brick, 0, 13, i, cb);
                placeBlock(level, brick, 12, 13, i, cb);
                if (i != 11) {
                    placeBlock(level, we, i + 1, 13, 0, cb);
                    placeBlock(level, we, i + 1, 13, 12, cb);
                    placeBlock(level, ns, 0, 13, i + 1, cb);
                    placeBlock(level, ns, 12, 13, i + 1, cb);
                }
            }
            placeBlock(level, fence(true, false, true, false), 0, 13, 0, cb);
            placeBlock(level, fence(false, true, true, false), 0, 13, 12, cb);
            placeBlock(level, fence(false, true, false, true), 12, 13, 12, cb);
            placeBlock(level, fence(true, false, false, true), 12, 13, 0, cb);
        }

        protected void buildUndercroft(WorldGenLevel level, BoundingBox cb) {
            bricks(level, cb, 4, 2, 0, 8, 2, 12);
            bricks(level, cb, 0, 2, 4, 12, 2, 8);
            bricks(level, cb, 4, 0, 0, 8, 1, 3);
            bricks(level, cb, 4, 0, 9, 8, 1, 12);
            bricks(level, cb, 0, 0, 4, 3, 1, 8);
            bricks(level, cb, 9, 0, 4, 12, 1, 8);
            footings(level, cb, 4, 0, 8, 2);
            footings(level, cb, 4, 10, 8, 12);
            footings(level, cb, 0, 4, 2, 8);
            footings(level, cb, 10, 4, 12, 8);
        }
    }

    public static class CastleEntrance extends CastleHall {
        public CastleEntrance(int genDepth, RandomSource random, BoundingBox boundingBox, Direction direction) {
            super(StructurePieceType.NETHER_FORTRESS_CASTLE_ENTRANCE, genDepth, boundingBox, direction);
        }

        public CastleEntrance(CompoundTag tag) {
            super(StructurePieceType.NETHER_FORTRESS_CASTLE_ENTRANCE, tag);
        }

        public void addChildren(StructurePiece startPiece, StructurePiecesBuilder builder, RandomSource random) {
            generateChildForward((StartPiece) startPiece, builder, random, 5, 3, true);
        }

        public static CastleEntrance createPiece(StructurePiecesBuilder builder, RandomSource random, int footX,
                int footY, int footZ, Direction direction, int genDepth) {
            BoundingBox box = BoundingBox.orientBox(footX, footY, footZ, -5, -3, 0, 13, 14, 13, direction);
            return fits(builder, box) ? new CastleEntrance(genDepth, random, box, direction) : null;
        }

        public void postProcess(WorldGenLevel level, StructureManager structureManager, ChunkGenerator generator,
                RandomSource random, BoundingBox cb, ChunkPos chunkPos, BlockPos referencePos) {
            buildHallShell(level, cb);
            filled(level, cb, 5, 8, 0, 7, 8, 0, st(Blocks.NETHER_BRICK_FENCE));
            buildBattlements(level, cb);
            BlockState westPost = fence(true, true, false, true);
            BlockState eastPost = fence(true, true, true, false);
            for (int z = 3; z <= 9; z += 2) {
                filled(level, cb, 1, 7, z, 1, 8, z, westPost);
                filled(level, cb, 11, 7, z, 11, 8, z, eastPost);
            }
            buildUndercroft(level, cb);
            bricks(level, cb, 5, 5, 5, 7, 5, 7);
            hollow(level, cb, 6, 1, 6, 6, 4, 6);
            placeBlock(level, st(Blocks.NETHER_BRICKS), 6, 0, 6, cb);
            placeBlock(level, st(Blocks.LAVA), 6, 5, 6, cb);
            BlockPos pos = getWorldPos(6, 5, 6);
            if (cb.isInside(pos)) {
                level.scheduleTick(pos, Fluids.LAVA, 0);
            }
        }
    }

    public static class CastleStalkRoom extends CastleHall {
        public CastleStalkRoom(int genDepth, BoundingBox boundingBox, Direction direction) {
            super(StructurePieceType.NETHER_FORTRESS_CASTLE_STALK_ROOM, genDepth, boundingBox, direction);
        }

        public CastleStalkRoom(CompoundTag tag) {
            super(StructurePieceType.NETHER_FORTRESS_CASTLE_STALK_ROOM, tag);
        }

        public void addChildren(StructurePiece startPiece, StructurePiecesBuilder builder, RandomSource random) {
            StartPiece start = (StartPiece) startPiece;
            generateChildForward(start, builder, random, 5, 3, true);
            generateChildForward(start, builder, random, 5, 11, true);
        }

        public static CastleStalkRoom createPiece(StructurePiecesBuilder builder, int footX, int footY, int footZ,
                Direction direction, int genDepth) {
            BoundingBox box = BoundingBox.orientBox(footX, footY, footZ, -5, -3, 0, 13, 14, 13, direction);
            return fits(builder, box) ? new CastleStalkRoom(genDepth, box, direction) : null;
        }

        public void postProcess(WorldGenLevel level, StructureManager structureManager, ChunkGenerator generator,
                RandomSource random, BoundingBox cb, ChunkPos chunkPos, BlockPos referencePos) {
            buildHallShell(level, cb);
            buildBattlements(level, cb);
            BlockState westPost = fence(true, true, false, true);
            BlockState eastPost = fence(true, true, true, false);
            for (int z = 3; z <= 9; z += 2) {
                filled(level, cb, 1, 7, z, 1, 8, z, westPost);
                filled(level, cb, 11, 7, z, 11, 8, z, eastPost);
            }
            BlockState stairs = st(Blocks.NETHER_BRICK_STAIRS).setValue(BlockStateProperties.HORIZONTAL_FACING,
                Direction.NORTH);
            for (int i = 0; i <= 6; i++) {
                int z = i + 4;
                for (int x = 5; x <= 7; x++) {
                    placeBlock(level, stairs, x, 5 + i, z, cb);
                }
                if (z >= 5 && z <= 8) {
                    bricks(level, cb, 5, 5, z, 7, i + 4, z);
                } else if (z >= 9 && z <= 10) {
                    bricks(level, cb, 5, 8, z, 7, i + 4, z);
                }
                if (i >= 1) {
                    hollow(level, cb, 5, 6 + i, z, 7, 9 + i, z);
                }
            }
            for (int x = 5; x <= 7; x++) {
                placeBlock(level, stairs, x, 12, 11, cb);
            }
            filled(level, cb, 5, 6, 7, 5, 7, 7, eastPost);
            filled(level, cb, 7, 6, 7, 7, 7, 7, westPost);
            hollow(level, cb, 5, 13, 12, 7, 13, 12);
            bricks(level, cb, 2, 5, 2, 3, 5, 3);
            bricks(level, cb, 2, 5, 9, 3, 5, 10);
            bricks(level, cb, 2, 5, 4, 2, 5, 8);
            bricks(level, cb, 9, 5, 2, 10, 5, 3);
            bricks(level, cb, 9, 5, 9, 10, 5, 10);
            bricks(level, cb, 10, 5, 4, 10, 5, 8);
            BlockState eastStairs = stairs.setValue(BlockStateProperties.HORIZONTAL_FACING, Direction.EAST);
            BlockState westStairs = stairs.setValue(BlockStateProperties.HORIZONTAL_FACING, Direction.WEST);
            for (int z : new int[] {2, 3, 9, 10}) {
                placeBlock(level, westStairs, 4, 5, z, cb);
                placeBlock(level, eastStairs, 8, 5, z, cb);
            }
            filled(level, cb, 3, 4, 4, 4, 4, 8, st(Blocks.SOUL_SAND));
            filled(level, cb, 8, 4, 4, 9, 4, 8, st(Blocks.SOUL_SAND));
            filled(level, cb, 3, 5, 4, 4, 5, 8, st(Blocks.NETHER_WART));
            filled(level, cb, 8, 5, 4, 9, 5, 8, st(Blocks.NETHER_WART));
            buildUndercroft(level, cb);
        }
    }

    public static class CastleSmallCorridorPiece extends NetherBridgePiece {
        public CastleSmallCorridorPiece(int genDepth, BoundingBox boundingBox, Direction direction) {
            super(StructurePieceType.NETHER_FORTRESS_CASTLE_SMALL_CORRIDOR, genDepth, boundingBox);
            setOrientation(direction);
        }

        public CastleSmallCorridorPiece(CompoundTag tag) {
            super(StructurePieceType.NETHER_FORTRESS_CASTLE_SMALL_CORRIDOR, tag);
        }

        public void addChildren(StructurePiece startPiece, StructurePiecesBuilder builder, RandomSource random) {
            generateChildForward((StartPiece) startPiece, builder, random, 1, 0, true);
        }

        public static CastleSmallCorridorPiece createPiece(StructurePiecesBuilder builder, int footX, int footY,
                int footZ, Direction direction, int genDepth) {
            BoundingBox box = BoundingBox.orientBox(footX, footY, footZ, -1, 0, 0, 5, 7, 5, direction);
            return fits(builder, box) ? new CastleSmallCorridorPiece(genDepth, box, direction) : null;
        }

        public void postProcess(WorldGenLevel level, StructureManager structureManager, ChunkGenerator generator,
                RandomSource random, BoundingBox cb, ChunkPos chunkPos, BlockPos referencePos) {
            bricks(level, cb, 0, 0, 0, 4, 1, 4);
            hollow(level, cb, 0, 2, 0, 4, 5, 4);
            BlockState ns = fence(true, true, false, false);
            bricks(level, cb, 0, 2, 0, 0, 5, 4);
            bricks(level, cb, 4, 2, 0, 4, 5, 4);
            filled(level, cb, 0, 3, 1, 0, 4, 1, ns);
            filled(level, cb, 0, 3, 3, 0, 4, 3, ns);
            filled(level, cb, 4, 3, 1, 4, 4, 1, ns);
            filled(level, cb, 4, 3, 3, 4, 4, 3, ns);
            bricks(level, cb, 0, 6, 0, 4, 6, 4);
            footings(level, cb, 0, 0, 4, 4);
        }
    }

    public static class CastleSmallCorridorCrossingPiece extends NetherBridgePiece {
        public CastleSmallCorridorCrossingPiece(int genDepth, BoundingBox boundingBox, Direction direction) {
            super(StructurePieceType.NETHER_FORTRESS_CASTLE_SMALL_CORRIDOR_CROSSING, genDepth, boundingBox);
            setOrientation(direction);
        }

        public CastleSmallCorridorCrossingPiece(CompoundTag tag) {
            super(StructurePieceType.NETHER_FORTRESS_CASTLE_SMALL_CORRIDOR_CROSSING, tag);
        }

        public void addChildren(StructurePiece startPiece, StructurePiecesBuilder builder, RandomSource random) {
            StartPiece start = (StartPiece) startPiece;
            generateChildForward(start, builder, random, 1, 0, true);
            generateChildLeft(start, builder, random, 0, 1, true);
            generateChildRight(start, builder, random, 0, 1, true);
        }

        public static CastleSmallCorridorCrossingPiece createPiece(StructurePiecesBuilder builder, int footX,
                int footY, int footZ, Direction direction, int genDepth) {
            BoundingBox box = BoundingBox.orientBox(footX, footY, footZ, -1, 0, 0, 5, 7, 5, direction);
            return fits(builder, box) ? new CastleSmallCorridorCrossingPiece(genDepth, box, direction) : null;
        }

        public void postProcess(WorldGenLevel level, StructureManager structureManager, ChunkGenerator generator,
                RandomSource random, BoundingBox cb, ChunkPos chunkPos, BlockPos referencePos) {
            bricks(level, cb, 0, 0, 0, 4, 1, 4);
            hollow(level, cb, 0, 2, 0, 4, 5, 4);
            bricks(level, cb, 0, 2, 0, 0, 5, 0);
            bricks(level, cb, 4, 2, 0, 4, 5, 0);
            bricks(level, cb, 0, 2, 4, 0, 5, 4);
            bricks(level, cb, 4, 2, 4, 4, 5, 4);
            bricks(level, cb, 0, 6, 0, 4, 6, 4);
            footings(level, cb, 0, 0, 4, 4);
        }
    }

    abstract static class CastleTurn extends NetherBridgePiece {
        protected boolean isNeedingChest;

        CastleTurn(StructurePieceType type, int genDepth, RandomSource random, BoundingBox boundingBox,
                Direction direction) {
            super(type, genDepth, boundingBox);
            setOrientation(direction);
            isNeedingChest = random.nextInt(3) == 0;
        }

        CastleTurn(StructurePieceType type, CompoundTag tag) {
            super(type, tag);
            isNeedingChest = tag.getBooleanOr("Chest", false);
        }

        protected void addAdditionalSaveData(StructurePieceSerializationContext context, CompoundTag tag) {
            super.addAdditionalSaveData(context, tag);
            tag.putBoolean("Chest", isNeedingChest);
        }

        protected void maybeChest(WorldGenLevel level, BoundingBox cb, RandomSource random, int x) {
            if (isNeedingChest && cb.isInside(getWorldPos(x, 2, 3))) {
                isNeedingChest = false;
                createChest(level, cb, random, x, 2, 3, BuiltInLootTables.NETHER_BRIDGE);
            }
        }
    }

    public static class CastleSmallCorridorRightTurnPiece extends CastleTurn {
        public CastleSmallCorridorRightTurnPiece(int genDepth, RandomSource random, BoundingBox boundingBox,
                Direction direction) {
            super(StructurePieceType.NETHER_FORTRESS_CASTLE_SMALL_CORRIDOR_RIGHT_TURN, genDepth, random, boundingBox,
                direction);
        }

        public CastleSmallCorridorRightTurnPiece(CompoundTag tag) {
            super(StructurePieceType.NETHER_FORTRESS_CASTLE_SMALL_CORRIDOR_RIGHT_TURN, tag);
        }

        public void addChildren(StructurePiece startPiece, StructurePiecesBuilder builder, RandomSource random) {
            generateChildRight((StartPiece) startPiece, builder, random, 0, 1, true);
        }

        public static CastleSmallCorridorRightTurnPiece createPiece(StructurePiecesBuilder builder,
                RandomSource random, int footX, int footY, int footZ, Direction direction, int genDepth) {
            BoundingBox box = BoundingBox.orientBox(footX, footY, footZ, -1, 0, 0, 5, 7, 5, direction);
            return fits(builder, box) ? new CastleSmallCorridorRightTurnPiece(genDepth, random, box, direction) : null;
        }

        public void postProcess(WorldGenLevel level, StructureManager structureManager, ChunkGenerator generator,
                RandomSource random, BoundingBox cb, ChunkPos chunkPos, BlockPos referencePos) {
            bricks(level, cb, 0, 0, 0, 4, 1, 4);
            hollow(level, cb, 0, 2, 0, 4, 5, 4);
            BlockState we = fence(false, false, true, true);
            BlockState ns = fence(true, true, false, false);
            bricks(level, cb, 0, 2, 0, 0, 5, 4);
            filled(level, cb, 0, 3, 1, 0, 4, 1, ns);
            filled(level, cb, 0, 3, 3, 0, 4, 3, ns);
            bricks(level, cb, 4, 2, 0, 4, 5, 0);
            bricks(level, cb, 1, 2, 4, 4, 5, 4);
            filled(level, cb, 1, 3, 4, 1, 4, 4, we);
            filled(level, cb, 3, 3, 4, 3, 4, 4, we);
            maybeChest(level, cb, random, 1);
            bricks(level, cb, 0, 6, 0, 4, 6, 4);
            footings(level, cb, 0, 0, 4, 4);
        }
    }

    public static class CastleSmallCorridorLeftTurnPiece extends CastleTurn {
        public CastleSmallCorridorLeftTurnPiece(int genDepth, RandomSource random, BoundingBox boundingBox,
                Direction direction) {
            super(StructurePieceType.NETHER_FORTRESS_CASTLE_SMALL_CORRIDOR_LEFT_TURN, genDepth, random, boundingBox,
                direction);
        }

        public CastleSmallCorridorLeftTurnPiece(CompoundTag tag) {
            super(StructurePieceType.NETHER_FORTRESS_CASTLE_SMALL_CORRIDOR_LEFT_TURN, tag);
        }

        public void addChildren(StructurePiece startPiece, StructurePiecesBuilder builder, RandomSource random) {
            generateChildLeft((StartPiece) startPiece, builder, random, 0, 1, true);
        }

        public static CastleSmallCorridorLeftTurnPiece createPiece(StructurePiecesBuilder builder, RandomSource random,
                int footX, int footY, int footZ, Direction direction, int genDepth) {
            BoundingBox box = BoundingBox.orientBox(footX, footY, footZ, -1, 0, 0, 5, 7, 5, direction);
            return fits(builder, box) ? new CastleSmallCorridorLeftTurnPiece(genDepth, random, box, direction) : null;
        }

        public void postProcess(WorldGenLevel level, StructureManager structureManager, ChunkGenerator generator,
                RandomSource random, BoundingBox cb, ChunkPos chunkPos, BlockPos referencePos) {
            bricks(level, cb, 0, 0, 0, 4, 1, 4);
            hollow(level, cb, 0, 2, 0, 4, 5, 4);
            BlockState we = fence(false, false, true, true);
            BlockState ns = fence(true, true, false, false);
            bricks(level, cb, 4, 2, 0, 4, 5, 4);
            filled(level, cb, 4, 3, 1, 4, 4, 1, ns);
            filled(level, cb, 4, 3, 3, 4, 4, 3, ns);
            bricks(level, cb, 0, 2, 0, 0, 5, 0);
            bricks(level, cb, 0, 2, 4, 3, 5, 4);
            filled(level, cb, 1, 3, 4, 1, 4, 4, we);
            filled(level, cb, 3, 3, 4, 3, 4, 4, we);
            maybeChest(level, cb, random, 3);
            bricks(level, cb, 0, 6, 0, 4, 6, 4);
            footings(level, cb, 0, 0, 4, 4);
        }
    }

    public static class CastleCorridorStairsPiece extends NetherBridgePiece {
        public CastleCorridorStairsPiece(int genDepth, BoundingBox boundingBox, Direction direction) {
            super(StructurePieceType.NETHER_FORTRESS_CASTLE_CORRIDOR_STAIRS, genDepth, boundingBox);
            setOrientation(direction);
        }

        public CastleCorridorStairsPiece(CompoundTag tag) {
            super(StructurePieceType.NETHER_FORTRESS_CASTLE_CORRIDOR_STAIRS, tag);
        }

        public void addChildren(StructurePiece startPiece, StructurePiecesBuilder builder, RandomSource random) {
            generateChildForward((StartPiece) startPiece, builder, random, 1, 0, true);
        }

        public static CastleCorridorStairsPiece createPiece(StructurePiecesBuilder builder, int footX, int footY,
                int footZ, Direction direction, int genDepth) {
            BoundingBox box = BoundingBox.orientBox(footX, footY, footZ, -1, -7, 0, 5, 14, 10, direction);
            return fits(builder, box) ? new CastleCorridorStairsPiece(genDepth, box, direction) : null;
        }

        public void postProcess(WorldGenLevel level, StructureManager structureManager, ChunkGenerator generator,
                RandomSource random, BoundingBox cb, ChunkPos chunkPos, BlockPos referencePos) {
            BlockState stairs = st(Blocks.NETHER_BRICK_STAIRS).setValue(BlockStateProperties.HORIZONTAL_FACING,
                Direction.SOUTH);
            BlockState ns = fence(true, true, false, false);
            for (int step = 0; step <= 9; step++) {
                int floor = Math.max(1, 7 - step);
                int roof = Math.min(Math.max(floor + 5, 14 - step), 13);
                bricks(level, cb, 0, 0, step, 4, floor, step);
                hollow(level, cb, 1, floor + 1, step, 3, roof - 1, step);
                if (step <= 6) {
                    for (int x = 1; x <= 3; x++) {
                        placeBlock(level, stairs, x, floor + 1, step, cb);
                    }
                }
                bricks(level, cb, 0, roof, step, 4, roof, step);
                bricks(level, cb, 0, floor + 1, step, 0, roof - 1, step);
                bricks(level, cb, 4, floor + 1, step, 4, roof - 1, step);
                if ((step & 1) == 0) {
                    filled(level, cb, 0, floor + 2, step, 0, floor + 3, step, ns);
                    filled(level, cb, 4, floor + 2, step, 4, floor + 3, step, ns);
                }
                footings(level, cb, 0, step, 4, step);
            }
        }
    }

    public static class CastleCorridorTBalconyPiece extends NetherBridgePiece {
        public CastleCorridorTBalconyPiece(int genDepth, BoundingBox boundingBox, Direction direction) {
            super(StructurePieceType.NETHER_FORTRESS_CASTLE_CORRIDOR_T_BALCONY, genDepth, boundingBox);
            setOrientation(direction);
        }

        public CastleCorridorTBalconyPiece(CompoundTag tag) {
            super(StructurePieceType.NETHER_FORTRESS_CASTLE_CORRIDOR_T_BALCONY, tag);
        }

        public void addChildren(StructurePiece startPiece, StructurePiecesBuilder builder, RandomSource random) {
            Direction facing = getOrientation();
            int zOff = facing == Direction.WEST || facing == Direction.NORTH ? 5 : 1;
            StartPiece start = (StartPiece) startPiece;
            generateChildLeft(start, builder, random, 0, zOff, random.nextInt(8) > 0);
            generateChildRight(start, builder, random, 0, zOff, random.nextInt(8) > 0);
        }

        public static CastleCorridorTBalconyPiece createPiece(StructurePiecesBuilder builder, int footX, int footY,
                int footZ, Direction direction, int genDepth) {
            BoundingBox box = BoundingBox.orientBox(footX, footY, footZ, -3, 0, 0, 9, 7, 9, direction);
            return fits(builder, box) ? new CastleCorridorTBalconyPiece(genDepth, box, direction) : null;
        }

        public void postProcess(WorldGenLevel level, StructureManager structureManager, ChunkGenerator generator,
                RandomSource random, BoundingBox cb, ChunkPos chunkPos, BlockPos referencePos) {
            BlockState ns = fence(true, true, false, false);
            BlockState we = fence(false, false, true, true);
            bricks(level, cb, 0, 0, 0, 8, 1, 8);
            hollow(level, cb, 0, 2, 0, 8, 5, 8);
            bricks(level, cb, 0, 6, 0, 8, 6, 5);
            bricks(level, cb, 0, 2, 0, 2, 5, 0);
            bricks(level, cb, 6, 2, 0, 8, 5, 0);
            filled(level, cb, 1, 3, 0, 1, 4, 0, we);
            filled(level, cb, 7, 3, 0, 7, 4, 0, we);
            bricks(level, cb, 0, 2, 4, 8, 2, 8);
            hollow(level, cb, 1, 1, 4, 2, 2, 4);
            hollow(level, cb, 6, 1, 4, 7, 2, 4);
            filled(level, cb, 1, 3, 8, 7, 3, 8, we);
            placeBlock(level, fence(false, true, true, false), 0, 3, 8, cb);
            placeBlock(level, fence(false, true, false, true), 8, 3, 8, cb);
            filled(level, cb, 0, 3, 6, 0, 3, 7, ns);
            filled(level, cb, 8, 3, 6, 8, 3, 7, ns);
            bricks(level, cb, 0, 3, 4, 0, 5, 5);
            bricks(level, cb, 8, 3, 4, 8, 5, 5);
            bricks(level, cb, 1, 3, 5, 2, 5, 5);
            bricks(level, cb, 6, 3, 5, 7, 5, 5);
            filled(level, cb, 1, 4, 5, 1, 5, 5, we);
            filled(level, cb, 7, 4, 5, 7, 5, 5, we);
            footings(level, cb, 0, 0, 8, 5);
        }
    }
}
