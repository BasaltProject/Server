// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.structure.structures;

import java.util.ArrayList;
import java.util.List;
import net.minecraft.core.BlockPos;
import net.minecraft.core.Direction;
import net.minecraft.nbt.CompoundTag;
import net.minecraft.resources.Identifier;
import net.minecraft.util.RandomSource;
import net.minecraft.util.Util;
import net.minecraft.world.entity.EntitySpawnReason;
import net.minecraft.world.entity.EntityTypes;
import net.minecraft.world.entity.Mob;
import net.minecraft.world.level.ServerLevelAccessor;
import net.minecraft.world.level.block.Blocks;
import net.minecraft.world.level.block.Mirror;
import net.minecraft.world.level.block.Rotation;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.block.state.properties.BlockStateProperties;
import net.minecraft.world.level.levelgen.structure.BoundingBox;
import net.minecraft.world.level.levelgen.structure.TemplateStructurePiece;
import net.minecraft.world.level.levelgen.structure.pieces.StructurePieceSerializationContext;
import net.minecraft.world.level.levelgen.structure.pieces.StructurePieceType;
import net.minecraft.world.level.levelgen.structure.templatesystem.BlockIgnoreProcessor;
import net.minecraft.world.level.levelgen.structure.templatesystem.StructurePlaceSettings;
import net.minecraft.world.level.levelgen.structure.templatesystem.StructureTemplate;
import net.minecraft.world.level.levelgen.structure.templatesystem.StructureTemplateManager;
import net.minecraft.world.level.storage.loot.BuiltInLootTables;

public class WoodlandMansionPieces {
    private static final int kCorridor = 1;
    private static final int kRoom = 2;
    private static final int kStartRoom = 3;
    private static final int kBlocked = 5;
    private static final int kRoom1x1 = 65536;
    private static final int kRoom1x2 = 131072;
    private static final int kRoom2x2 = 262144;
    private static final int kOriginFlag = 1048576;
    private static final int kDoorFlag = 2097152;
    private static final int kStairsFlag = 4194304;
    private static final int kCorridorFlag = 8388608;
    private static final int kTypeMask = 983040;
    private static final int kIdMask = 65535;

    public static void generateMansion(StructureTemplateManager structureTemplateManager, BlockPos origin,
            Rotation rotation, List<WoodlandMansionPiece> pieces, RandomSource random) {
        MansionGrid grid = new MansionGrid(random);
        new MansionPiecePlacer(structureTemplateManager, random).createMansion(origin, rotation, pieces, grid);
    }

    private static BlockPos shift(BlockPos from, Rotation rot, int east, int south) {
        return from.relative(rot.rotate(Direction.EAST), east).relative(rot.rotate(Direction.SOUTH), south);
    }

    public static class WoodlandMansionPiece extends TemplateStructurePiece {
        public WoodlandMansionPiece(StructureTemplateManager structureTemplateManager, String templateName,
                BlockPos position, Rotation rotation) {
            this(structureTemplateManager, templateName, position, rotation, Mirror.NONE);
        }

        public WoodlandMansionPiece(StructureTemplateManager structureTemplateManager, String templateName,
                BlockPos position, Rotation rotation, Mirror mirror) {
            super(StructurePieceType.WOODLAND_MANSION_PIECE, 0, structureTemplateManager, makeLocation(templateName),
                templateName, makeSettings(mirror, rotation), position);
        }

        public WoodlandMansionPiece(StructureTemplateManager structureTemplateManager, CompoundTag tag) {
            super(StructurePieceType.WOODLAND_MANSION_PIECE, tag, structureTemplateManager,
                location -> makeSettings(Mirror.valueOf(tag.getStringOr("Mi", "NONE")),
                    Rotation.valueOf(tag.getStringOr("Rot", "NONE"))));
        }

        protected Identifier makeTemplateLocation() {
            return makeLocation(templateName);
        }

        private static Identifier makeLocation(String templateName) {
            return Identifier.withDefaultNamespace("woodland_mansion/" + templateName);
        }

        private static StructurePlaceSettings makeSettings(Mirror mirror, Rotation rotation) {
            return new StructurePlaceSettings().setIgnoreEntities(true).setRotation(rotation).setMirror(mirror)
                .addProcessor(BlockIgnoreProcessor.STRUCTURE_BLOCK);
        }

        protected void addAdditionalSaveData(StructurePieceSerializationContext context, CompoundTag tag) {
            super.addAdditionalSaveData(context, tag);
            tag.putString("Rot", placeSettings.getRotation().name());
            tag.putString("Mi", placeSettings.getMirror().name());
        }

        protected void handleDataMarker(String markerId, BlockPos position, ServerLevelAccessor level,
                RandomSource random, BoundingBox chunkBB) {
            if (markerId.startsWith("Chest")) {
                Direction facing = switch (markerId) {
                    case "ChestWest" -> placeSettings.getRotation().rotate(Direction.WEST);
                    case "ChestEast" -> placeSettings.getRotation().rotate(Direction.EAST);
                    case "ChestSouth" -> placeSettings.getRotation().rotate(Direction.SOUTH);
                    case "ChestNorth" -> placeSettings.getRotation().rotate(Direction.NORTH);
                    default -> null;
                };
                BlockState chest = Blocks.CHEST.defaultBlockState();
                if (facing != null) {
                    chest = chest.setValue(BlockStateProperties.HORIZONTAL_FACING, facing);
                }
                createChest(level, chunkBB, random, position, BuiltInLootTables.WOODLAND_MANSION, chest);
                return;
            }
            List<Mob> mobs = new ArrayList<>();
            switch (markerId) {
                case "Mage" -> mobs.add((Mob) EntityTypes.EVOKER.create(level.getLevel(), EntitySpawnReason.STRUCTURE));
                case "Warrior" -> mobs.add(
                    (Mob) EntityTypes.VINDICATOR.create(level.getLevel(), EntitySpawnReason.STRUCTURE));
                case "Group of Allays" -> {
                    int count = level.getRandom().nextInt(3) + 1;
                    for (int i = 0; i < count; i++) {
                        mobs.add((Mob) EntityTypes.ALLAY.create(level.getLevel(), EntitySpawnReason.STRUCTURE));
                    }
                }
                default -> {
                    return;
                }
            }
            for (Mob mob : mobs) {
                if (mob == null) {
                    continue;
                }
                mob.setPersistenceRequired();
                mob.snapTo(position, 0.0F, 0.0F);
                mob.finalizeSpawn(level, level.getCurrentDifficultyAt(mob.blockPosition()), EntitySpawnReason.STRUCTURE,
                    null);
                level.addFreshEntityWithPassengers(mob);
                level.setBlock(position, Blocks.AIR.defaultBlockState(), 2);
            }
        }
    }

