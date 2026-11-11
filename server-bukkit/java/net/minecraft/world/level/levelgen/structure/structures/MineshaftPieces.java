// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.structure.structures;

import java.util.ArrayList;
import java.util.List;
import net.minecraft.core.BlockPos;
import net.minecraft.core.Direction;
import net.minecraft.nbt.CompoundTag;
import net.minecraft.resources.ResourceKey;
import net.minecraft.tags.BiomeTags;
import net.minecraft.util.RandomSource;
import net.minecraft.world.entity.EntityTypes;
import net.minecraft.world.entity.EntitySpawnReason;
import net.minecraft.world.entity.vehicle.minecart.MinecartChest;
import net.minecraft.world.level.BlockGetter;
import net.minecraft.world.level.ChunkPos;
import net.minecraft.world.level.LevelAccessor;
import net.minecraft.world.level.LevelReader;
import net.minecraft.world.level.StructureManager;
import net.minecraft.world.level.WorldGenLevel;
import net.minecraft.world.level.block.Block;
import net.minecraft.world.level.block.Blocks;
import net.minecraft.world.level.block.FallingBlock;
import net.minecraft.world.level.block.entity.BlockEntity;
import net.minecraft.world.level.block.entity.SpawnerBlockEntity;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.block.state.properties.BlockStateProperties;
import net.minecraft.world.level.block.state.properties.RailShape;
import net.minecraft.world.level.chunk.ChunkGenerator;
import net.minecraft.world.level.levelgen.structure.BoundingBox;
import net.minecraft.world.level.levelgen.structure.StructurePiece;
import net.minecraft.world.level.levelgen.structure.pieces.StructurePieceSerializationContext;
import net.minecraft.world.level.levelgen.structure.pieces.StructurePieceType;
import net.minecraft.world.level.levelgen.structure.pieces.StructurePiecesBuilder;
import net.minecraft.world.level.storage.loot.BuiltInLootTables;
import net.minecraft.world.level.storage.loot.LootTable;

public class MineshaftPieces {
    public static final int MAGIC_START_Y = 50;
    private static final int kMaxDepth = 8;
    private static final int kMaxReach = 80;

    private static MineShaftPiece pickPiece(StructurePiecesBuilder builder, RandomSource random, int x, int y, int z,
            Direction dir, int depth, MineshaftStructure.Type type) {
        int roll = random.nextInt(100);
        if (roll >= 80) {
            BoundingBox box = MineShaftCrossing.findCrossing(builder, random, x, y, z, dir);
            return box == null ? null : new MineShaftCrossing(depth, box, dir, type);
        }
        if (roll >= 70) {
            BoundingBox box = MineShaftStairs.findStairs(builder, random, x, y, z, dir);
            return box == null ? null : new MineShaftStairs(depth, box, dir, type);
        }
        BoundingBox box = MineShaftCorridor.findCorridorSize(builder, random, x, y, z, dir);
        return box == null ? null : new MineShaftCorridor(depth, random, box, dir, type);
    }

    private static MineShaftPiece generateAndAddPiece(StructurePiece start, StructurePiecesBuilder builder,
            RandomSource random, int x, int y, int z, Direction dir, int depth) {
        if (depth > kMaxDepth) {
            return null;
        }
        BoundingBox startBox = start.getBoundingBox();
        if (Math.abs(x - startBox.minX()) > kMaxReach || Math.abs(z - startBox.minZ()) > kMaxReach) {
            return null;
        }
        MineShaftPiece piece = pickPiece(builder, random, x, y, z, dir, depth + 1, ((MineShaftPiece) start).type);
        if (piece != null) {
            builder.addPiece(piece);
            piece.addChildren(start, builder, random);
        }
        return piece;
    }

    static abstract class MineShaftPiece extends StructurePiece {
        protected MineshaftStructure.Type type;

        public MineShaftPiece(StructurePieceType pieceType, int genDepth, MineshaftStructure.Type type,
                BoundingBox boundingBox) {
            super(pieceType, genDepth, boundingBox);
            this.type = type;
        }

        public MineShaftPiece(StructurePieceType pieceType, CompoundTag tag) {
            super(pieceType, tag);
            this.type = MineshaftStructure.Type.byId(tag.getIntOr("MST", 0));
        }

        protected boolean canBeReplaced(LevelReader level, int x, int y, int z, BoundingBox chunkBB) {
            BlockState state = getBlock(level, x, y, z, chunkBB);
            return !state.is(type.getPlanksState().getBlock()) && !state.is(type.getWoodState().getBlock())
                && !state.is(type.getFenceState().getBlock()) && !state.is(Blocks.IRON_CHAIN);
        }

        protected void addAdditionalSaveData(StructurePieceSerializationContext context, CompoundTag tag) {
            tag.putInt("MST", type.ordinal());
        }

        protected boolean isSupportingBox(BlockGetter level, BoundingBox chunkBB, int x0, int x1, int y1, int z0) {
            for (int x = x0; x <= x1; x++) {
                if (getBlock(level, x, y1 + 1, z0, chunkBB).isAir()) {
                    return false;
                }
            }
            return true;
        }