    static class PlacementData {
        public Rotation rotation;
        public BlockPos position;
        public String wallType;
    }

    static class MansionPiecePlacer {
        private final StructureTemplateManager manager;
        private final RandomSource random;
        private int startX;
        private int startY;

        public MansionPiecePlacer(StructureTemplateManager structureTemplateManager, RandomSource random) {
            this.manager = structureTemplateManager;
            this.random = random;
        }

        private void add(List<WoodlandMansionPiece> pieces, String name, BlockPos pos, Rotation rot) {
            pieces.add(new WoodlandMansionPiece(manager, name, pos, rot));
        }

        private void add(List<WoodlandMansionPiece> pieces, String name, BlockPos pos, Rotation rot, Mirror mirror) {
            pieces.add(new WoodlandMansionPiece(manager, name, pos, rot, mirror));
        }

        public void createMansion(BlockPos origin, Rotation rotation, List<WoodlandMansionPiece> pieces,
                MansionGrid mansion) {
            PlacementData lower = new PlacementData();
            lower.position = origin;
            lower.rotation = rotation;
            lower.wallType = "wall_flat";
            entrance(pieces, lower);
            PlacementData upper = new PlacementData();
            upper.position = lower.position.above(8);
            upper.rotation = lower.rotation;
            upper.wallType = "wall_window";
            SimpleGrid baseGrid = mansion.baseGrid;
            SimpleGrid thirdGrid = mansion.thirdFloorGrid;
            startX = mansion.entranceX + 1;
            startY = mansion.entranceY + 1;
            int endX = mansion.entranceX + 1;
            int endY = mansion.entranceY;
            traverseOuterWalls(pieces, lower, baseGrid, Direction.SOUTH, startX, startY, endX, endY);
            traverseOuterWalls(pieces, upper, baseGrid, Direction.SOUTH, startX, startY, endX, endY);
            PlacementData top = new PlacementData();
            top.position = lower.position.above(19);
            top.rotation = lower.rotation;
            top.wallType = "wall_window";
            boolean done = false;
            for (int y = 0; y < thirdGrid.height && !done; y++) {
                for (int x = thirdGrid.width - 1; x >= 0 && !done; x--) {
                    if (MansionGrid.isHouse(thirdGrid, x, y)) {
                        top.position = shift(top.position, rotation, (x - startX) * 8, 8 + (y - startY) * 8);
                        traverseWallPiece(pieces, top);
                        traverseOuterWalls(pieces, top, thirdGrid, Direction.SOUTH, x, y, x, y);
                        done = true;
                    }
                }
            }
            createRoof(pieces, origin.above(16), rotation, baseGrid, thirdGrid);
            createRoof(pieces, origin.above(27), rotation, thirdGrid, null);
            FloorRoomCollection[] collections = {new FirstFloorRoomCollection(), new SecondFloorRoomCollection(),
                new ThirdFloorRoomCollection()};
            for (int floorNum = 0; floorNum < 3; floorNum++) {
                BlockPos floorOrigin = origin.above(8 * floorNum + (floorNum == 2 ? 3 : 0));
                SimpleGrid rooms = mansion.floorRooms[floorNum];
                SimpleGrid grid = floorNum == 2 ? thirdGrid : baseGrid;
                placeCarpets(pieces, floorOrigin, rotation, grid, rooms, floorNum);
                placeRooms(pieces, mansion, floorOrigin, rotation, grid, rooms, floorNum, collections[floorNum]);
            }
        }

        private static boolean carpeted(SimpleGrid grid, SimpleGrid rooms, int x, int y) {
            return grid.get(x, y) == 1 || (rooms.get(x, y) & kCorridorFlag) == kCorridorFlag;
        }

        private void placeCarpets(List<WoodlandMansionPiece> pieces, BlockPos floorOrigin, Rotation rotation,
                SimpleGrid grid, SimpleGrid rooms, int floorNum) {
            String south = floorNum == 0 ? "carpet_south_1" : "carpet_south_2";
            String west = floorNum == 0 ? "carpet_west_1" : "carpet_west_2";
            for (int y = 0; y < grid.height; y++) {
                for (int x = 0; x < grid.width; x++) {
                    if (grid.get(x, y) != 1) {
                        continue;
                    }
                    BlockPos pos = shift(floorOrigin, rotation, (x - startX) * 8, 8 + (y - startY) * 8);
                    add(pieces, "corridor_floor", pos, rotation);
                    if (carpeted(grid, rooms, x, y - 1)) {
                        add(pieces, "carpet_north", shift(pos, rotation, 1, 0).above(), rotation);
                    }
                    if (carpeted(grid, rooms, x + 1, y)) {
                        add(pieces, "carpet_east", shift(pos, rotation, 5, 1).above(), rotation);
                    }
                    if (carpeted(grid, rooms, x, y + 1)) {
                        add(pieces, south, shift(pos, rotation, -1, 5), rotation);
                    }
                    if (carpeted(grid, rooms, x - 1, y)) {
                        add(pieces, west, shift(pos, rotation, -1, -1), rotation);
                    }
                }
            }
        }

        private void placeRooms(List<WoodlandMansionPiece> pieces, MansionGrid mansion, BlockPos floorOrigin,
                Rotation rotation, SimpleGrid grid, SimpleGrid rooms, int floorNum, FloorRoomCollection collection) {
            String wall = floorNum == 0 ? "indoors_wall_1" : "indoors_wall_2";
            String door = floorNum == 0 ? "indoors_door_1" : "indoors_door_2";
            List<Direction> doorDirs = new ArrayList<>();
            for (int y = 0; y < grid.height; y++) {
                for (int x = 0; x < grid.width; x++) {
                    boolean thirdStart = floorNum == 2 && grid.get(x, y) == 3;
                    if (grid.get(x, y) != 2 && !thirdStart) {
                        continue;
                    }
                    int roomData = rooms.get(x, y);
                    int roomType = roomData & kTypeMask;
                    int roomId = roomData & kIdMask;
                    thirdStart = thirdStart && (roomData & kCorridorFlag) == kCorridorFlag;
                    doorDirs.clear();
                    if ((roomData & kDoorFlag) == kDoorFlag) {
                        for (Direction dir : Direction.Plane.HORIZONTAL) {
                            if (grid.get(x + dir.getStepX(), y + dir.getStepZ()) == 1) {
                                doorDirs.add(dir);
                            }
                        }
                    }
                    Direction doorDir = null;
                    if (!doorDirs.isEmpty()) {
                        doorDir = doorDirs.get(random.nextInt(doorDirs.size()));
                    } else if ((roomData & kOriginFlag) == kOriginFlag) {
                        doorDir = Direction.UP;
                    }
                    BlockPos roomPos = shift(floorOrigin, rotation, -1 + (x - startX) * 8, 8 + (y - startY) * 8);
                    if (MansionGrid.isHouse(grid, x - 1, y) && !mansion.isRoomId(grid, x - 1, y, floorNum, roomId)) {
                        add(pieces, doorDir == Direction.WEST ? door : wall, roomPos, rotation);
                    }
                    if (grid.get(x + 1, y) == 1 && !thirdStart) {
                        add(pieces, doorDir == Direction.EAST ? door : wall, shift(roomPos, rotation, 8, 0), rotation);
                    }
                    if (MansionGrid.isHouse(grid, x, y + 1) && !mansion.isRoomId(grid, x, y + 1, floorNum, roomId)) {
                        add(pieces, doorDir == Direction.SOUTH ? door : wall, shift(roomPos, rotation, 7, 7),
                            rotation.getRotated(Rotation.CLOCKWISE_90));
                    }
                    if (grid.get(x, y - 1) == 1 && !thirdStart) {
                        add(pieces, doorDir == Direction.NORTH ? door : wall, shift(roomPos, rotation, 7, -1),
                            rotation.getRotated(Rotation.CLOCKWISE_90));
                    }
                    if (roomType == kRoom1x1) {
                        addRoom1x1(pieces, roomPos, rotation, doorDir, collection);
                    } else if (roomType == kRoom1x2 && doorDir != null) {
                        Direction roomDir = mansion.get1x2RoomDirection(grid, x, y, floorNum, roomId);
                        boolean stairs = (roomData & kStairsFlag) == kStairsFlag;
                        addRoom1x2(pieces, roomPos, rotation, roomDir, doorDir, collection, stairs);
                    } else if (roomType == kRoom2x2 && doorDir != null && doorDir != Direction.UP) {
                        Direction roomDir = doorDir.getClockWise();
                        if (!mansion.isRoomId(grid, x + roomDir.getStepX(), y + roomDir.getStepZ(), floorNum, roomId)) {
                            roomDir = roomDir.getOpposite();
                        }
                        addRoom2x2(pieces, roomPos, rotation, roomDir, doorDir, collection);
                    } else if (roomType == kRoom2x2 && doorDir == Direction.UP) {
                        addRoom2x2Secret(pieces, roomPos, rotation, collection);
                    }
                }
            }
        }

        private void traverseOuterWalls(List<WoodlandMansionPiece> pieces, PlacementData data, SimpleGrid grid,
                Direction heading, int fromX, int fromY, int endX, int endY) {
            int gx = fromX;
            int gy = fromY;
            Direction startHeading = heading;
            do {
                int ahead = MansionGrid.isHouse(grid, gx + heading.getStepX(), gy + heading.getStepZ()) ? 1 : 0;
                Direction left = heading.getCounterClockWise();
                if (ahead == 0) {
                    traverseTurn(pieces, data);
                    heading = heading.getClockWise();
                    if (gx != endX || gy != endY || startHeading != heading) {
                        traverseWallPiece(pieces, data);
                    }
                } else if (MansionGrid.isHouse(grid, gx + heading.getStepX() + left.getStepX(),
                    gy + heading.getStepZ() + left.getStepZ())) {
                    traverseInnerTurn(pieces, data);
                    gx += heading.getStepX();
                    gy += heading.getStepZ();
                    heading = left;
                } else {
                    gx += heading.getStepX();
                    gy += heading.getStepZ();
                    if (gx != endX || gy != endY || startHeading != heading) {
                        traverseWallPiece(pieces, data);
                    }
                }
            } while (gx != endX || gy != endY || startHeading != heading);
        }