        protected boolean isInInvalidLocation(LevelAccessor level, BoundingBox chunkBB) {
            int x0 = Math.max(boundingBox.minX() - 1, chunkBB.minX());
            int y0 = Math.max(boundingBox.minY() - 1, chunkBB.minY());
            int z0 = Math.max(boundingBox.minZ() - 1, chunkBB.minZ());
            int x1 = Math.min(boundingBox.maxX() + 1, chunkBB.maxX());
            int y1 = Math.min(boundingBox.maxY() + 1, chunkBB.maxY());
            int z1 = Math.min(boundingBox.maxZ() + 1, chunkBB.maxZ());
            BlockPos.MutableBlockPos cursor = new BlockPos.MutableBlockPos((x0 + x1) / 2, (y0 + y1) / 2, (z0 + z1) / 2);
            if (level.getBiome(cursor).is(BiomeTags.MINESHAFT_BLOCKING)) {
                return true;
            }
            for (int a = x0; a <= x1; a++) {
                for (int b = z0; b <= z1; b++) {
                    if (wet(level, cursor.set(a, y0, b)) || wet(level, cursor.set(a, y1, b))) {
                        return true;
                    }
                }
            }
            for (int a = x0; a <= x1; a++) {
                for (int b = y0; b <= y1; b++) {
                    if (wet(level, cursor.set(a, b, z0)) || wet(level, cursor.set(a, b, z1))) {
                        return true;
                    }
                }
            }
            for (int a = z0; a <= z1; a++) {
                for (int b = y0; b <= y1; b++) {
                    if (wet(level, cursor.set(x0, b, a)) || wet(level, cursor.set(x1, b, a))) {
                        return true;
                    }
                }
            }
            return false;
        }

        private static boolean wet(LevelAccessor level, BlockPos pos) {
            return level.getBlockState(pos).liquid();
        }

        protected void setPlanksBlock(WorldGenLevel level, BoundingBox chunkBB, BlockState planksBlock, int x, int y,
                int z) {
            if (!isInterior(level, x, y, z, chunkBB)) {
                return;
            }
            BlockPos pos = getWorldPos(x, y, z);
            if (!level.getBlockState(pos).isFaceSturdy(level, pos, Direction.UP)) {
                level.setBlock(pos, planksBlock, 2);
            }
        }
    }

    public static class MineShaftRoom extends MineShaftPiece {
        private final List<BoundingBox> childEntranceBoxes = new ArrayList<>();

        public MineShaftRoom(int genDepth, RandomSource random, int west, int north, MineshaftStructure.Type type) {
            super(StructurePieceType.MINE_SHAFT_ROOM, genDepth, type,
                new BoundingBox(west, 50, north, west + 7 + random.nextInt(6), 54 + random.nextInt(6),
                    north + 7 + random.nextInt(6)));
        }

        public MineShaftRoom(CompoundTag tag) {
            super(StructurePieceType.MINE_SHAFT_ROOM, tag);
            for (var entry : tag.getListOrEmpty("Entrances")) {
                entry.asIntArray().filter(v -> v.length == 6).ifPresent(v -> childEntranceBoxes
                    .add(new BoundingBox(v[0], v[1], v[2], v[3], v[4], v[5])));
            }
        }

        public void addChildren(StructurePiece startPiece, StructurePiecesBuilder builder, RandomSource random) {
            int depth = getGenDepth();
            int heightSpace = Math.max(boundingBox.getYSpan() - 4, 1);
            int spanX = boundingBox.getXSpan();
            int spanZ = boundingBox.getZSpan();
            for (int pos = 0; pos < spanX; pos += 4) {
                pos += random.nextInt(spanX);
                if (pos + 3 > spanX) {
                    break;
                }
                MineShaftPiece child = generateAndAddPiece(startPiece, builder, random, boundingBox.minX() + pos,
                    boundingBox.minY() + random.nextInt(heightSpace) + 1, boundingBox.minZ() - 1, Direction.NORTH, depth);
                if (child != null) {
                    BoundingBox cb = child.getBoundingBox();
                    childEntranceBoxes.add(new BoundingBox(cb.minX(), cb.minY(), boundingBox.minZ(), cb.maxX(),
                        cb.maxY(), boundingBox.minZ() + 1));
                }
            }
            for (int pos = 0; pos < spanX; pos += 4) {
                pos += random.nextInt(spanX);
                if (pos + 3 > spanX) {
                    break;
                }
                MineShaftPiece child = generateAndAddPiece(startPiece, builder, random, boundingBox.minX() + pos,
                    boundingBox.minY() + random.nextInt(heightSpace) + 1, boundingBox.maxZ() + 1, Direction.SOUTH, depth);
                if (child != null) {
                    BoundingBox cb = child.getBoundingBox();
                    childEntranceBoxes.add(new BoundingBox(cb.minX(), cb.minY(), boundingBox.maxZ() - 1, cb.maxX(),
                        cb.maxY(), boundingBox.maxZ()));
                }
            }
            for (int pos = 0; pos < spanZ; pos += 4) {
                pos += random.nextInt(spanZ);
                if (pos + 3 > spanZ) {
                    break;
                }
                MineShaftPiece child = generateAndAddPiece(startPiece, builder, random, boundingBox.minX() - 1,
                    boundingBox.minY() + random.nextInt(heightSpace) + 1, boundingBox.minZ() + pos, Direction.WEST, depth);
                if (child != null) {
                    BoundingBox cb = child.getBoundingBox();
                    childEntranceBoxes.add(new BoundingBox(boundingBox.minX(), cb.minY(), cb.minZ(),
                        boundingBox.minX() + 1, cb.maxY(), cb.maxZ()));
                }
            }
            for (int pos = 0; pos < spanZ; pos += 4) {
                pos += random.nextInt(spanZ);
                if (pos + 3 > spanZ) {
                    break;
                }
                MineShaftPiece child = generateAndAddPiece(startPiece, builder, random, boundingBox.maxX() + 1,
                    boundingBox.minY() + random.nextInt(heightSpace) + 1, boundingBox.minZ() + pos, Direction.EAST, depth);
                if (child != null) {
                    BoundingBox cb = child.getBoundingBox();
                    childEntranceBoxes.add(new BoundingBox(boundingBox.maxX() - 1, cb.minY(), cb.minZ(),
                        boundingBox.maxX(), cb.maxY(), cb.maxZ()));
                }
            }
        }