        private void createRoof(List<WoodlandMansionPiece> pieces, BlockPos roofOrigin, Rotation rotation,
                SimpleGrid grid, SimpleGrid aboveGrid) {
            for (int y = 0; y < grid.height; y++) {
                for (int x = 0; x < grid.width; x++) {
                    BlockPos position = shift(roofOrigin, rotation, (x - startX) * 8, 8 + (y - startY) * 8);
                    boolean covered = aboveGrid != null && MansionGrid.isHouse(aboveGrid, x, y);
                    if (!MansionGrid.isHouse(grid, x, y) || covered) {
                        continue;
                    }
                    add(pieces, "roof", position.above(3), rotation);
                    if (!MansionGrid.isHouse(grid, x + 1, y)) {
                        add(pieces, "roof_front", shift(position, rotation, 6, 0), rotation);
                    }
                    if (!MansionGrid.isHouse(grid, x - 1, y)) {
                        add(pieces, "roof_front", shift(position, rotation, 0, 7),
                            rotation.getRotated(Rotation.CLOCKWISE_180));
                    }
                    if (!MansionGrid.isHouse(grid, x, y - 1)) {
                        add(pieces, "roof_front", shift(position, rotation, -1, 0),
                            rotation.getRotated(Rotation.COUNTERCLOCKWISE_90));
                    }
                    if (!MansionGrid.isHouse(grid, x, y + 1)) {
                        add(pieces, "roof_front", shift(position, rotation, 6, 6),
                            rotation.getRotated(Rotation.CLOCKWISE_90));
                    }
                }
            }
            if (aboveGrid != null) {
                for (int y = 0; y < grid.height; y++) {
                    for (int x = 0; x < grid.width; x++) {
                        BlockPos position = shift(roofOrigin, rotation, (x - startX) * 8, 8 + (y - startY) * 8);
                        if (!MansionGrid.isHouse(grid, x, y) || !MansionGrid.isHouse(aboveGrid, x, y)) {
                            continue;
                        }
                        boolean openEast = !MansionGrid.isHouse(grid, x + 1, y);
                        boolean openWest = !MansionGrid.isHouse(grid, x - 1, y);
                        boolean openNorth = !MansionGrid.isHouse(grid, x, y - 1);
                        boolean openSouth = !MansionGrid.isHouse(grid, x, y + 1);
                        if (openEast) {
                            add(pieces, "small_wall", shift(position, rotation, 7, 0), rotation);
                        }
                        if (openWest) {
                            add(pieces, "small_wall", shift(position, rotation, -1, 6),
                                rotation.getRotated(Rotation.CLOCKWISE_180));
                        }
                        if (openNorth) {
                            add(pieces, "small_wall", shift(position, rotation, 0, -1),
                                rotation.getRotated(Rotation.COUNTERCLOCKWISE_90));
                        }
                        if (openSouth) {
                            add(pieces, "small_wall", shift(position, rotation, 6, 7),
                                rotation.getRotated(Rotation.CLOCKWISE_90));
                        }
                        if (openEast && openNorth) {
                            add(pieces, "small_wall_corner", shift(position, rotation, 7, -2), rotation);
                        }
                        if (openEast && openSouth) {
                            add(pieces, "small_wall_corner", shift(position, rotation, 8, 7),
                                rotation.getRotated(Rotation.CLOCKWISE_90));
                        }
                        if (openWest && openNorth) {
                            add(pieces, "small_wall_corner", shift(position, rotation, -2, -1),
                                rotation.getRotated(Rotation.COUNTERCLOCKWISE_90));
                        }
                        if (openWest && openSouth) {
                            add(pieces, "small_wall_corner", shift(position, rotation, -1, 8),
                                rotation.getRotated(Rotation.CLOCKWISE_180));
                        }
                    }
                }
            }
            for (int y = 0; y < grid.height; y++) {
                for (int x = 0; x < grid.width; x++) {
                    BlockPos position = shift(roofOrigin, rotation, (x - startX) * 8, 8 + (y - startY) * 8);
                    boolean covered = aboveGrid != null && MansionGrid.isHouse(aboveGrid, x, y);
                    if (!MansionGrid.isHouse(grid, x, y) || covered) {
                        continue;
                    }
                    if (!MansionGrid.isHouse(grid, x + 1, y)) {
                        BlockPos edge = shift(position, rotation, 6, 0);
                        if (!MansionGrid.isHouse(grid, x, y + 1)) {
                            add(pieces, "roof_corner", shift(edge, rotation, 0, 6), rotation);
                        } else if (MansionGrid.isHouse(grid, x + 1, y + 1)) {
                            add(pieces, "roof_inner_corner", shift(edge, rotation, 0, 5), rotation);
                        }
                        if (!MansionGrid.isHouse(grid, x, y - 1)) {
                            add(pieces, "roof_corner", edge, rotation.getRotated(Rotation.COUNTERCLOCKWISE_90));
                        } else if (MansionGrid.isHouse(grid, x + 1, y - 1)) {
                            add(pieces, "roof_inner_corner", shift(position, rotation, 9, -2),
                                rotation.getRotated(Rotation.CLOCKWISE_90));
                        }
                    }
                    if (!MansionGrid.isHouse(grid, x - 1, y)) {
                        if (!MansionGrid.isHouse(grid, x, y + 1)) {
                            add(pieces, "roof_corner", shift(position, rotation, 0, 6),
                                rotation.getRotated(Rotation.CLOCKWISE_90));
                        } else if (MansionGrid.isHouse(grid, x - 1, y + 1)) {
                            add(pieces, "roof_inner_corner", shift(position, rotation, -3, 8),
                                rotation.getRotated(Rotation.COUNTERCLOCKWISE_90));
                        }
                        if (!MansionGrid.isHouse(grid, x, y - 1)) {
                            add(pieces, "roof_corner", position, rotation.getRotated(Rotation.CLOCKWISE_180));
                        } else if (MansionGrid.isHouse(grid, x - 1, y - 1)) {
                            add(pieces, "roof_inner_corner", shift(position, rotation, 0, 1),
                                rotation.getRotated(Rotation.CLOCKWISE_180));
                        }
                    }
                }
            }
        }

        private void entrance(List<WoodlandMansionPiece> pieces, PlacementData data) {
            add(pieces, "entrance", shift(data.position, data.rotation, -9, 0), data.rotation);
            data.position = shift(data.position, data.rotation, 0, 16);
        }

        private void traverseWallPiece(List<WoodlandMansionPiece> pieces, PlacementData data) {
            add(pieces, data.wallType, shift(data.position, data.rotation, 7, 0), data.rotation);
            data.position = shift(data.position, data.rotation, 0, 8);
        }

        private void traverseTurn(List<WoodlandMansionPiece> pieces, PlacementData data) {
            data.position = shift(data.position, data.rotation, 0, -1);
            add(pieces, "wall_corner", data.position, data.rotation);
            data.position = shift(data.position, data.rotation, 6, -7);
            data.rotation = data.rotation.getRotated(Rotation.CLOCKWISE_90);
        }