        public void postProcess(WorldGenLevel level, StructureManager structureManager, ChunkGenerator generator,
                RandomSource random, BoundingBox chunkBB, ChunkPos chunkPos, BlockPos referencePos) {
            if (isInInvalidLocation(level, chunkBB)) {
                return;
            }
            BoundingBox bb = boundingBox;
            generateBox(level, chunkBB, bb.minX(), bb.minY() + 1, bb.minZ(), bb.maxX(),
                Math.min(bb.minY() + 3, bb.maxY()), bb.maxZ(), CAVE_AIR, CAVE_AIR, false);
            for (BoundingBox e : childEntranceBoxes) {
                generateBox(level, chunkBB, e.minX(), e.maxY() - 2, e.minZ(), e.maxX(), e.maxY(), e.maxZ(), CAVE_AIR,
                    CAVE_AIR, false);
            }
            generateUpperHalfSphere(level, chunkBB, bb.minX(), bb.minY() + 4, bb.minZ(), bb.maxX(), bb.maxY(),
                bb.maxZ(), CAVE_AIR, false);
        }

        public void move(int dx, int dy, int dz) {
            super.move(dx, dy, dz);
            for (BoundingBox e : childEntranceBoxes) {
                e.move(dx, dy, dz);
            }
        }

        protected void addAdditionalSaveData(StructurePieceSerializationContext context, CompoundTag tag) {
            super.addAdditionalSaveData(context, tag);
            var list = new net.minecraft.nbt.ListTag();
            for (BoundingBox e : childEntranceBoxes) {
                list.add(new net.minecraft.nbt.IntArrayTag(
                    new int[] {e.minX(), e.minY(), e.minZ(), e.maxX(), e.maxY(), e.maxZ()}));
            }
            tag.put("Entrances", list);
        }
    }

    public static class MineShaftCorridor extends MineShaftPiece {
        private final boolean hasRails;
        private final boolean spiderCorridor;
        private boolean hasPlacedSpider;
        private final int numSections;

        public MineShaftCorridor(CompoundTag tag) {
            super(StructurePieceType.MINE_SHAFT_CORRIDOR, tag);
            hasRails = tag.getBooleanOr("hr", false);
            spiderCorridor = tag.getBooleanOr("sc", false);
            hasPlacedSpider = tag.getBooleanOr("hps", false);
            numSections = tag.getIntOr("Num", 0);
        }

        protected void addAdditionalSaveData(StructurePieceSerializationContext context, CompoundTag tag) {
            super.addAdditionalSaveData(context, tag);
            tag.putBoolean("hr", hasRails);
            tag.putBoolean("sc", spiderCorridor);
            tag.putBoolean("hps", hasPlacedSpider);
            tag.putInt("Num", numSections);
        }

        public MineShaftCorridor(int genDepth, RandomSource random, BoundingBox boundingBox, Direction direction,
                MineshaftStructure.Type type) {
            super(StructurePieceType.MINE_SHAFT_CORRIDOR, genDepth, type, boundingBox);
            setOrientation(direction);
            hasRails = random.nextInt(3) == 0;
            spiderCorridor = !hasRails && random.nextInt(23) == 0;
            numSections = (getOrientation().getAxis() == Direction.Axis.Z ? boundingBox.getZSpan()
                : boundingBox.getXSpan()) / 5;
        }