        private void traverseInnerTurn(List<WoodlandMansionPiece> pieces, PlacementData data) {
            data.position = shift(data.position, data.rotation, 8, 6);
            data.rotation = data.rotation.getRotated(Rotation.COUNTERCLOCKWISE_90);
        }

        private void addRoom1x1(List<WoodlandMansionPiece> pieces, BlockPos roomPos, Rotation rotation,
                Direction doorDir, FloorRoomCollection rooms) {
            Rotation pieceRot = Rotation.NONE;
            String roomType = rooms.get1x1(random);
            if (doorDir != Direction.EAST) {
                if (doorDir == Direction.NORTH) {
                    pieceRot = pieceRot.getRotated(Rotation.COUNTERCLOCKWISE_90);
                } else if (doorDir == Direction.WEST) {
                    pieceRot = pieceRot.getRotated(Rotation.CLOCKWISE_180);
                } else if (doorDir == Direction.SOUTH) {
                    pieceRot = pieceRot.getRotated(Rotation.CLOCKWISE_90);
                } else {
                    roomType = rooms.get1x1Secret(random);
                }
            }
            BlockPos local = StructureTemplate.getZeroPositionWithTransform(new BlockPos(1, 0, 0), Mirror.NONE,
                pieceRot, 7, 7);
            pieceRot = pieceRot.getRotated(rotation);
            local = local.rotate(rotation);
            add(pieces, roomType, roomPos.offset(local.getX(), 0, local.getZ()), pieceRot);
        }

        private void addRoom1x2(List<WoodlandMansionPiece> pieces, BlockPos roomPos, Rotation rotation,
                Direction roomDir, Direction doorDir, FloorRoomCollection rooms, boolean isStairsRoom) {
            Rotation cw = Rotation.CLOCKWISE_90;
            Rotation ccw = Rotation.COUNTERCLOCKWISE_90;
            Rotation half = Rotation.CLOCKWISE_180;
            if (doorDir == Direction.EAST && roomDir == Direction.SOUTH) {
                add(pieces, rooms.get1x2SideEntrance(random, isStairsRoom), shift(roomPos, rotation, 1, 0), rotation);
            } else if (doorDir == Direction.EAST && roomDir == Direction.NORTH) {
                add(pieces, rooms.get1x2SideEntrance(random, isStairsRoom), shift(roomPos, rotation, 1, 6), rotation,
                    Mirror.LEFT_RIGHT);
            } else if (doorDir == Direction.WEST && roomDir == Direction.NORTH) {
                add(pieces, rooms.get1x2SideEntrance(random, isStairsRoom), shift(roomPos, rotation, 7, 6),
                    rotation.getRotated(half));
            } else if (doorDir == Direction.WEST && roomDir == Direction.SOUTH) {
                add(pieces, rooms.get1x2SideEntrance(random, isStairsRoom), shift(roomPos, rotation, 7, 0), rotation,
                    Mirror.FRONT_BACK);
            } else if (doorDir == Direction.SOUTH && roomDir == Direction.EAST) {
                add(pieces, rooms.get1x2SideEntrance(random, isStairsRoom), shift(roomPos, rotation, 1, 0),
                    rotation.getRotated(cw), Mirror.LEFT_RIGHT);
            } else if (doorDir == Direction.SOUTH && roomDir == Direction.WEST) {
                add(pieces, rooms.get1x2SideEntrance(random, isStairsRoom), shift(roomPos, rotation, 7, 0),
                    rotation.getRotated(cw));
            } else if (doorDir == Direction.NORTH && roomDir == Direction.WEST) {
                add(pieces, rooms.get1x2SideEntrance(random, isStairsRoom), shift(roomPos, rotation, 7, 6),
                    rotation.getRotated(cw), Mirror.FRONT_BACK);
            } else if (doorDir == Direction.NORTH && roomDir == Direction.EAST) {
                add(pieces, rooms.get1x2SideEntrance(random, isStairsRoom), shift(roomPos, rotation, 1, 6),
                    rotation.getRotated(ccw));
            } else if (doorDir == Direction.SOUTH && roomDir == Direction.NORTH) {
                add(pieces, rooms.get1x2FrontEntrance(random, isStairsRoom), shift(roomPos, rotation, 1, -8),
                    rotation);
            } else if (doorDir == Direction.NORTH && roomDir == Direction.SOUTH) {
                add(pieces, rooms.get1x2FrontEntrance(random, isStairsRoom), shift(roomPos, rotation, 7, 14),
                    rotation.getRotated(half));
            } else if (doorDir == Direction.WEST && roomDir == Direction.EAST) {
                add(pieces, rooms.get1x2FrontEntrance(random, isStairsRoom), shift(roomPos, rotation, 15, 0),
                    rotation.getRotated(cw));
            } else if (doorDir == Direction.EAST && roomDir == Direction.WEST) {
                add(pieces, rooms.get1x2FrontEntrance(random, isStairsRoom), shift(roomPos, rotation, -7, 6),
                    rotation.getRotated(ccw));
            } else if (doorDir == Direction.UP && roomDir == Direction.EAST) {
                add(pieces, rooms.get1x2Secret(random), shift(roomPos, rotation, 15, 0), rotation.getRotated(cw));
            } else if (doorDir == Direction.UP && roomDir == Direction.SOUTH) {
                add(pieces, rooms.get1x2Secret(random), shift(roomPos, rotation, 1, 0), rotation);
            }
        }