        public static BoundingBox findCorridorSize(StructurePiecesBuilder builder, RandomSource random, int footX,
                int footY, int footZ, Direction direction) {
            for (int sections = random.nextInt(3) + 2; sections > 0; sections--) {
                int len = sections * 5 - 1;
                BoundingBox box = switch (direction) {
                    case SOUTH -> new BoundingBox(0, 0, 0, 2, 2, len);
                    case WEST -> new BoundingBox(-len, 0, 0, 0, 2, 2);
                    case EAST -> new BoundingBox(0, 0, 0, len, 2, 2);
                    default -> new BoundingBox(0, 0, -len, 2, 2, 0);
                };
                box.move(footX, footY, footZ);
                if (builder.findCollisionPiece(box) == null) {
                    return box;
                }
            }
            return null;
        }

        public void addChildren(StructurePiece startPiece, StructurePiecesBuilder builder, RandomSource random) {
            int depth = getGenDepth();
            int end = random.nextInt(4);
            Direction facing = getOrientation();
            BoundingBox bb = boundingBox;
            if (facing != null) {
                int y = bb.minY() - 1 + random.nextInt(3);
                switch (facing) {
                    case SOUTH -> {
                        if (end <= 1) {
                            generateAndAddPiece(startPiece, builder, random, bb.minX(), y, bb.maxZ() + 1, facing, depth);
                        } else if (end == 2) {
                            generateAndAddPiece(startPiece, builder, random, bb.minX() - 1, y, bb.maxZ() - 3,
                                Direction.WEST, depth);
                        } else {
                            generateAndAddPiece(startPiece, builder, random, bb.maxX() + 1, y, bb.maxZ() - 3,
                                Direction.EAST, depth);
                        }
                    }
                    case WEST -> {
                        if (end <= 1) {
                            generateAndAddPiece(startPiece, builder, random, bb.minX() - 1, y, bb.minZ(), facing, depth);
                        } else if (end == 2) {
                            generateAndAddPiece(startPiece, builder, random, bb.minX(), y, bb.minZ() - 1,
                                Direction.NORTH, depth);
                        } else {
                            generateAndAddPiece(startPiece, builder, random, bb.minX(), y, bb.maxZ() + 1,
                                Direction.SOUTH, depth);
                        }
                    }
                    case EAST -> {
                        if (end <= 1) {
                            generateAndAddPiece(startPiece, builder, random, bb.maxX() + 1, y, bb.minZ(), facing, depth);
                        } else if (end == 2) {
                            generateAndAddPiece(startPiece, builder, random, bb.maxX() - 3, y, bb.minZ() - 1,
                                Direction.NORTH, depth);
                        } else {
                            generateAndAddPiece(startPiece, builder, random, bb.maxX() - 3, y, bb.maxZ() + 1,
                                Direction.SOUTH, depth);
                        }
                    }
                    default -> {
                        if (end <= 1) {
                            generateAndAddPiece(startPiece, builder, random, bb.minX(), y, bb.minZ() - 1, facing, depth);
                        } else if (end == 2) {
                            generateAndAddPiece(startPiece, builder, random, bb.minX() - 1, y, bb.minZ(),
                                Direction.WEST, depth);
                        } else {
                            generateAndAddPiece(startPiece, builder, random, bb.maxX() + 1, y, bb.minZ(),
                                Direction.EAST, depth);
                        }
                    }
                }
            }
            if (depth >= kMaxDepth) {
                return;
            }
            boolean alongZ = facing == Direction.NORTH || facing == Direction.SOUTH;
            if (alongZ) {
                for (int z = bb.minZ() + 3; z + 3 <= bb.maxZ(); z += 5) {
                    int pick = random.nextInt(5);
                    if (pick == 0) {
                        generateAndAddPiece(startPiece, builder, random, bb.minX() - 1, bb.minY(), z, Direction.WEST,
                            depth + 1);
                    } else if (pick == 1) {
                        generateAndAddPiece(startPiece, builder, random, bb.maxX() + 1, bb.minY(), z, Direction.EAST,
                            depth + 1);
                    }
                }
            } else {
                for (int x = bb.minX() + 3; x + 3 <= bb.maxX(); x += 5) {
                    int pick = random.nextInt(5);
                    if (pick == 0) {
                        generateAndAddPiece(startPiece, builder, random, x, bb.minY(), bb.minZ() - 1, Direction.NORTH,
                            depth + 1);
                    } else if (pick == 1) {
                        generateAndAddPiece(startPiece, builder, random, x, bb.minY(), bb.maxZ() + 1, Direction.SOUTH,
                            depth + 1);
                    }
                }
            }
        }

        protected boolean createChest(WorldGenLevel level, BoundingBox chunkBB, RandomSource random, int x, int y,
                int z, ResourceKey<LootTable> lootTable) {
            BlockPos pos = getWorldPos(x, y, z);
            if (!chunkBB.isInside(pos) || !level.getBlockState(pos).isAir()
                || level.getBlockState(pos.below()).isAir()) {
                return false;
            }
            RailShape shape = random.nextBoolean() ? RailShape.NORTH_SOUTH : RailShape.EAST_WEST;
            placeBlock(level, Blocks.RAIL.defaultBlockState().setValue(BlockStateProperties.RAIL_SHAPE, shape), x, y,
                z, chunkBB);
            if (EntityTypes.CHEST_MINECART.create(level.getLevel(), EntitySpawnReason.CHUNK_GENERATION)
                instanceof MinecartChest cart) {
                cart.setInitialPos(pos.getX() + 0.5D, pos.getY() + 0.5D, pos.getZ() + 0.5D);
                cart.setLootTable(lootTable, random.nextLong());
                level.addFreshEntity(cart);
            }
            return true;
        }