        private void addRoom2x2(List<WoodlandMansionPiece> pieces, BlockPos roomPos, Rotation rotation,
                Direction roomDir, Direction doorDir, FloorRoomCollection rooms) {
            int east = 0;
            int south = 0;
            Rotation rot = rotation;
            Mirror mirror = Mirror.NONE;
            if (doorDir == Direction.EAST && roomDir == Direction.SOUTH) {
                east = -7;
            } else if (doorDir == Direction.EAST && roomDir == Direction.NORTH) {
                east = -7;
                south = 6;
                mirror = Mirror.LEFT_RIGHT;
            } else if (doorDir == Direction.NORTH && roomDir == Direction.EAST) {
                east = 1;
                south = 14;
                rot = rotation.getRotated(Rotation.COUNTERCLOCKWISE_90);
            } else if (doorDir == Direction.NORTH && roomDir == Direction.WEST) {
                east = 7;
                south = 14;
                rot = rotation.getRotated(Rotation.COUNTERCLOCKWISE_90);
                mirror = Mirror.LEFT_RIGHT;
            } else if (doorDir == Direction.SOUTH && roomDir == Direction.WEST) {
                east = 7;
                south = -8;
                rot = rotation.getRotated(Rotation.CLOCKWISE_90);
            } else if (doorDir == Direction.SOUTH && roomDir == Direction.EAST) {
                east = 1;
                south = -8;
                rot = rotation.getRotated(Rotation.CLOCKWISE_90);
                mirror = Mirror.LEFT_RIGHT;
            } else if (doorDir == Direction.WEST && roomDir == Direction.NORTH) {
                east = 15;
                south = 6;
                rot = rotation.getRotated(Rotation.CLOCKWISE_180);
            } else if (doorDir == Direction.WEST && roomDir == Direction.SOUTH) {
                east = 15;
                mirror = Mirror.FRONT_BACK;
            }
            add(pieces, rooms.get2x2(random), shift(roomPos, rotation, east, south), rot, mirror);
        }

        private void addRoom2x2Secret(List<WoodlandMansionPiece> pieces, BlockPos roomPos, Rotation rotation,
                FloorRoomCollection rooms) {
            add(pieces, rooms.get2x2Secret(random), shift(roomPos, rotation, 1, 0), rotation, Mirror.NONE);
        }
    }

    static class MansionGrid {
        private final RandomSource random;
        private final SimpleGrid baseGrid;
        private final SimpleGrid thirdFloorGrid;
        private final SimpleGrid[] floorRooms;
        private final int entranceX;
        private final int entranceY;

        public MansionGrid(RandomSource random) {
            this.random = random;
            entranceX = 7;
            entranceY = 4;
            baseGrid = new SimpleGrid(11, 11, kBlocked);
            baseGrid.set(entranceX, entranceY, entranceX + 1, entranceY + 1, kStartRoom);
            baseGrid.set(entranceX - 1, entranceY, entranceX - 1, entranceY + 1, kRoom);
            baseGrid.set(entranceX + 2, entranceY - 2, entranceX + 3, entranceY + 3, kBlocked);
            baseGrid.set(entranceX + 1, entranceY - 2, entranceX + 1, entranceY - 1, kCorridor);
            baseGrid.set(entranceX + 1, entranceY + 2, entranceX + 1, entranceY + 3, kCorridor);
            baseGrid.set(entranceX - 1, entranceY - 1, kCorridor);
            baseGrid.set(entranceX - 1, entranceY + 2, kCorridor);
            baseGrid.set(0, 0, 11, 1, kBlocked);
            baseGrid.set(0, 9, 11, 11, kBlocked);
            recursiveCorridor(baseGrid, entranceX, entranceY - 2, Direction.WEST, 6);
            recursiveCorridor(baseGrid, entranceX, entranceY + 3, Direction.WEST, 6);
            recursiveCorridor(baseGrid, entranceX - 2, entranceY - 1, Direction.WEST, 3);
            recursiveCorridor(baseGrid, entranceX - 2, entranceY + 2, Direction.WEST, 3);
            while (cleanEdges(baseGrid)) {
                continue;
            }
            floorRooms = new SimpleGrid[] {new SimpleGrid(11, 11, kBlocked), new SimpleGrid(11, 11, kBlocked),
                new SimpleGrid(11, 11, kBlocked)};
            identifyRooms(baseGrid, floorRooms[0]);
            identifyRooms(baseGrid, floorRooms[1]);
            floorRooms[0].set(entranceX + 1, entranceY, entranceX + 1, entranceY + 1, kCorridorFlag);
            floorRooms[1].set(entranceX + 1, entranceY, entranceX + 1, entranceY + 1, kCorridorFlag);
            thirdFloorGrid = new SimpleGrid(baseGrid.width, baseGrid.height, kBlocked);
            setupThirdFloor();
            identifyRooms(thirdFloorGrid, floorRooms[2]);
        }

        public static boolean isHouse(SimpleGrid grid, int x, int y) {
            int value = grid.get(x, y);
            return value >= 1 && value <= 4;
        }

        public boolean isRoomId(SimpleGrid grid, int x, int y, int floor, int roomId) {
            return (floorRooms[floor].get(x, y) & kIdMask) == roomId;
        }

        public Direction get1x2RoomDirection(SimpleGrid grid, int x, int y, int floorNum, int roomId) {
            for (Direction dir : Direction.Plane.HORIZONTAL) {
                if (isRoomId(grid, x + dir.getStepX(), y + dir.getStepZ(), floorNum, roomId)) {
                    return dir;
                }
            }
            return null;
        }