        public void postProcess(WorldGenLevel level, StructureManager structureManager, ChunkGenerator generator,
                RandomSource random, BoundingBox chunkBB, ChunkPos chunkPos, BlockPos referencePos) {
            if (isInInvalidLocation(level, chunkBB)) {
                return;
            }
            int length = numSections * 5 - 1;
            BlockState planks = type.getPlanksState();
            generateBox(level, chunkBB, 0, 0, 0, 2, 1, length, CAVE_AIR, CAVE_AIR, false);
            generateMaybeBox(level, chunkBB, random, 0.8F, 0, 2, 0, 2, 2, length, CAVE_AIR, CAVE_AIR, false, false);
            if (spiderCorridor) {
                generateMaybeBox(level, chunkBB, random, 0.6F, 0, 0, 0, 2, 1, length,
                    Blocks.COBWEB.defaultBlockState(), CAVE_AIR, false, true);
            }
            for (int section = 0; section < numSections; section++) {
                int z = 2 + section * 5;
                placeSupport(level, chunkBB, 0, 0, z, 2, 2, random);
                webAt(level, chunkBB, random, 0.1F, 0, 2, z - 1);
                webAt(level, chunkBB, random, 0.1F, 2, 2, z - 1);
                webAt(level, chunkBB, random, 0.1F, 0, 2, z + 1);
                webAt(level, chunkBB, random, 0.1F, 2, 2, z + 1);
                webAt(level, chunkBB, random, 0.05F, 0, 2, z - 2);
                webAt(level, chunkBB, random, 0.05F, 2, 2, z - 2);
                webAt(level, chunkBB, random, 0.05F, 0, 2, z + 2);
                webAt(level, chunkBB, random, 0.05F, 2, 2, z + 2);
                if (random.nextInt(100) == 0) {
                    createChest(level, chunkBB, random, 2, 0, z - 1, BuiltInLootTables.ABANDONED_MINESHAFT);
                }
                if (random.nextInt(100) == 0) {
                    createChest(level, chunkBB, random, 0, 0, z + 1, BuiltInLootTables.ABANDONED_MINESHAFT);
                }
                if (spiderCorridor && !hasPlacedSpider) {
                    int spawnZ = z - 1 + random.nextInt(3);
                    BlockPos pos = getWorldPos(1, 0, spawnZ);
                    if (chunkBB.isInside(pos) && isInterior(level, 1, 0, spawnZ, chunkBB)) {
                        hasPlacedSpider = true;
                        level.setBlock(pos, Blocks.SPAWNER.defaultBlockState(), 2);
                        BlockEntity entity = level.getBlockEntity(pos);
                        if (entity instanceof SpawnerBlockEntity spawner) {
                            spawner.setEntityId(EntityTypes.CAVE_SPIDER, random);
                        }
                    }
                }
            }
            for (int x = 0; x <= 2; x++) {
                for (int z = 0; z <= length; z++) {
                    setPlanksBlock(level, chunkBB, planks, x, -1, z);
                }
            }
            pillarPair(level, chunkBB, 0, -1, 2);
            if (numSections > 1) {
                pillarPair(level, chunkBB, 0, -1, length - 2);
            }
            if (hasRails) {
                BlockState rail = Blocks.RAIL.defaultBlockState().setValue(BlockStateProperties.RAIL_SHAPE,
                    RailShape.NORTH_SOUTH);
                for (int z = 0; z <= length; z++) {
                    BlockState floor = getBlock(level, 1, -1, z, chunkBB);
                    if (!floor.isAir() && floor.isSolidRender()) {
                        float chance = isInterior(level, 1, 0, z, chunkBB) ? 0.7F : 0.9F;
                        maybeGenerateBlock(level, chunkBB, random, chance, 1, 0, z, rail);
                    }
                }
            }
        }

        private void pillarPair(WorldGenLevel level, BoundingBox chunkBB, int x, int y, int z) {
            BlockState wood = type.getWoodState();
            Block planks = type.getPlanksState().getBlock();
            if (getBlock(level, x, y, z, chunkBB).is(planks)) {
                fillPillarDownOrChainUp(level, wood, x, y, z, chunkBB);
            }
            if (getBlock(level, x + 2, y, z, chunkBB).is(planks)) {
                fillPillarDownOrChainUp(level, wood, x + 2, y, z, chunkBB);
            }
        }

        protected void fillColumnDown(WorldGenLevel level, BlockState columnState, int x, int startY, int z,
                BoundingBox chunkBB) {
            BlockPos.MutableBlockPos pos = getWorldPos(x, startY, z);
            if (!chunkBB.isInside(pos)) {
                return;
            }
            int top = pos.getY();
            while (isReplaceableByStructures(level.getBlockState(pos)) && pos.getY() > level.getMinY() + 1) {
                pos.move(Direction.DOWN);
            }
            if (level.getBlockState(pos).isFaceSturdy(level, pos, Direction.UP)) {
                while (pos.getY() < top) {
                    pos.move(Direction.UP);
                    level.setBlock(pos, columnState, 2);
                }
            }
        }

        protected void fillPillarDownOrChainUp(WorldGenLevel level, BlockState pillarState, int x, int y, int z,
                BoundingBox chunkBB) {
            BlockPos.MutableBlockPos pos = getWorldPos(x, y, z);
            if (!chunkBB.isInside(pos)) {
                return;
            }
            int originY = pos.getY();
            boolean below = true;
            boolean above = true;
            for (int step = 1; below || above; step++) {
                if (below) {
                    pos.setY(originY - step);
                    BlockState state = level.getBlockState(pos);
                    boolean open = isReplaceableByStructures(state) && !state.is(Blocks.LAVA);
                    if (!open && state.isFaceSturdy(level, pos, Direction.UP)) {
                        fillBetween(level, pillarState, pos, originY - step + 1, originY);
                        return;
                    }
                    below = step <= 20 && open && pos.getY() > level.getMinY() + 1;
                }
                if (above) {
                    pos.setY(originY + step);
                    BlockState state = level.getBlockState(pos);
                    boolean open = isReplaceableByStructures(state);
                    if (!open && Block.canSupportCenter(level, pos, Direction.DOWN)
                        && !(state.getBlock() instanceof FallingBlock)) {
                        level.setBlock(pos.setY(originY + 1), type.getFenceState(), 2);
                        fillBetween(level, Blocks.IRON_CHAIN.defaultBlockState(), pos, originY + 2, originY + step);
                        return;
                    }
                    above = step <= 50 && open && pos.getY() < level.getMaxY();
                }
            }
        }

        private static void fillBetween(WorldGenLevel level, BlockState state, BlockPos.MutableBlockPos pos, int from,
                int toExclusive) {
            for (int y = from; y < toExclusive; y++) {
                level.setBlock(pos.setY(y), state, 2);
            }
        }

        private void placeSupport(WorldGenLevel level, BoundingBox chunkBB, int x0, int y0, int z, int y1, int x1,
                RandomSource random) {
            if (!isSupportingBox(level, chunkBB, x0, x1, y1, z)) {
                return;
            }
            BlockState planks = type.getPlanksState();
            BlockState fence = type.getFenceState();
            generateBox(level, chunkBB, x0, y0, z, x0, y1 - 1, z, fence.setValue(BlockStateProperties.WEST, true),
                CAVE_AIR, false);
            generateBox(level, chunkBB, x1, y0, z, x1, y1 - 1, z, fence.setValue(BlockStateProperties.EAST, true),
                CAVE_AIR, false);
            if (random.nextInt(4) == 0) {
                generateBox(level, chunkBB, x0, y1, z, x0, y1, z, planks, CAVE_AIR, false);
                generateBox(level, chunkBB, x1, y1, z, x1, y1, z, planks, CAVE_AIR, false);
                return;
            }
            generateBox(level, chunkBB, x0, y1, z, x1, y1, z, planks, CAVE_AIR, false);
            BlockState torch = Blocks.WALL_TORCH.defaultBlockState();
            maybeGenerateBlock(level, chunkBB, random, 0.05F, x0 + 1, y1, z - 1,
                torch.setValue(BlockStateProperties.HORIZONTAL_FACING, Direction.SOUTH));
            maybeGenerateBlock(level, chunkBB, random, 0.05F, x0 + 1, y1, z + 1,
                torch.setValue(BlockStateProperties.HORIZONTAL_FACING, Direction.NORTH));
        }

        private void webAt(WorldGenLevel level, BoundingBox chunkBB, RandomSource random, float chance, int x, int y,
                int z) {
            if (isInterior(level, x, y, z, chunkBB) && random.nextFloat() < chance
                && sturdyNeighbourCount(level, chunkBB, x, y, z) >= 2) {
                placeBlock(level, Blocks.COBWEB.defaultBlockState(), x, y, z, chunkBB);
            }
        }

        private int sturdyNeighbourCount(WorldGenLevel level, BoundingBox chunkBB, int x, int y, int z) {
            BlockPos.MutableBlockPos pos = getWorldPos(x, y, z);
            int count = 0;
            for (Direction dir : Direction.values()) {
                pos.move(dir);
                if (chunkBB.isInside(pos) && level.getBlockState(pos).isFaceSturdy(level, pos, dir.getOpposite())) {
                    if (++count >= 2) {
                        return count;
                    }
                }
                pos.move(dir.getOpposite());
            }
            return count;
        }
    }