        private void recursiveCorridor(SimpleGrid grid, int x, int y, Direction heading, int depth) {
            if (depth <= 0) {
                return;
            }
            int hx = heading.getStepX();
            int hz = heading.getStepZ();
            grid.set(x, y, kCorridor);
            grid.setif(x + hx, y + hz, 0, kCorridor);
            for (int attempt = 0; attempt < 8; attempt++) {
                Direction next = Direction.from2DDataValue(random.nextInt(4));
                if (next == heading.getOpposite() || next == Direction.EAST && random.nextBoolean()) {
                    continue;
                }
                int nx = x + hx;
                int nz = y + hz;
                if (grid.get(nx + next.getStepX(), nz + next.getStepZ()) == 0
                    && grid.get(nx + next.getStepX() * 2, nz + next.getStepZ() * 2) == 0) {
                    recursiveCorridor(grid, x + hx + next.getStepX(), y + hz + next.getStepZ(), next, depth - 1);
                    break;
                }
            }
            Direction cw = heading.getClockWise();
            Direction ccw = heading.getCounterClockWise();
            grid.setif(x + cw.getStepX(), y + cw.getStepZ(), 0, kRoom);
            grid.setif(x + ccw.getStepX(), y + ccw.getStepZ(), 0, kRoom);
            grid.setif(x + hx + cw.getStepX(), y + hz + cw.getStepZ(), 0, kRoom);
            grid.setif(x + hx + ccw.getStepX(), y + hz + ccw.getStepZ(), 0, kRoom);
            grid.setif(x + hx * 2, y + hz * 2, 0, kRoom);
            grid.setif(x + cw.getStepX() * 2, y + cw.getStepZ() * 2, 0, kRoom);
            grid.setif(x + ccw.getStepX() * 2, y + ccw.getStepZ() * 2, 0, kRoom);
        }

        private boolean cleanEdges(SimpleGrid grid) {
            boolean touched = false;
            for (int y = 0; y < grid.height; y++) {
                for (int x = 0; x < grid.width; x++) {
                    if (grid.get(x, y) != 0) {
                        continue;
                    }
                    int direct = (isHouse(grid, x + 1, y) ? 1 : 0) + (isHouse(grid, x - 1, y) ? 1 : 0)
                        + (isHouse(grid, x, y + 1) ? 1 : 0) + (isHouse(grid, x, y - 1) ? 1 : 0);
                    if (direct >= 3) {
                        grid.set(x, y, kRoom);
                        touched = true;
                    } else if (direct == 2) {
                        int diagonal = (isHouse(grid, x + 1, y + 1) ? 1 : 0) + (isHouse(grid, x - 1, y + 1) ? 1 : 0)
                            + (isHouse(grid, x + 1, y - 1) ? 1 : 0) + (isHouse(grid, x - 1, y - 1) ? 1 : 0);
                        if (diagonal <= 1) {
                            grid.set(x, y, kRoom);
                            touched = true;
                        }
                    }
                }
            }
            return touched;
        }

        private void setupThirdFloor() {
            List<GridPos> candidates = new ArrayList<>();
            SimpleGrid second = floorRooms[1];
            for (int y = 0; y < thirdFloorGrid.height; y++) {
                for (int x = 0; x < thirdFloorGrid.width; x++) {
                    int data = second.get(x, y);
                    if ((data & kTypeMask) == kRoom1x2 && (data & kDoorFlag) == kDoorFlag) {
                        candidates.add(new GridPos(x, y));
                    }
                }
            }
            if (candidates.isEmpty()) {
                thirdFloorGrid.set(0, 0, thirdFloorGrid.width, thirdFloorGrid.height, kBlocked);
                return;
            }
            GridPos pick = candidates.get(random.nextInt(candidates.size()));
            int data = second.get(pick.x(), pick.y());
            second.set(pick.x(), pick.y(), data | kStairsFlag);
            Direction roomDir = get1x2RoomDirection(baseGrid, pick.x(), pick.y(), 1, data & kIdMask);
            int endX = pick.x() + roomDir.getStepX();
            int endY = pick.y() + roomDir.getStepZ();
            for (int y = 0; y < thirdFloorGrid.height; y++) {
                for (int x = 0; x < thirdFloorGrid.width; x++) {
                    if (!isHouse(baseGrid, x, y)) {
                        thirdFloorGrid.set(x, y, kBlocked);
                    } else if (x == pick.x() && y == pick.y()) {
                        thirdFloorGrid.set(x, y, kStartRoom);
                    } else if (x == endX && y == endY) {
                        thirdFloorGrid.set(x, y, kStartRoom);
                        floorRooms[2].set(x, y, kCorridorFlag);
                    }
                }
            }
            List<Direction> exits = new ArrayList<>();
            for (Direction dir : Direction.Plane.HORIZONTAL) {
                if (thirdFloorGrid.get(endX + dir.getStepX(), endY + dir.getStepZ()) == 0) {
                    exits.add(dir);
                }
            }
            if (exits.isEmpty()) {
                thirdFloorGrid.set(0, 0, thirdFloorGrid.width, thirdFloorGrid.height, kBlocked);
                second.set(pick.x(), pick.y(), data);
                return;
            }
            Direction exit = exits.get(random.nextInt(exits.size()));
            recursiveCorridor(thirdFloorGrid, endX + exit.getStepX(), endY + exit.getStepZ(), exit, 4);
            while (cleanEdges(thirdFloorGrid)) {
                continue;
            }
        }