    public static class MineShaftCrossing extends MineShaftPiece {
        private final Direction direction;
        private final boolean isTwoFloored;

        public MineShaftCrossing(CompoundTag tag) {
            super(StructurePieceType.MINE_SHAFT_CROSSING, tag);
            isTwoFloored = tag.getBooleanOr("tf", false);
            direction = Direction.from2DDataValue(tag.getIntOr("D", 0));
        }

        protected void addAdditionalSaveData(StructurePieceSerializationContext context, CompoundTag tag) {
            super.addAdditionalSaveData(context, tag);
            tag.putBoolean("tf", isTwoFloored);
            tag.putByte("D", (byte) direction.get2DDataValue());
        }

        public MineShaftCrossing(int genDepth, BoundingBox boundingBox, Direction direction,
                MineshaftStructure.Type type) {
            super(StructurePieceType.MINE_SHAFT_CROSSING, genDepth, type, boundingBox);
            this.direction = direction;
            this.isTwoFloored = boundingBox.getYSpan() > 3;
        }

        public static BoundingBox findCrossing(StructurePiecesBuilder builder, RandomSource random, int footX,
                int footY, int footZ, Direction direction) {
            int top = random.nextInt(4) == 0 ? 6 : 2;
            BoundingBox box = switch (direction) {
                case SOUTH -> new BoundingBox(-1, 0, 0, 3, top, 4);
                case WEST -> new BoundingBox(-4, 0, -1, 0, top, 3);
                case EAST -> new BoundingBox(0, 0, -1, 4, top, 3);
                default -> new BoundingBox(-1, 0, -4, 3, top, 0);
            };
            box.move(footX, footY, footZ);
            return builder.findCollisionPiece(box) != null ? null : box;
        }

        public void addChildren(StructurePiece startPiece, StructurePiecesBuilder builder, RandomSource random) {
            int depth = getGenDepth();
            BoundingBox bb = boundingBox;
            int[][] exits;
            switch (direction) {
                case SOUTH -> exits = new int[][] {{bb.minX() + 1, bb.maxZ() + 1, 2}, {bb.minX() - 1, bb.minZ() + 1, 1},
                    {bb.maxX() + 1, bb.minZ() + 1, 3}};
                case WEST -> exits = new int[][] {{bb.minX() + 1, bb.minZ() - 1, 0}, {bb.minX() + 1, bb.maxZ() + 1, 2},
                    {bb.minX() - 1, bb.minZ() + 1, 1}};
                case EAST -> exits = new int[][] {{bb.minX() + 1, bb.minZ() - 1, 0}, {bb.minX() + 1, bb.maxZ() + 1, 2},
                    {bb.maxX() + 1, bb.minZ() + 1, 3}};
                default -> exits = new int[][] {{bb.minX() + 1, bb.minZ() - 1, 0}, {bb.minX() - 1, bb.minZ() + 1, 1},
                    {bb.maxX() + 1, bb.minZ() + 1, 3}};
            }
            Direction[] dirs = {Direction.NORTH, Direction.WEST, Direction.SOUTH, Direction.EAST};
            for (int[] e : exits) {
                generateAndAddPiece(startPiece, builder, random, e[0], bb.minY(), e[1], dirs[e[2]], depth);
            }
            if (!isTwoFloored) {
                return;
            }
            int y = bb.minY() + 4;
            if (random.nextBoolean()) {
                generateAndAddPiece(startPiece, builder, random, bb.minX() + 1, y, bb.minZ() - 1, Direction.NORTH, depth);
            }
            if (random.nextBoolean()) {
                generateAndAddPiece(startPiece, builder, random, bb.minX() - 1, y, bb.minZ() + 1, Direction.WEST, depth);
            }
            if (random.nextBoolean()) {
                generateAndAddPiece(startPiece, builder, random, bb.maxX() + 1, y, bb.minZ() + 1, Direction.EAST, depth);
            }
            if (random.nextBoolean()) {
                generateAndAddPiece(startPiece, builder, random, bb.minX() + 1, y, bb.maxZ() + 1, Direction.SOUTH, depth);
            }
        }