        private void identifyRooms(SimpleGrid fromGrid, SimpleGrid roomGrid) {
            List<GridPos> spots = new ArrayList<>();
            for (int y = 0; y < fromGrid.height; y++) {
                for (int x = 0; x < fromGrid.width; x++) {
                    if (fromGrid.get(x, y) == kRoom) {
                        spots.add(new GridPos(x, y));
                    }
                }
            }
            Util.shuffle(spots, random);
            int roomId = 10;
            for (GridPos spot : spots) {
                int x = spot.x();
                int y = spot.y();
                if (roomGrid.get(x, y) != 0) {
                    continue;
                }
                int x0 = x;
                int x1 = x;
                int y0 = y;
                int y1 = y;
                int type = kRoom1x1;
                if (free(roomGrid, fromGrid, x + 1, y) && free(roomGrid, fromGrid, x, y + 1)
                    && free(roomGrid, fromGrid, x + 1, y + 1)) {
                    x1 = x + 1;
                    y1 = y + 1;
                    type = kRoom2x2;
                } else if (free(roomGrid, fromGrid, x - 1, y) && free(roomGrid, fromGrid, x, y + 1)
                    && free(roomGrid, fromGrid, x - 1, y + 1)) {
                    x0 = x - 1;
                    y1 = y + 1;
                    type = kRoom2x2;
                } else if (free(roomGrid, fromGrid, x - 1, y) && free(roomGrid, fromGrid, x, y - 1)
                    && free(roomGrid, fromGrid, x - 1, y - 1)) {
                    x0 = x - 1;
                    y0 = y - 1;
                    type = kRoom2x2;
                } else if (free(roomGrid, fromGrid, x + 1, y)) {
                    x1 = x + 1;
                    type = kRoom1x2;
                } else if (free(roomGrid, fromGrid, x, y + 1)) {
                    y1 = y + 1;
                    type = kRoom1x2;
                } else if (free(roomGrid, fromGrid, x - 1, y)) {
                    x0 = x - 1;
                    type = kRoom1x2;
                } else if (free(roomGrid, fromGrid, x, y - 1)) {
                    y0 = y - 1;
                    type = kRoom1x2;
                }
                int doorX = random.nextBoolean() ? x0 : x1;
                int doorY = random.nextBoolean() ? y0 : y1;
                int doorFlag = kDoorFlag;
                if (!fromGrid.edgesTo(doorX, doorY, kCorridor)) {
                    doorX = doorX == x0 ? x1 : x0;
                    doorY = doorY == y0 ? y1 : y0;
                    if (!fromGrid.edgesTo(doorX, doorY, kCorridor)) {
                        doorY = doorY == y0 ? y1 : y0;
                        if (!fromGrid.edgesTo(doorX, doorY, kCorridor)) {
                            doorX = doorX == x0 ? x1 : x0;
                            doorY = doorY == y0 ? y1 : y0;
                            if (!fromGrid.edgesTo(doorX, doorY, kCorridor)) {
                                doorFlag = 0;
                                doorX = x0;
                                doorY = y0;
                            }
                        }
                    }
                }
                for (int ry = y0; ry <= y1; ry++) {
                    for (int rx = x0; rx <= x1; rx++) {
                        boolean door = rx == doorX && ry == doorY;
                        roomGrid.set(rx, ry, (door ? kOriginFlag | doorFlag : 0) | type | roomId);
                    }
                }
                roomId++;
            }
        }

        private static boolean free(SimpleGrid roomGrid, SimpleGrid fromGrid, int x, int y) {
            return roomGrid.get(x, y) == 0 && fromGrid.get(x, y) == kRoom;
        }

        record GridPos(int x, int y) {
        }
    }

    static class SimpleGrid {
        private final int[][] grid;
        private final int width;
        private final int height;
        private final int valueIfOutside;

        public SimpleGrid(int width, int height, int valueIfOutside) {
            this.width = width;
            this.height = height;
            this.valueIfOutside = valueIfOutside;
            this.grid = new int[width][height];
        }

        public void set(int x, int y, int value) {
            if (x >= 0 && x < width && y >= 0 && y < height) {
                grid[x][y] = value;
            }
        }

        public void set(int x0, int y0, int x1, int y1, int value) {
            for (int y = y0; y <= y1; y++) {
                for (int x = x0; x <= x1; x++) {
                    set(x, y, value);
                }
            }
        }

        public int get(int x, int y) {
            return x >= 0 && x < width && y >= 0 && y < height ? grid[x][y] : valueIfOutside;
        }

        public void setif(int x, int y, int ifValue, int value) {
            if (get(x, y) == ifValue) {
                set(x, y, value);
            }
        }

        public boolean edgesTo(int x, int y, int ifValue) {
            return get(x - 1, y) == ifValue || get(x + 1, y) == ifValue || get(x, y + 1) == ifValue
                || get(x, y - 1) == ifValue;
        }
    }

    static abstract class FloorRoomCollection {
        public abstract String get1x1(RandomSource random);

        public abstract String get1x1Secret(RandomSource random);

        public abstract String get1x2SideEntrance(RandomSource random, boolean isStairsRoom);

        public abstract String get1x2FrontEntrance(RandomSource random, boolean isStairsRoom);

        public abstract String get1x2Secret(RandomSource random);

        public abstract String get2x2(RandomSource random);

        public abstract String get2x2Secret(RandomSource random);
    }

    static class FirstFloorRoomCollection extends FloorRoomCollection {
        public String get1x1(RandomSource random) {
            return "1x1_a" + (random.nextInt(5) + 1);
        }

        public String get1x1Secret(RandomSource random) {
            return "1x1_as" + (random.nextInt(4) + 1);
        }

        public String get1x2SideEntrance(RandomSource random, boolean isStairsRoom) {
            return "1x2_a" + (random.nextInt(9) + 1);
        }

        public String get1x2FrontEntrance(RandomSource random, boolean isStairsRoom) {
            return "1x2_b" + (random.nextInt(5) + 1);
        }

        public String get1x2Secret(RandomSource random) {
            return "1x2_s" + (random.nextInt(2) + 1);
        }

        public String get2x2(RandomSource random) {
            return "2x2_a" + (random.nextInt(4) + 1);
        }

        public String get2x2Secret(RandomSource random) {
            return "2x2_s1";
        }
    }

    static class SecondFloorRoomCollection extends FloorRoomCollection {
        public String get1x1(RandomSource random) {
            return "1x1_b" + (random.nextInt(5) + 1);
        }

        public String get1x1Secret(RandomSource random) {
            return "1x1_as" + (random.nextInt(4) + 1);
        }

        public String get1x2SideEntrance(RandomSource random, boolean isStairsRoom) {
            return isStairsRoom ? "1x2_c_stairs" : "1x2_c" + (random.nextInt(4) + 1);
        }

        public String get1x2FrontEntrance(RandomSource random, boolean isStairsRoom) {
            return isStairsRoom ? "1x2_d_stairs" : "1x2_d" + (random.nextInt(5) + 1);
        }

        public String get1x2Secret(RandomSource random) {
            return "1x2_se" + (random.nextInt(1) + 1);
        }

        public String get2x2(RandomSource random) {
            return "2x2_b" + (random.nextInt(5) + 1);
        }

        public String get2x2Secret(RandomSource random) {
            return "2x2_s1";
        }
    }

    static class ThirdFloorRoomCollection extends SecondFloorRoomCollection {
    }
}