        public void postProcess(WorldGenLevel level, StructureManager structureManager, ChunkGenerator generator,
                RandomSource random, BoundingBox chunkBB, ChunkPos chunkPos, BlockPos referencePos) {
            if (isInInvalidLocation(level, chunkBB)) {
                return;
            }
            BoundingBox bb = boundingBox;
            if (isTwoFloored) {
                int lowTop = bb.minY() + 2;
                int highBottom = bb.maxY() - 2;
                clear(level, chunkBB, bb.minX() + 1, bb.minY(), bb.minZ(), bb.maxX() - 1, lowTop, bb.maxZ());
                clear(level, chunkBB, bb.minX(), bb.minY(), bb.minZ() + 1, bb.maxX(), lowTop, bb.maxZ() - 1);
                clear(level, chunkBB, bb.minX() + 1, highBottom, bb.minZ(), bb.maxX() - 1, bb.maxY(), bb.maxZ());
                clear(level, chunkBB, bb.minX(), highBottom, bb.minZ() + 1, bb.maxX(), bb.maxY(), bb.maxZ() - 1);
                clear(level, chunkBB, bb.minX() + 1, bb.minY() + 3, bb.minZ() + 1, bb.maxX() - 1, bb.minY() + 3,
                    bb.maxZ() - 1);
            } else {
                clear(level, chunkBB, bb.minX() + 1, bb.minY(), bb.minZ(), bb.maxX() - 1, bb.maxY(), bb.maxZ());
                clear(level, chunkBB, bb.minX(), bb.minY(), bb.minZ() + 1, bb.maxX(), bb.maxY(), bb.maxZ() - 1);
            }
            pillar(level, chunkBB, bb.minX() + 1, bb.minY(), bb.minZ() + 1, bb.maxY());
            pillar(level, chunkBB, bb.minX() + 1, bb.minY(), bb.maxZ() - 1, bb.maxY());
            pillar(level, chunkBB, bb.maxX() - 1, bb.minY(), bb.minZ() + 1, bb.maxY());
            pillar(level, chunkBB, bb.maxX() - 1, bb.minY(), bb.maxZ() - 1, bb.maxY());
            BlockState planks = type.getPlanksState();
            for (int x = bb.minX(); x <= bb.maxX(); x++) {
                for (int z = bb.minZ(); z <= bb.maxZ(); z++) {
                    setPlanksBlock(level, chunkBB, planks, x, bb.minY() - 1, z);
                }
            }
        }

        private void clear(WorldGenLevel level, BoundingBox chunkBB, int x0, int y0, int z0, int x1, int y1, int z1) {
            generateBox(level, chunkBB, x0, y0, z0, x1, y1, z1, CAVE_AIR, CAVE_AIR, false);
        }

        private void pillar(WorldGenLevel level, BoundingBox chunkBB, int x, int y0, int z, int y1) {
            if (!getBlock(level, x, y1 + 1, z, chunkBB).isAir()) {
                generateBox(level, chunkBB, x, y0, z, x, y1, z, type.getPlanksState(), CAVE_AIR, false);
            }
        }
    }

    public static class MineShaftStairs extends MineShaftPiece {
        public MineShaftStairs(int genDepth, BoundingBox boundingBox, Direction direction,
                MineshaftStructure.Type type) {
            super(StructurePieceType.MINE_SHAFT_STAIRS, genDepth, type, boundingBox);
            setOrientation(direction);
        }

        public MineShaftStairs(CompoundTag tag) {
            super(StructurePieceType.MINE_SHAFT_STAIRS, tag);
        }

        public static BoundingBox findStairs(StructurePiecesBuilder builder, RandomSource random, int footX, int footY,
                int footZ, Direction direction) {
            BoundingBox box = switch (direction) {
                case SOUTH -> new BoundingBox(0, -5, 0, 2, 2, 8);
                case WEST -> new BoundingBox(-8, -5, 0, 0, 2, 2);
                case EAST -> new BoundingBox(0, -5, 0, 8, 2, 2);
                default -> new BoundingBox(0, -5, -8, 2, 2, 0);
            };
            box.move(footX, footY, footZ);
            return builder.findCollisionPiece(box) != null ? null : box;
        }

        public void addChildren(StructurePiece startPiece, StructurePiecesBuilder builder, RandomSource random) {
            Direction facing = getOrientation();
            if (facing == null) {
                return;
            }
            int depth = getGenDepth();
            BoundingBox bb = boundingBox;
            switch (facing) {
                case SOUTH -> generateAndAddPiece(startPiece, builder, random, bb.minX(), bb.minY(), bb.maxZ() + 1,
                    Direction.SOUTH, depth);
                case WEST -> generateAndAddPiece(startPiece, builder, random, bb.minX() - 1, bb.minY(), bb.minZ(),
                    Direction.WEST, depth);
                case EAST -> generateAndAddPiece(startPiece, builder, random, bb.maxX() + 1, bb.minY(), bb.minZ(),
                    Direction.EAST, depth);
                default -> generateAndAddPiece(startPiece, builder, random, bb.minX(), bb.minY(), bb.minZ() - 1,
                    Direction.NORTH, depth);
            }
        }

        public void postProcess(WorldGenLevel level, StructureManager structureManager, ChunkGenerator generator,
                RandomSource random, BoundingBox chunkBB, ChunkPos chunkPos, BlockPos referencePos) {
            if (isInInvalidLocation(level, chunkBB)) {
                return;
            }
            generateBox(level, chunkBB, 0, 5, 0, 2, 7, 1, CAVE_AIR, CAVE_AIR, false);
            generateBox(level, chunkBB, 0, 0, 7, 2, 2, 8, CAVE_AIR, CAVE_AIR, false);
            for (int i = 0; i < 5; i++) {
                generateBox(level, chunkBB, 0, 5 - i - (i < 4 ? 1 : 0), 2 + i, 2, 7 - i, 2 + i, CAVE_AIR, CAVE_AIR,
                    false);
            }
        }
    }
}
