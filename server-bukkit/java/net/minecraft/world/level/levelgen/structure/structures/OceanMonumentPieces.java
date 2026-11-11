// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.structure.structures;

import java.util.ArrayList;
import java.util.List;
import java.util.Set;
import net.minecraft.core.BlockPos;
import net.minecraft.core.Direction;
import net.minecraft.nbt.CompoundTag;
import net.minecraft.util.RandomSource;
import net.minecraft.util.Util;
import net.minecraft.world.entity.EntitySpawnReason;
import net.minecraft.world.entity.EntityTypes;
import net.minecraft.world.entity.monster.ElderGuardian;
import net.minecraft.world.level.ChunkPos;
import net.minecraft.world.level.StructureManager;
import net.minecraft.world.level.WorldGenLevel;
import net.minecraft.world.level.block.Block;
import net.minecraft.world.level.block.Blocks;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.chunk.ChunkGenerator;
import net.minecraft.world.level.levelgen.structure.BoundingBox;
import net.minecraft.world.level.levelgen.structure.StructurePiece;
import net.minecraft.world.level.levelgen.structure.pieces.StructurePieceSerializationContext;
import net.minecraft.world.level.levelgen.structure.pieces.StructurePieceType;

public class OceanMonumentPieces {
    private OceanMonumentPieces() {
    }

    private static final int kDown = 0;
    private static final int kUp = 1;
    private static final int kNorth = 2;
    private static final int kSouth = 3;
    private static final int kWest = 4;
    private static final int kEast = 5;

    protected static abstract class OceanMonumentPiece extends StructurePiece {
        protected static final BlockState BASE_GRAY = Blocks.PRISMARINE.defaultBlockState();
        protected static final BlockState BASE_LIGHT = Blocks.PRISMARINE_BRICKS.defaultBlockState();
        protected static final BlockState BASE_BLACK = Blocks.DARK_PRISMARINE.defaultBlockState();
        protected static final BlockState DOT_DECO_DATA = BASE_LIGHT;
        protected static final BlockState LAMP_BLOCK = Blocks.SEA_LANTERN.defaultBlockState();
        protected static final boolean DO_FILL = true;
        protected static final BlockState FILL_BLOCK = Blocks.WATER.defaultBlockState();
        protected static final Set<Block> FILL_KEEP = Set.of(Blocks.ICE, Blocks.PACKED_ICE, Blocks.BLUE_ICE,
            FILL_BLOCK.getBlock());
        protected static final int GRIDROOM_WIDTH = 8;
        protected static final int GRIDROOM_DEPTH = 8;
        protected static final int GRIDROOM_HEIGHT = 4;
        protected static final int GRID_WIDTH = 5;
        protected static final int GRID_DEPTH = 5;
        protected static final int GRID_HEIGHT = 3;
        protected static final int GRID_FLOOR_COUNT = 25;
        protected static final int GRID_SIZE = 75;
        protected static final int GRIDROOM_SOURCE_INDEX = getRoomIndex(2, 0, 0);
        protected static final int GRIDROOM_TOP_CONNECT_INDEX = getRoomIndex(2, 2, 0);
        protected static final int GRIDROOM_LEFTWING_CONNECT_INDEX = getRoomIndex(0, 1, 0);
        protected static final int GRIDROOM_RIGHTWING_CONNECT_INDEX = getRoomIndex(4, 1, 0);
        protected static final int LEFTWING_INDEX = 1001;
        protected static final int RIGHTWING_INDEX = 1002;
        protected static final int PENTHOUSE_INDEX = 1003;
        protected RoomDefinition roomDefinition;

        protected static int getRoomIndex(int roomX, int roomY, int roomZ) {
            return roomY * 25 + roomZ * 5 + roomX;
        }

        public OceanMonumentPiece(StructurePieceType type, Direction orientation, int genDepth,
                BoundingBox boundingBox) {
            super(type, genDepth, boundingBox);
            setOrientation(orientation);
        }

        protected OceanMonumentPiece(StructurePieceType type, int genDepth, Direction orientation,
                RoomDefinition roomDefinition, int roomWidth, int roomHeight, int roomDepth) {
            super(type, genDepth, makeRoomBox(orientation, roomDefinition, roomWidth, roomHeight, roomDepth));
            setOrientation(orientation);
            this.roomDefinition = roomDefinition;
        }

        private static BoundingBox makeRoomBox(Direction orientation, RoomDefinition definition, int roomWidth,
                int roomHeight, int roomDepth) {
            int index = definition.index;
            int roomX = index % 5;
            int roomZ = index / 5 % 5;
            int roomY = index / 25;
            BoundingBox box = makeBoundingBox(0, 0, 0, orientation, roomWidth * 8, roomHeight * 4, roomDepth * 8);
            switch (orientation) {
                case NORTH -> box.move(roomX * 8, roomY * 4, -(roomZ + roomDepth) * 8 + 1);
                case SOUTH -> box.move(roomX * 8, roomY * 4, roomZ * 8);
                case WEST -> box.move(-(roomZ + roomDepth) * 8 + 1, roomY * 4, roomX * 8);
                default -> box.move(roomZ * 8, roomY * 4, roomX * 8);
            }
            return box;
        }

        public OceanMonumentPiece(StructurePieceType type, CompoundTag tag) {
            super(type, tag);
        }

        protected void addAdditionalSaveData(StructurePieceSerializationContext context, CompoundTag tag) {
        }

        protected void box(WorldGenLevel level, BoundingBox cb, BlockState state, int x0, int y0, int z0, int x1,
                int y1, int z1) {
            generateBox(level, cb, x0, y0, z0, x1, y1, z1, state, state, false);
        }

        protected void light(WorldGenLevel level, BoundingBox cb, int x0, int y0, int z0, int x1, int y1, int z1) {
            box(level, cb, BASE_LIGHT, x0, y0, z0, x1, y1, z1);
        }

        protected void gray(WorldGenLevel level, BoundingBox cb, int x0, int y0, int z0, int x1, int y1, int z1) {
            box(level, cb, BASE_GRAY, x0, y0, z0, x1, y1, z1);
        }

        protected void dark(WorldGenLevel level, BoundingBox cb, int x0, int y0, int z0, int x1, int y1, int z1) {
            box(level, cb, BASE_BLACK, x0, y0, z0, x1, y1, z1);
        }

        protected void generateWaterBox(WorldGenLevel level, BoundingBox cb, int x0, int y0, int z0, int x1, int y1,
                int z1) {
            for (int y = y0; y <= y1; y++) {
                for (int x = x0; x <= x1; x++) {
                    for (int z = z0; z <= z1; z++) {
                        BlockState existing = getBlock(level, x, y, z, cb);
                        if (FILL_KEEP.contains(existing.getBlock())) {
                            continue;
                        }
                        if (getWorldY(y) >= level.getSeaLevel() && existing != FILL_BLOCK) {
                            placeBlock(level, Blocks.AIR.defaultBlockState(), x, y, z, cb);
                        } else {
                            placeBlock(level, FILL_BLOCK, x, y, z, cb);
                        }
                    }
                }
            }
        }

        protected void generateDefaultFloor(WorldGenLevel level, BoundingBox cb, int xOff, int zOff,
                boolean downOpening) {
            if (!downOpening) {
                gray(level, cb, xOff, 0, zOff, xOff + 7, 0, zOff + 7);
                return;
            }
            gray(level, cb, xOff, 0, zOff, xOff + 2, 0, zOff + 7);
            gray(level, cb, xOff + 5, 0, zOff, xOff + 7, 0, zOff + 7);
            gray(level, cb, xOff + 3, 0, zOff, xOff + 4, 0, zOff + 2);
            gray(level, cb, xOff + 3, 0, zOff + 5, xOff + 4, 0, zOff + 7);
            light(level, cb, xOff + 3, 0, zOff + 2, xOff + 4, 0, zOff + 2);
            light(level, cb, xOff + 3, 0, zOff + 5, xOff + 4, 0, zOff + 5);
            light(level, cb, xOff + 2, 0, zOff + 3, xOff + 2, 0, zOff + 4);
            light(level, cb, xOff + 5, 0, zOff + 3, xOff + 5, 0, zOff + 4);
        }

        protected void generateBoxOnFillOnly(WorldGenLevel level, BoundingBox cb, int x0, int y0, int z0, int x1,
                int y1, int z1, BlockState targetBlock) {
            for (int y = y0; y <= y1; y++) {
                for (int x = x0; x <= x1; x++) {
                    for (int z = z0; z <= z1; z++) {
                        if (getBlock(level, x, y, z, cb) == FILL_BLOCK) {
                            placeBlock(level, targetBlock, x, y, z, cb);
                        }
                    }
                }
            }
        }

        protected boolean chunkIntersects(BoundingBox cb, int x0, int z0, int x1, int z1) {
            int wx0 = getWorldX(x0, z0);
            int wz0 = getWorldZ(x0, z0);
            int wx1 = getWorldX(x1, z1);
            int wz1 = getWorldZ(x1, z1);
            return cb.intersects(Math.min(wx0, wx1), Math.min(wz0, wz1), Math.max(wx0, wx1), Math.max(wz0, wz1));
        }

        protected void spawnElder(WorldGenLevel level, BoundingBox cb, int x, int y, int z) {
            BlockPos pos = getWorldPos(x, y, z);
            if (!cb.isInside(pos)) {
                return;
            }
            ElderGuardian elder = (ElderGuardian) EntityTypes.ELDER_GUARDIAN.create(level.getLevel(),
                EntitySpawnReason.STRUCTURE);
            if (elder == null) {
                return;
            }
            elder.heal(elder.getMaxHealth());
            elder.snapTo(pos.getX() + 0.5, pos.getY(), pos.getZ() + 0.5, 0.0F, 0.0F);
            elder.finalizeSpawn(level, level.getCurrentDifficultyAt(elder.blockPosition()),
                EntitySpawnReason.STRUCTURE, null);
            level.addFreshEntityWithPassengers(elder);
        }

        protected static boolean open(RoomDefinition room, int direction) {
            return room.hasOpening[direction];
        }
    }

    public static class MonumentBuilding extends OceanMonumentPiece {
        public static final int BIOME_RANGE_CHECK = 29;
        private RoomDefinition sourceRoom;
        private RoomDefinition coreRoom;
        private final List<OceanMonumentPiece> childPieces = new ArrayList<>();

        public MonumentBuilding(RandomSource random, int west, int north, Direction direction) {
            super(StructurePieceType.OCEAN_MONUMENT_BUILDING, direction, 0,
                makeBoundingBox(west, 39, north, direction, 58, 23, 58));
            setOrientation(direction);
            List<RoomDefinition> roomDefinitions = generateRoomGraph(random);
            sourceRoom.claimed = true;
            childPieces.add(new OceanMonumentEntryRoom(direction, sourceRoom));
            childPieces.add(new OceanMonumentCoreRoom(direction, coreRoom));
            List<MonumentRoomFitter> fitters = List.of(new FitDoubleXYRoom(), new FitDoubleYZRoom(),
                new FitDoubleZRoom(), new FitDoubleXRoom(), new FitDoubleYRoom(), new FitSimpleTopRoom(),
                new FitSimpleRoom());
            for (RoomDefinition definition : roomDefinitions) {
                if (definition.claimed || definition.isSpecial()) {
                    continue;
                }
                for (MonumentRoomFitter fitter : fitters) {
                    if (fitter.fits(definition)) {
                        childPieces.add(fitter.create(direction, definition, random));
                        break;
                    }
                }
            }
            BlockPos offset = getWorldPos(9, 0, 22);
            for (OceanMonumentPiece child : childPieces) {
                child.getBoundingBox().move(offset);
            }
            BoundingBox leftWing = BoundingBox.fromCorners(getWorldPos(1, 1, 1), getWorldPos(23, 8, 21));
            BoundingBox rightWing = BoundingBox.fromCorners(getWorldPos(34, 1, 1), getWorldPos(56, 8, 21));
            BoundingBox penthouse = BoundingBox.fromCorners(getWorldPos(22, 13, 22), getWorldPos(35, 17, 35));
            int wingRandom = random.nextInt();
            childPieces.add(new OceanMonumentWingRoom(direction, leftWing, wingRandom++));
            childPieces.add(new OceanMonumentWingRoom(direction, rightWing, wingRandom));
            childPieces.add(new OceanMonumentPenthouse(direction, penthouse));
        }

        public MonumentBuilding(CompoundTag tag) {
            super(StructurePieceType.OCEAN_MONUMENT_BUILDING, tag);
        }

        private List<RoomDefinition> generateRoomGraph(RandomSource random) {
            RoomDefinition[] grid = new RoomDefinition[75];
            for (int y = 0; y < 3; y++) {
                int xFrom = y == 2 ? 1 : 0;
                int xTo = y == 2 ? 4 : 5;
                int zTo = y == 2 ? 2 : 4;
                for (int x = xFrom; x < xTo; x++) {
                    for (int z = 0; z < zTo; z++) {
                        int pos = getRoomIndex(x, y, z);
                        grid[pos] = new RoomDefinition(pos);
                    }
                }
            }
            sourceRoom = grid[GRIDROOM_SOURCE_INDEX];
            for (int x = 0; x < 5; x++) {
                for (int z = 0; z < 5; z++) {
                    for (int y = 0; y < 3; y++) {
                        int pos = getRoomIndex(x, y, z);
                        if (grid[pos] == null) {
                            continue;
                        }
                        for (Direction direction : Direction.values()) {
                            int nx = x + direction.getStepX();
                            int ny = y + direction.getStepY();
                            int nz = z + direction.getStepZ();
                            if (nx < 0 || nx >= 5 || nz < 0 || nz >= 5 || ny < 0 || ny >= 3) {
                                continue;
                            }
                            RoomDefinition neighbour = grid[getRoomIndex(nx, ny, nz)];
                            if (neighbour == null) {
                                continue;
                            }
                            grid[pos].setConnection(nz == z ? direction : direction.getOpposite(), neighbour);
                        }
                    }
                }
            }
            RoomDefinition roofRoom = new RoomDefinition(PENTHOUSE_INDEX);
            RoomDefinition leftWing = new RoomDefinition(LEFTWING_INDEX);
            RoomDefinition rightWing = new RoomDefinition(RIGHTWING_INDEX);
            grid[GRIDROOM_TOP_CONNECT_INDEX].setConnection(Direction.UP, roofRoom);
            grid[GRIDROOM_LEFTWING_CONNECT_INDEX].setConnection(Direction.SOUTH, leftWing);
            grid[GRIDROOM_RIGHTWING_CONNECT_INDEX].setConnection(Direction.SOUTH, rightWing);
            roofRoom.claimed = true;
            leftWing.claimed = true;
            rightWing.claimed = true;
            sourceRoom.isSource = true;
            coreRoom = grid[getRoomIndex(random.nextInt(4), 0, 2)];
            coreRoom.claimed = true;
            RoomDefinition coreEast = coreRoom.connections[kEast];
            RoomDefinition coreNorth = coreRoom.connections[kNorth];
            RoomDefinition coreNorthEast = coreEast.connections[kNorth];
            for (RoomDefinition floor : new RoomDefinition[] {coreEast, coreNorth, coreNorthEast}) {
                floor.claimed = true;
            }
            for (RoomDefinition floor : new RoomDefinition[] {coreRoom, coreEast, coreNorth, coreNorthEast}) {
                floor.connections[kUp].claimed = true;
            }
            List<RoomDefinition> roomDefs = new ArrayList<>();
            for (RoomDefinition definition : grid) {
                if (definition != null) {
                    definition.updateOpenings();
                    roomDefs.add(definition);
                }
            }
            roofRoom.updateOpenings();
            Util.shuffle(roomDefs, random);
            int scanIndex = 1;
            for (RoomDefinition definition : roomDefs) {
                int closeCount = 0;
                int attempts = 0;
                while (closeCount < 2 && attempts < 5) {
                    attempts++;
                    int face = random.nextInt(6);
                    if (!definition.hasOpening[face]) {
                        continue;
                    }
                    int opposite = Direction.from3DDataValue(face).getOpposite().get3DDataValue();
                    definition.hasOpening[face] = false;
                    definition.connections[face].hasOpening[opposite] = false;
                    if (definition.findSource(scanIndex++) && definition.connections[face].findSource(scanIndex++)) {
                        closeCount++;
                    } else {
                        definition.hasOpening[face] = true;
                        definition.connections[face].hasOpening[opposite] = true;
                    }
                }
            }
            roomDefs.add(roofRoom);
            roomDefs.add(leftWing);
            roomDefs.add(rightWing);
            return roomDefs;
        }

        public void postProcess(WorldGenLevel level, StructureManager structureManager, ChunkGenerator generator,
                RandomSource random, BoundingBox cb, ChunkPos chunkPos, BlockPos referencePos) {
            int waterHeight = Math.max(level.getSeaLevel(), 64) - boundingBox.minY();
            generateWaterBox(level, cb, 0, 0, 0, 58, waterHeight, 58);
            generateWing(false, 0, level, cb);
            generateWing(true, 33, level, cb);
            generateEntranceArchs(level, cb);
            generateEntranceWall(level, cb);
            generateRoofPiece(level, cb);
            generateLowerWall(level, cb);
            generateMiddleWall(level, cb);
            generateUpperWall(level, cb);
            for (int px = 0; px < 7; px++) {
                int pz = 0;
                while (pz < 7) {
                    if (pz == 0 && px == 3) {
                        pz = 6;
                    }
                    int bx = px * 9;
                    int bz = pz * 9;
                    for (int w = 0; w < 4; w++) {
                        for (int d = 0; d < 4; d++) {
                            placeBlock(level, BASE_LIGHT, bx + w, 0, bz + d, cb);
                            fillColumnDown(level, BASE_LIGHT, bx + w, -1, bz + d, cb);
                        }
                    }
                    pz += px != 0 && px != 6 ? 6 : 1;
                }
            }
            for (int i = 0; i < 5; i++) {
                generateWaterBox(level, cb, -1 - i, i * 2, -1 - i, -1 - i, 23, 58 + i);
                generateWaterBox(level, cb, 58 + i, i * 2, -1 - i, 58 + i, 23, 58 + i);
                generateWaterBox(level, cb, -i, i * 2, -1 - i, 57 + i, 23, -1 - i);
                generateWaterBox(level, cb, -i, i * 2, 58 + i, 57 + i, 23, 58 + i);
            }
            for (OceanMonumentPiece child : childPieces) {
                if (child.getBoundingBox().intersects(cb)) {
                    child.postProcess(level, structureManager, generator, random, cb, chunkPos, referencePos);
                }
            }
        }

        private void generateWing(boolean flipped, int xo, WorldGenLevel level, BoundingBox cb) {
            if (!chunkIntersects(cb, xo, 0, xo + 23, 20)) {
                return;
            }
            gray(level, cb, xo, 0, 0, xo + 24, 0, 20);
            generateWaterBox(level, cb, xo, 1, 0, xo + 24, 10, 20);
            for (int i = 0; i < 4; i++) {
                light(level, cb, xo + i, i + 1, i, xo + i, i + 1, 20);
                light(level, cb, xo + i + 7, i + 5, i + 7, xo + i + 7, i + 5, 20);
                light(level, cb, xo + 17 - i, i + 5, i + 7, xo + 17 - i, i + 5, 20);
                light(level, cb, xo + 24 - i, i + 1, i, xo + 24 - i, i + 1, 20);
                light(level, cb, xo + i + 1, i + 1, i, xo + 23 - i, i + 1, i);
                light(level, cb, xo + i + 8, i + 5, i + 7, xo + 16 - i, i + 5, i + 7);
            }
            gray(level, cb, xo + 4, 4, 4, xo + 6, 4, 20);
            gray(level, cb, xo + 7, 4, 4, xo + 17, 4, 6);
            gray(level, cb, xo + 18, 4, 4, xo + 20, 4, 20);
            gray(level, cb, xo + 11, 8, 11, xo + 13, 8, 20);
            placeBlock(level, DOT_DECO_DATA, xo + 12, 9, 12, cb);
            placeBlock(level, DOT_DECO_DATA, xo + 12, 9, 15, cb);
            placeBlock(level, DOT_DECO_DATA, xo + 12, 9, 18, cb);
            int leftPos = xo + (flipped ? 19 : 5);
            int rightPos = xo + (flipped ? 5 : 19);
            for (int z = 20; z >= 5; z -= 3) {
                placeBlock(level, DOT_DECO_DATA, leftPos, 5, z, cb);
            }
            for (int z = 19; z >= 7; z -= 3) {
                placeBlock(level, DOT_DECO_DATA, rightPos, 5, z, cb);
            }
            for (int i = 0; i < 4; i++) {
                int pos = flipped ? xo + 24 - (17 - i * 3) : xo + 17 - i * 3;
                placeBlock(level, DOT_DECO_DATA, pos, 5, 5, cb);
            }
            placeBlock(level, DOT_DECO_DATA, rightPos, 5, 5, cb);
            gray(level, cb, xo + 11, 1, 12, xo + 13, 7, 12);
            gray(level, cb, xo + 12, 1, 11, xo + 12, 7, 13);
        }

        private void generateEntranceArchs(WorldGenLevel level, BoundingBox cb) {
            if (!chunkIntersects(cb, 22, 5, 35, 17)) {
                return;
            }
            generateWaterBox(level, cb, 25, 0, 0, 32, 8, 20);
            for (int i = 0; i < 4; i++) {
                int z = 5 + i * 4;
                light(level, cb, 24, 2, z, 24, 4, z);
                light(level, cb, 22, 4, z, 23, 4, z);
                placeBlock(level, BASE_LIGHT, 25, 5, z, cb);
                placeBlock(level, BASE_LIGHT, 26, 6, z, cb);
                placeBlock(level, LAMP_BLOCK, 26, 5, z, cb);
                light(level, cb, 33, 2, z, 33, 4, z);
                light(level, cb, 34, 4, z, 35, 4, z);
                placeBlock(level, BASE_LIGHT, 32, 5, z, cb);
                placeBlock(level, BASE_LIGHT, 31, 6, z, cb);
                placeBlock(level, LAMP_BLOCK, 31, 5, z, cb);
                gray(level, cb, 27, 6, z, 30, 6, z);
            }
        }

        private void generateEntranceWall(WorldGenLevel level, BoundingBox cb) {
            if (!chunkIntersects(cb, 15, 20, 42, 21)) {
                return;
            }
            int[][] grays = {{15, 0, 42, 0}, {21, 12, 36, 12}, {17, 11, 40, 11}, {16, 10, 41, 10}, {15, 7, 42, 9},
                {16, 6, 41, 6}, {17, 5, 40, 5}, {21, 4, 36, 4}, {22, 3, 26, 3}, {31, 3, 35, 3}, {23, 2, 25, 2},
                {32, 2, 34, 2}};
            gray(level, cb, 15, 0, 21, 42, 0, 21);
            generateWaterBox(level, cb, 26, 1, 21, 31, 3, 21);
            for (int i = 1; i < grays.length; i++) {
                int[] g = grays[i];
                gray(level, cb, g[0], g[1], 21, g[2], g[3], 21);
            }
            light(level, cb, 28, 4, 20, 29, 4, 21);
            int[][] lights = {{27, 3}, {30, 3}, {26, 2}, {31, 2}, {25, 1}, {32, 1}};
            for (int[] p : lights) {
                placeBlock(level, BASE_LIGHT, p[0], p[1], 21, cb);
            }
            for (int i = 0; i < 7; i++) {
                placeBlock(level, BASE_BLACK, 28 - i, 6 + i, 21, cb);
                placeBlock(level, BASE_BLACK, 29 + i, 6 + i, 21, cb);
            }
            for (int i = 0; i < 4; i++) {
                placeBlock(level, BASE_BLACK, 28 - i, 9 + i, 21, cb);
                placeBlock(level, BASE_BLACK, 29 + i, 9 + i, 21, cb);
            }
            placeBlock(level, BASE_BLACK, 28, 12, 21, cb);
            placeBlock(level, BASE_BLACK, 29, 12, 21, cb);
            for (int i = 0; i < 3; i++) {
                placeBlock(level, BASE_BLACK, 22 - i * 2, 8, 21, cb);
                placeBlock(level, BASE_BLACK, 22 - i * 2, 9, 21, cb);
                placeBlock(level, BASE_BLACK, 35 + i * 2, 8, 21, cb);
                placeBlock(level, BASE_BLACK, 35 + i * 2, 9, 21, cb);
            }
            int[][] waters = {{15, 13, 42, 15}, {15, 1, 15, 6}, {16, 1, 16, 5}, {17, 1, 20, 4}, {21, 1, 21, 3},
                {22, 1, 22, 2}, {23, 1, 24, 1}, {42, 1, 42, 6}, {41, 1, 41, 5}, {37, 1, 40, 4}, {36, 1, 36, 3},
                {33, 1, 34, 1}, {35, 1, 35, 2}};
            for (int[] w : waters) {
                generateWaterBox(level, cb, w[0], w[1], 21, w[2], w[3], 21);
            }
        }

        private void generateRoofPiece(WorldGenLevel level, BoundingBox cb) {
            if (!chunkIntersects(cb, 21, 21, 36, 36)) {
                return;
            }
            gray(level, cb, 21, 0, 22, 36, 0, 36);
            generateWaterBox(level, cb, 21, 1, 22, 36, 23, 36);
            for (int i = 0; i < 4; i++) {
                light(level, cb, 21 + i, 13 + i, 21 + i, 36 - i, 13 + i, 21 + i);
                light(level, cb, 21 + i, 13 + i, 36 - i, 36 - i, 13 + i, 36 - i);
                light(level, cb, 21 + i, 13 + i, 22 + i, 21 + i, 13 + i, 35 - i);
                light(level, cb, 36 - i, 13 + i, 22 + i, 36 - i, 13 + i, 35 - i);
            }
            gray(level, cb, 25, 16, 25, 32, 16, 32);
            light(level, cb, 25, 17, 25, 25, 19, 25);
            light(level, cb, 32, 17, 25, 32, 19, 25);
            light(level, cb, 25, 17, 32, 25, 19, 32);
            light(level, cb, 32, 17, 32, 32, 19, 32);
            int[][] lamps = {{26, 26, 27, 27}, {26, 31, 27, 30}, {31, 31, 30, 30}, {31, 26, 30, 27}};
            for (int[] p : lamps) {
                placeBlock(level, BASE_LIGHT, p[0], 20, p[1], cb);
                placeBlock(level, BASE_LIGHT, p[2], 21, p[3], cb);
                placeBlock(level, LAMP_BLOCK, p[2], 20, p[3], cb);
            }
            gray(level, cb, 28, 21, 27, 29, 21, 27);
            gray(level, cb, 27, 21, 28, 27, 21, 29);
            gray(level, cb, 28, 21, 30, 29, 21, 30);
            gray(level, cb, 30, 21, 28, 30, 21, 29);
        }

        private void generateLowerWall(WorldGenLevel level, BoundingBox cb) {
            if (chunkIntersects(cb, 0, 21, 6, 58)) {
                gray(level, cb, 0, 0, 21, 6, 0, 57);
                generateWaterBox(level, cb, 0, 1, 21, 6, 7, 57);
                gray(level, cb, 4, 4, 21, 6, 4, 53);
                for (int i = 0; i < 4; i++) {
                    light(level, cb, i, i + 1, 21, i, i + 1, 57 - i);
                }
                for (int z = 23; z < 53; z += 3) {
                    placeBlock(level, DOT_DECO_DATA, 5, 5, z, cb);
                }
                placeBlock(level, DOT_DECO_DATA, 5, 5, 52, cb);
                for (int i = 0; i < 4; i++) {
                    light(level, cb, i, i + 1, 21, i, i + 1, 57 - i);
                }
                gray(level, cb, 4, 1, 52, 6, 3, 52);
                gray(level, cb, 5, 1, 51, 5, 3, 53);
            }
            if (chunkIntersects(cb, 51, 21, 58, 58)) {
                gray(level, cb, 51, 0, 21, 57, 0, 57);
                generateWaterBox(level, cb, 51, 1, 21, 57, 7, 57);
                gray(level, cb, 51, 4, 21, 53, 4, 53);
                for (int i = 0; i < 4; i++) {
                    light(level, cb, 57 - i, i + 1, 21, 57 - i, i + 1, 57 - i);
                }
                for (int z = 23; z < 53; z += 3) {
                    placeBlock(level, DOT_DECO_DATA, 52, 5, z, cb);
                }
                placeBlock(level, DOT_DECO_DATA, 52, 5, 52, cb);
                gray(level, cb, 51, 1, 52, 53, 3, 52);
                gray(level, cb, 52, 1, 51, 52, 3, 53);
            }
            if (chunkIntersects(cb, 0, 51, 57, 57)) {
                gray(level, cb, 7, 0, 51, 50, 0, 57);
                generateWaterBox(level, cb, 7, 1, 51, 50, 10, 57);
                for (int i = 0; i < 4; i++) {
                    light(level, cb, i + 1, i + 1, 57 - i, 56 - i, i + 1, 57 - i);
                }
            }
        }

        private void generateMiddleWall(WorldGenLevel level, BoundingBox cb) {
            if (chunkIntersects(cb, 7, 21, 13, 50)) {
                gray(level, cb, 7, 0, 21, 13, 0, 50);
                generateWaterBox(level, cb, 7, 1, 21, 13, 10, 50);
                gray(level, cb, 11, 8, 21, 13, 8, 53);
                for (int i = 0; i < 4; i++) {
                    light(level, cb, i + 7, i + 5, 21, i + 7, i + 5, 54);
                }
                for (int z = 21; z <= 45; z += 3) {
                    placeBlock(level, DOT_DECO_DATA, 12, 9, z, cb);
                }
            }
            if (chunkIntersects(cb, 44, 21, 50, 54)) {
                gray(level, cb, 44, 0, 21, 50, 0, 50);
                generateWaterBox(level, cb, 44, 1, 21, 50, 10, 50);
                gray(level, cb, 44, 8, 21, 46, 8, 53);
                for (int i = 0; i < 4; i++) {
                    light(level, cb, 50 - i, i + 5, 21, 50 - i, i + 5, 54);
                }
                for (int z = 21; z <= 45; z += 3) {
                    placeBlock(level, DOT_DECO_DATA, 45, 9, z, cb);
                }
            }
            if (chunkIntersects(cb, 8, 44, 49, 54)) {
                gray(level, cb, 14, 0, 44, 43, 0, 50);
                generateWaterBox(level, cb, 14, 1, 44, 43, 10, 50);
                int[][] extra = {{9, 47}, {9, 50}, {10, 45}, {10, 46}, {10, 51}, {10, 52}, {11, 47}, {11, 50},
                    {12, 48}, {12, 49}};
                for (int x = 12; x <= 45; x += 3) {
                    placeBlock(level, DOT_DECO_DATA, x, 9, 45, cb);
                    placeBlock(level, DOT_DECO_DATA, x, 9, 52, cb);
                    if (x == 12 || x == 18 || x == 24 || x == 33 || x == 39 || x == 45) {
                        for (int[] e : extra) {
                            placeBlock(level, DOT_DECO_DATA, x, e[0], e[1], cb);
                        }
                    }
                }
                for (int i = 0; i < 3; i++) {
                    gray(level, cb, 8 + i, 5 + i, 54, 49 - i, 5 + i, 54);
                }
                light(level, cb, 11, 8, 54, 46, 8, 54);
                gray(level, cb, 14, 8, 44, 43, 8, 53);
            }
        }

        private void generateUpperWall(WorldGenLevel level, BoundingBox cb) {
            if (chunkIntersects(cb, 14, 21, 20, 43)) {
                gray(level, cb, 14, 0, 21, 20, 0, 43);
                generateWaterBox(level, cb, 14, 1, 22, 20, 14, 43);
                gray(level, cb, 18, 12, 22, 20, 12, 39);
                light(level, cb, 18, 12, 21, 20, 12, 21);
                for (int i = 0; i < 4; i++) {
                    light(level, cb, i + 14, i + 9, 21, i + 14, i + 9, 43 - i);
                }
                for (int z = 23; z <= 39; z += 3) {
                    placeBlock(level, DOT_DECO_DATA, 19, 13, z, cb);
                }
            }
            if (chunkIntersects(cb, 37, 21, 43, 43)) {
                gray(level, cb, 37, 0, 21, 43, 0, 43);
                generateWaterBox(level, cb, 37, 1, 22, 43, 14, 43);
                gray(level, cb, 37, 12, 22, 39, 12, 39);
                light(level, cb, 37, 12, 21, 39, 12, 21);
                for (int i = 0; i < 4; i++) {
                    light(level, cb, 43 - i, i + 9, 21, 43 - i, i + 9, 43 - i);
                }
                for (int z = 23; z <= 39; z += 3) {
                    placeBlock(level, DOT_DECO_DATA, 38, 13, z, cb);
                }
            }
            if (chunkIntersects(cb, 15, 37, 42, 43)) {
                gray(level, cb, 21, 0, 37, 36, 0, 43);
                generateWaterBox(level, cb, 21, 1, 37, 36, 14, 43);
                gray(level, cb, 21, 12, 37, 36, 12, 39);
                for (int i = 0; i < 4; i++) {
                    light(level, cb, 15 + i, i + 9, 43 - i, 42 - i, i + 9, 43 - i);
                }
                for (int x = 21; x <= 36; x += 3) {
                    placeBlock(level, DOT_DECO_DATA, x, 13, 38, cb);
                }
            }
        }
    }

    public static class OceanMonumentEntryRoom extends OceanMonumentPiece {
        public OceanMonumentEntryRoom(Direction orientation, RoomDefinition definition) {
            super(StructurePieceType.OCEAN_MONUMENT_ENTRY_ROOM, 1, orientation, definition, 1, 1, 1);
        }

        public OceanMonumentEntryRoom(CompoundTag tag) {
            super(StructurePieceType.OCEAN_MONUMENT_ENTRY_ROOM, tag);
        }

        public void postProcess(WorldGenLevel level, StructureManager structureManager, ChunkGenerator generator,
                RandomSource random, BoundingBox cb, ChunkPos chunkPos, BlockPos referencePos) {
            light(level, cb, 0, 3, 0, 2, 3, 7);
            light(level, cb, 5, 3, 0, 7, 3, 7);
            light(level, cb, 0, 2, 0, 1, 2, 7);
            light(level, cb, 6, 2, 0, 7, 2, 7);
            light(level, cb, 0, 1, 0, 0, 1, 7);
            light(level, cb, 7, 1, 0, 7, 1, 7);
            light(level, cb, 0, 1, 7, 7, 3, 7);
            light(level, cb, 1, 1, 0, 2, 3, 0);
            light(level, cb, 5, 1, 0, 6, 3, 0);
            if (open(roomDefinition, kNorth)) {
                generateWaterBox(level, cb, 3, 1, 7, 4, 2, 7);
            }
            if (open(roomDefinition, kWest)) {
                generateWaterBox(level, cb, 0, 1, 3, 1, 2, 4);
            }
            if (open(roomDefinition, kEast)) {
                generateWaterBox(level, cb, 6, 1, 3, 7, 2, 4);
            }
        }
    }

    public static class OceanMonumentSimpleRoom extends OceanMonumentPiece {
        private int mainDesign;

        public OceanMonumentSimpleRoom(Direction orientation, RoomDefinition definition, RandomSource random) {
            super(StructurePieceType.OCEAN_MONUMENT_SIMPLE_ROOM, 1, orientation, definition, 1, 1, 1);
            mainDesign = random.nextInt(3);
        }

        public OceanMonumentSimpleRoom(CompoundTag tag) {
            super(StructurePieceType.OCEAN_MONUMENT_SIMPLE_ROOM, tag);
        }

        public void postProcess(WorldGenLevel level, StructureManager structureManager, ChunkGenerator generator,
                RandomSource random, BoundingBox cb, ChunkPos chunkPos, BlockPos referencePos) {
            if (roomDefinition.index / 25 > 0) {
                generateDefaultFloor(level, cb, 0, 0, open(roomDefinition, kDown));
            }
            if (roomDefinition.connections[kUp] == null) {
                generateBoxOnFillOnly(level, cb, 1, 4, 1, 6, 4, 6, BASE_GRAY);
            }
            boolean centerPillar = mainDesign != 0 && random.nextBoolean() && !open(roomDefinition, kDown)
                && !open(roomDefinition, kUp) && roomDefinition.countOpenings() > 1;
            if (mainDesign == 0) {
                designCorners(level, cb);
            } else if (mainDesign == 1) {
                designPillars(level, cb);
            } else if (mainDesign == 2) {
                designFrame(level, cb);
            }
            if (centerPillar) {
                light(level, cb, 3, 1, 3, 4, 1, 4);
                gray(level, cb, 3, 2, 3, 4, 2, 4);
                light(level, cb, 3, 3, 3, 4, 3, 4);
            }
        }

        private void designCorners(WorldGenLevel level, BoundingBox cb) {
            light(level, cb, 0, 1, 0, 2, 1, 2);
            light(level, cb, 0, 3, 0, 2, 3, 2);
            gray(level, cb, 0, 2, 0, 0, 2, 2);
            gray(level, cb, 1, 2, 0, 2, 2, 0);
            placeBlock(level, LAMP_BLOCK, 1, 2, 1, cb);
            light(level, cb, 5, 1, 0, 7, 1, 2);
            light(level, cb, 5, 3, 0, 7, 3, 2);
            gray(level, cb, 7, 2, 0, 7, 2, 2);
            gray(level, cb, 5, 2, 0, 6, 2, 0);
            placeBlock(level, LAMP_BLOCK, 6, 2, 1, cb);
            light(level, cb, 0, 1, 5, 2, 1, 7);
            light(level, cb, 0, 3, 5, 2, 3, 7);
            gray(level, cb, 0, 2, 5, 0, 2, 7);
            gray(level, cb, 1, 2, 7, 2, 2, 7);
            placeBlock(level, LAMP_BLOCK, 1, 2, 6, cb);
            light(level, cb, 5, 1, 5, 7, 1, 7);
            light(level, cb, 5, 3, 5, 7, 3, 7);
            gray(level, cb, 7, 2, 5, 7, 2, 7);
            gray(level, cb, 5, 2, 7, 6, 2, 7);
            placeBlock(level, LAMP_BLOCK, 6, 2, 6, cb);
            if (open(roomDefinition, kSouth)) {
                light(level, cb, 3, 3, 0, 4, 3, 0);
            } else {
                light(level, cb, 3, 3, 0, 4, 3, 1);
                gray(level, cb, 3, 2, 0, 4, 2, 0);
                light(level, cb, 3, 1, 0, 4, 1, 1);
            }
            if (open(roomDefinition, kNorth)) {
                light(level, cb, 3, 3, 7, 4, 3, 7);
            } else {
                light(level, cb, 3, 3, 6, 4, 3, 7);
                gray(level, cb, 3, 2, 7, 4, 2, 7);
                light(level, cb, 3, 1, 6, 4, 1, 7);
            }
            if (open(roomDefinition, kWest)) {
                light(level, cb, 0, 3, 3, 0, 3, 4);
            } else {
                light(level, cb, 0, 3, 3, 1, 3, 4);
                gray(level, cb, 0, 2, 3, 0, 2, 4);
                light(level, cb, 0, 1, 3, 1, 1, 4);
            }
            if (open(roomDefinition, kEast)) {
                light(level, cb, 7, 3, 3, 7, 3, 4);
            } else {
                light(level, cb, 6, 3, 3, 7, 3, 4);
                gray(level, cb, 7, 2, 3, 7, 2, 4);
                light(level, cb, 6, 1, 3, 7, 1, 4);
            }
        }

        private void designPillars(WorldGenLevel level, BoundingBox cb) {
            light(level, cb, 2, 1, 2, 2, 3, 2);
            light(level, cb, 2, 1, 5, 2, 3, 5);
            light(level, cb, 5, 1, 5, 5, 3, 5);
            light(level, cb, 5, 1, 2, 5, 3, 2);
            placeBlock(level, LAMP_BLOCK, 2, 2, 2, cb);
            placeBlock(level, LAMP_BLOCK, 2, 2, 5, cb);
            placeBlock(level, LAMP_BLOCK, 5, 2, 5, cb);
            placeBlock(level, LAMP_BLOCK, 5, 2, 2, cb);
            light(level, cb, 0, 1, 0, 1, 3, 0);
            light(level, cb, 0, 1, 1, 0, 3, 1);
            light(level, cb, 0, 1, 7, 1, 3, 7);
            light(level, cb, 0, 1, 6, 0, 3, 6);
            light(level, cb, 6, 1, 7, 7, 3, 7);
            light(level, cb, 7, 1, 6, 7, 3, 6);
            light(level, cb, 6, 1, 0, 7, 3, 0);
            light(level, cb, 7, 1, 1, 7, 3, 1);
            int[][] grayPoints = {{1, 0}, {0, 1}, {1, 7}, {0, 6}, {6, 7}, {7, 6}, {6, 0}, {7, 1}};
            for (int[] p : grayPoints) {
                placeBlock(level, BASE_GRAY, p[0], 2, p[1], cb);
            }
            if (!open(roomDefinition, kSouth)) {
                wallStack(level, cb, 1, 0, 6, 0);
            }
            if (!open(roomDefinition, kNorth)) {
                wallStack(level, cb, 1, 7, 6, 7);
            }
            if (!open(roomDefinition, kWest)) {
                wallStack(level, cb, 0, 1, 0, 6);
            }
            if (!open(roomDefinition, kEast)) {
                wallStack(level, cb, 7, 1, 7, 6);
            }
        }

        private void wallStack(WorldGenLevel level, BoundingBox cb, int x0, int z0, int x1, int z1) {
            light(level, cb, x0, 3, z0, x1, 3, z1);
            gray(level, cb, x0, 2, z0, x1, 2, z1);
            light(level, cb, x0, 1, z0, x1, 1, z1);
        }

        private void designFrame(WorldGenLevel level, BoundingBox cb) {
            light(level, cb, 0, 1, 0, 0, 1, 7);
            light(level, cb, 7, 1, 0, 7, 1, 7);
            light(level, cb, 1, 1, 0, 6, 1, 0);
            light(level, cb, 1, 1, 7, 6, 1, 7);
            dark(level, cb, 0, 2, 0, 0, 2, 7);
            dark(level, cb, 7, 2, 0, 7, 2, 7);
            dark(level, cb, 1, 2, 0, 6, 2, 0);
            dark(level, cb, 1, 2, 7, 6, 2, 7);
            light(level, cb, 0, 3, 0, 0, 3, 7);
            light(level, cb, 7, 3, 0, 7, 3, 7);
            light(level, cb, 1, 3, 0, 6, 3, 0);
            light(level, cb, 1, 3, 7, 6, 3, 7);
            dark(level, cb, 0, 1, 3, 0, 2, 4);
            dark(level, cb, 7, 1, 3, 7, 2, 4);
            dark(level, cb, 3, 1, 0, 4, 2, 0);
            dark(level, cb, 3, 1, 7, 4, 2, 7);
            if (open(roomDefinition, kSouth)) {
                generateWaterBox(level, cb, 3, 1, 0, 4, 2, 0);
            }
            if (open(roomDefinition, kNorth)) {
                generateWaterBox(level, cb, 3, 1, 7, 4, 2, 7);
            }
            if (open(roomDefinition, kWest)) {
                generateWaterBox(level, cb, 0, 1, 3, 0, 2, 4);
            }
            if (open(roomDefinition, kEast)) {
                generateWaterBox(level, cb, 7, 1, 3, 7, 2, 4);
            }
        }
    }

    public static class OceanMonumentSimpleTopRoom extends OceanMonumentPiece {
        public OceanMonumentSimpleTopRoom(Direction orientation, RoomDefinition definition) {
            super(StructurePieceType.OCEAN_MONUMENT_SIMPLE_TOP_ROOM, 1, orientation, definition, 1, 1, 1);
        }

        public OceanMonumentSimpleTopRoom(CompoundTag tag) {
            super(StructurePieceType.OCEAN_MONUMENT_SIMPLE_TOP_ROOM, tag);
        }

        public void postProcess(WorldGenLevel level, StructureManager structureManager, ChunkGenerator generator,
                RandomSource random, BoundingBox cb, ChunkPos chunkPos, BlockPos referencePos) {
            if (roomDefinition.index / 25 > 0) {
                generateDefaultFloor(level, cb, 0, 0, open(roomDefinition, kDown));
            }
            if (roomDefinition.connections[kUp] == null) {
                generateBoxOnFillOnly(level, cb, 1, 4, 1, 6, 4, 6, BASE_GRAY);
            }
            BlockState sponge = Blocks.WET_SPONGE.defaultBlockState();
            for (int x = 1; x <= 6; x++) {
                for (int z = 1; z <= 6; z++) {
                    if (random.nextInt(3) != 0) {
                        int y0 = 2 + (random.nextInt(4) == 0 ? 0 : 1);
                        box(level, cb, sponge, x, y0, z, x, 3, z);
                    }
                }
            }
            light(level, cb, 0, 1, 0, 0, 1, 7);
            light(level, cb, 7, 1, 0, 7, 1, 7);
            light(level, cb, 1, 1, 0, 6, 1, 0);
            light(level, cb, 1, 1, 7, 6, 1, 7);
            dark(level, cb, 0, 2, 0, 0, 2, 7);
            dark(level, cb, 7, 2, 0, 7, 2, 7);
            dark(level, cb, 1, 2, 0, 6, 2, 0);
            dark(level, cb, 1, 2, 7, 6, 2, 7);
            light(level, cb, 0, 3, 0, 0, 3, 7);
            light(level, cb, 7, 3, 0, 7, 3, 7);
            light(level, cb, 1, 3, 0, 6, 3, 0);
            light(level, cb, 1, 3, 7, 6, 3, 7);
            dark(level, cb, 0, 1, 3, 0, 2, 4);
            dark(level, cb, 7, 1, 3, 7, 2, 4);
            dark(level, cb, 3, 1, 0, 4, 2, 0);
            dark(level, cb, 3, 1, 7, 4, 2, 7);
            if (open(roomDefinition, kSouth)) {
                generateWaterBox(level, cb, 3, 1, 0, 4, 2, 0);
            }
        }
    }

    public static class OceanMonumentDoubleYRoom extends OceanMonumentPiece {
        public OceanMonumentDoubleYRoom(Direction orientation, RoomDefinition definition) {
            super(StructurePieceType.OCEAN_MONUMENT_DOUBLE_Y_ROOM, 1, orientation, definition, 1, 2, 1);
        }

        public OceanMonumentDoubleYRoom(CompoundTag tag) {
            super(StructurePieceType.OCEAN_MONUMENT_DOUBLE_Y_ROOM, tag);
        }

        public void postProcess(WorldGenLevel level, StructureManager structureManager, ChunkGenerator generator,
                RandomSource random, BoundingBox cb, ChunkPos chunkPos, BlockPos referencePos) {
            if (roomDefinition.index / 25 > 0) {
                generateDefaultFloor(level, cb, 0, 0, open(roomDefinition, kDown));
            }
            RoomDefinition above = roomDefinition.connections[kUp];
            if (above.connections[kUp] == null) {
                generateBoxOnFillOnly(level, cb, 1, 8, 1, 6, 8, 6, BASE_GRAY);
            }
            light(level, cb, 0, 4, 0, 0, 4, 7);
            light(level, cb, 7, 4, 0, 7, 4, 7);
            light(level, cb, 1, 4, 0, 6, 4, 0);
            light(level, cb, 1, 4, 7, 6, 4, 7);
            light(level, cb, 2, 4, 1, 2, 4, 2);
            light(level, cb, 1, 4, 2, 1, 4, 2);
            light(level, cb, 5, 4, 1, 5, 4, 2);
            light(level, cb, 6, 4, 2, 6, 4, 2);
            light(level, cb, 2, 4, 5, 2, 4, 6);
            light(level, cb, 1, 4, 5, 1, 4, 5);
            light(level, cb, 5, 4, 5, 5, 4, 6);
            light(level, cb, 6, 4, 5, 6, 4, 5);
            RoomDefinition definition = roomDefinition;
            for (int y = 1; y <= 5; y += 4) {
                for (int side = 0; side < 2; side++) {
                    int z = side == 0 ? 0 : 7;
                    if (open(definition, side == 0 ? kSouth : kNorth)) {
                        light(level, cb, 2, y, z, 2, y + 2, z);
                        light(level, cb, 5, y, z, 5, y + 2, z);
                        light(level, cb, 3, y + 2, z, 4, y + 2, z);
                    } else {
                        light(level, cb, 0, y, z, 7, y + 2, z);
                        gray(level, cb, 0, y + 1, z, 7, y + 1, z);
                    }
                }
                for (int side = 0; side < 2; side++) {
                    int x = side == 0 ? 0 : 7;
                    if (open(definition, side == 0 ? kWest : kEast)) {
                        light(level, cb, x, y, 2, x, y + 2, 2);
                        light(level, cb, x, y, 5, x, y + 2, 5);
                        light(level, cb, x, y + 2, 3, x, y + 2, 4);
                    } else {
                        light(level, cb, x, y, 0, x, y + 2, 7);
                        gray(level, cb, x, y + 1, 0, x, y + 1, 7);
                    }
                }
                definition = above;
            }
        }
    }

    public static class OceanMonumentDoubleXRoom extends OceanMonumentPiece {
        public OceanMonumentDoubleXRoom(Direction orientation, RoomDefinition definition) {
            super(StructurePieceType.OCEAN_MONUMENT_DOUBLE_X_ROOM, 1, orientation, definition, 2, 1, 1);
        }

        public OceanMonumentDoubleXRoom(CompoundTag tag) {
            super(StructurePieceType.OCEAN_MONUMENT_DOUBLE_X_ROOM, tag);
        }

        public void postProcess(WorldGenLevel level, StructureManager structureManager, ChunkGenerator generator,
                RandomSource random, BoundingBox cb, ChunkPos chunkPos, BlockPos referencePos) {
            RoomDefinition east = roomDefinition.connections[kEast];
            RoomDefinition west = roomDefinition;
            if (roomDefinition.index / 25 > 0) {
                generateDefaultFloor(level, cb, 8, 0, open(east, kDown));
                generateDefaultFloor(level, cb, 0, 0, open(west, kDown));
            }
            if (west.connections[kUp] == null) {
                generateBoxOnFillOnly(level, cb, 1, 4, 1, 7, 4, 6, BASE_GRAY);
            }
            if (east.connections[kUp] == null) {
                generateBoxOnFillOnly(level, cb, 8, 4, 1, 14, 4, 6, BASE_GRAY);
            }
            light(level, cb, 0, 3, 0, 0, 3, 7);
            light(level, cb, 15, 3, 0, 15, 3, 7);
            light(level, cb, 1, 3, 0, 15, 3, 0);
            light(level, cb, 1, 3, 7, 14, 3, 7);
            gray(level, cb, 0, 2, 0, 0, 2, 7);
            gray(level, cb, 15, 2, 0, 15, 2, 7);
            gray(level, cb, 1, 2, 0, 15, 2, 0);
            gray(level, cb, 1, 2, 7, 14, 2, 7);
            light(level, cb, 0, 1, 0, 0, 1, 7);
            light(level, cb, 15, 1, 0, 15, 1, 7);
            light(level, cb, 1, 1, 0, 15, 1, 0);
            light(level, cb, 1, 1, 7, 14, 1, 7);
            light(level, cb, 5, 1, 0, 10, 1, 4);
            gray(level, cb, 6, 2, 0, 9, 2, 3);
            light(level, cb, 5, 3, 0, 10, 3, 4);
            placeBlock(level, LAMP_BLOCK, 6, 2, 3, cb);
            placeBlock(level, LAMP_BLOCK, 9, 2, 3, cb);
            if (open(west, kSouth)) {
                generateWaterBox(level, cb, 3, 1, 0, 4, 2, 0);
            }
            if (open(west, kNorth)) {
                generateWaterBox(level, cb, 3, 1, 7, 4, 2, 7);
            }
            if (open(west, kWest)) {
                generateWaterBox(level, cb, 0, 1, 3, 0, 2, 4);
            }
            if (open(east, kSouth)) {
                generateWaterBox(level, cb, 11, 1, 0, 12, 2, 0);
            }
            if (open(east, kNorth)) {
                generateWaterBox(level, cb, 11, 1, 7, 12, 2, 7);
            }
            if (open(east, kEast)) {
                generateWaterBox(level, cb, 15, 1, 3, 15, 2, 4);
            }
        }
    }

    public static class OceanMonumentDoubleZRoom extends OceanMonumentPiece {
        public OceanMonumentDoubleZRoom(Direction orientation, RoomDefinition definition) {
            super(StructurePieceType.OCEAN_MONUMENT_DOUBLE_Z_ROOM, 1, orientation, definition, 1, 1, 2);
        }

        public OceanMonumentDoubleZRoom(CompoundTag tag) {
            super(StructurePieceType.OCEAN_MONUMENT_DOUBLE_Z_ROOM, tag);
        }

        public void postProcess(WorldGenLevel level, StructureManager structureManager, ChunkGenerator generator,
                RandomSource random, BoundingBox cb, ChunkPos chunkPos, BlockPos referencePos) {
            RoomDefinition north = roomDefinition.connections[kNorth];
            RoomDefinition south = roomDefinition;
            if (roomDefinition.index / 25 > 0) {
                generateDefaultFloor(level, cb, 0, 8, open(north, kDown));
                generateDefaultFloor(level, cb, 0, 0, open(south, kDown));
            }
            if (south.connections[kUp] == null) {
                generateBoxOnFillOnly(level, cb, 1, 4, 1, 6, 4, 7, BASE_GRAY);
            }
            if (north.connections[kUp] == null) {
                generateBoxOnFillOnly(level, cb, 1, 4, 8, 6, 4, 14, BASE_GRAY);
            }
            light(level, cb, 0, 3, 0, 0, 3, 15);
            light(level, cb, 7, 3, 0, 7, 3, 15);
            light(level, cb, 1, 3, 0, 7, 3, 0);
            light(level, cb, 1, 3, 15, 6, 3, 15);
            gray(level, cb, 0, 2, 0, 0, 2, 15);
            gray(level, cb, 7, 2, 0, 7, 2, 15);
            gray(level, cb, 1, 2, 0, 7, 2, 0);
            gray(level, cb, 1, 2, 15, 6, 2, 15);
            light(level, cb, 0, 1, 0, 0, 1, 15);
            light(level, cb, 7, 1, 0, 7, 1, 15);
            light(level, cb, 1, 1, 0, 7, 1, 0);
            light(level, cb, 1, 1, 15, 6, 1, 15);
            light(level, cb, 1, 1, 1, 1, 1, 2);
            light(level, cb, 6, 1, 1, 6, 1, 2);
            light(level, cb, 1, 3, 1, 1, 3, 2);
            light(level, cb, 6, 3, 1, 6, 3, 2);
            light(level, cb, 1, 1, 13, 1, 1, 14);
            light(level, cb, 6, 1, 13, 6, 1, 14);
            light(level, cb, 1, 3, 13, 1, 3, 14);
            light(level, cb, 6, 3, 13, 6, 3, 14);
            light(level, cb, 2, 1, 6, 2, 3, 6);
            light(level, cb, 5, 1, 6, 5, 3, 6);
            light(level, cb, 2, 1, 9, 2, 3, 9);
            light(level, cb, 5, 1, 9, 5, 3, 9);
            light(level, cb, 3, 2, 6, 4, 2, 6);
            light(level, cb, 3, 2, 9, 4, 2, 9);
            light(level, cb, 2, 2, 7, 2, 2, 8);
            light(level, cb, 5, 2, 7, 5, 2, 8);
            placeBlock(level, LAMP_BLOCK, 2, 2, 5, cb);
            placeBlock(level, LAMP_BLOCK, 5, 2, 5, cb);
            placeBlock(level, LAMP_BLOCK, 2, 2, 10, cb);
            placeBlock(level, LAMP_BLOCK, 5, 2, 10, cb);
            placeBlock(level, BASE_LIGHT, 2, 3, 5, cb);
            placeBlock(level, BASE_LIGHT, 5, 3, 5, cb);
            placeBlock(level, BASE_LIGHT, 2, 3, 10, cb);
            placeBlock(level, BASE_LIGHT, 5, 3, 10, cb);
            if (open(south, kSouth)) {
                generateWaterBox(level, cb, 3, 1, 0, 4, 2, 0);
            }
            if (open(south, kEast)) {
                generateWaterBox(level, cb, 7, 1, 3, 7, 2, 4);
            }
            if (open(south, kWest)) {
                generateWaterBox(level, cb, 0, 1, 3, 0, 2, 4);
            }
            if (open(north, kNorth)) {
                generateWaterBox(level, cb, 3, 1, 15, 4, 2, 15);
            }
            if (open(north, kWest)) {
                generateWaterBox(level, cb, 0, 1, 11, 0, 2, 12);
            }
            if (open(north, kEast)) {
                generateWaterBox(level, cb, 7, 1, 11, 7, 2, 12);
            }
        }
    }

    public static class OceanMonumentDoubleXYRoom extends OceanMonumentPiece {
        public OceanMonumentDoubleXYRoom(Direction orientation, RoomDefinition definition) {
            super(StructurePieceType.OCEAN_MONUMENT_DOUBLE_XY_ROOM, 1, orientation, definition, 2, 2, 1);
        }

        public OceanMonumentDoubleXYRoom(CompoundTag tag) {
            super(StructurePieceType.OCEAN_MONUMENT_DOUBLE_XY_ROOM, tag);
        }

        public void postProcess(WorldGenLevel level, StructureManager structureManager, ChunkGenerator generator,
                RandomSource random, BoundingBox cb, ChunkPos chunkPos, BlockPos referencePos) {
            RoomDefinition east = roomDefinition.connections[kEast];
            RoomDefinition west = roomDefinition;
            RoomDefinition westUp = west.connections[kUp];
            RoomDefinition eastUp = east.connections[kUp];
            if (roomDefinition.index / 25 > 0) {
                generateDefaultFloor(level, cb, 8, 0, open(east, kDown));
                generateDefaultFloor(level, cb, 0, 0, open(west, kDown));
            }
            if (westUp.connections[kUp] == null) {
                generateBoxOnFillOnly(level, cb, 1, 8, 1, 7, 8, 6, BASE_GRAY);
            }
            if (eastUp.connections[kUp] == null) {
                generateBoxOnFillOnly(level, cb, 8, 8, 1, 14, 8, 6, BASE_GRAY);
            }
            for (int y = 1; y <= 7; y++) {
                BlockState state = y == 2 || y == 6 ? BASE_GRAY : BASE_LIGHT;
                box(level, cb, state, 0, y, 0, 0, y, 7);
                box(level, cb, state, 15, y, 0, 15, y, 7);
                box(level, cb, state, 1, y, 0, 15, y, 0);
                box(level, cb, state, 1, y, 7, 14, y, 7);
            }
            light(level, cb, 2, 1, 3, 2, 7, 4);
            light(level, cb, 3, 1, 2, 4, 7, 2);
            light(level, cb, 3, 1, 5, 4, 7, 5);
            light(level, cb, 13, 1, 3, 13, 7, 4);
            light(level, cb, 11, 1, 2, 12, 7, 2);
            light(level, cb, 11, 1, 5, 12, 7, 5);
            light(level, cb, 5, 1, 3, 5, 3, 4);
            light(level, cb, 10, 1, 3, 10, 3, 4);
            light(level, cb, 5, 7, 2, 10, 7, 5);
            light(level, cb, 5, 5, 2, 5, 7, 2);
            light(level, cb, 10, 5, 2, 10, 7, 2);
            light(level, cb, 5, 5, 5, 5, 7, 5);
            light(level, cb, 10, 5, 5, 10, 7, 5);
            placeBlock(level, BASE_LIGHT, 6, 6, 2, cb);
            placeBlock(level, BASE_LIGHT, 9, 6, 2, cb);
            placeBlock(level, BASE_LIGHT, 6, 6, 5, cb);
            placeBlock(level, BASE_LIGHT, 9, 6, 5, cb);
            light(level, cb, 5, 4, 3, 6, 4, 4);
            light(level, cb, 9, 4, 3, 10, 4, 4);
            placeBlock(level, LAMP_BLOCK, 5, 4, 2, cb);
            placeBlock(level, LAMP_BLOCK, 5, 4, 5, cb);
            placeBlock(level, LAMP_BLOCK, 10, 4, 2, cb);
            placeBlock(level, LAMP_BLOCK, 10, 4, 5, cb);
            if (open(west, kSouth)) {
                generateWaterBox(level, cb, 3, 1, 0, 4, 2, 0);
            }
            if (open(west, kNorth)) {
                generateWaterBox(level, cb, 3, 1, 7, 4, 2, 7);
            }
            if (open(west, kWest)) {
                generateWaterBox(level, cb, 0, 1, 3, 0, 2, 4);
            }
            if (open(east, kSouth)) {
                generateWaterBox(level, cb, 11, 1, 0, 12, 2, 0);
            }
            if (open(east, kNorth)) {
                generateWaterBox(level, cb, 11, 1, 7, 12, 2, 7);
            }
            if (open(east, kEast)) {
                generateWaterBox(level, cb, 15, 1, 3, 15, 2, 4);
            }
            if (open(westUp, kSouth)) {
                generateWaterBox(level, cb, 3, 5, 0, 4, 6, 0);
            }
            if (open(westUp, kNorth)) {
                generateWaterBox(level, cb, 3, 5, 7, 4, 6, 7);
            }
            if (open(westUp, kWest)) {
                generateWaterBox(level, cb, 0, 5, 3, 0, 6, 4);
            }
            if (open(eastUp, kSouth)) {
                generateWaterBox(level, cb, 11, 5, 0, 12, 6, 0);
            }
            if (open(eastUp, kNorth)) {
                generateWaterBox(level, cb, 11, 5, 7, 12, 6, 7);
            }
            if (open(eastUp, kEast)) {
                generateWaterBox(level, cb, 15, 5, 3, 15, 6, 4);
            }
        }
    }

    public static class OceanMonumentDoubleYZRoom extends OceanMonumentPiece {
        public OceanMonumentDoubleYZRoom(Direction orientation, RoomDefinition definition) {
            super(StructurePieceType.OCEAN_MONUMENT_DOUBLE_YZ_ROOM, 1, orientation, definition, 1, 2, 2);
        }

        public OceanMonumentDoubleYZRoom(CompoundTag tag) {
            super(StructurePieceType.OCEAN_MONUMENT_DOUBLE_YZ_ROOM, tag);
        }

        public void postProcess(WorldGenLevel level, StructureManager structureManager, ChunkGenerator generator,
                RandomSource random, BoundingBox cb, ChunkPos chunkPos, BlockPos referencePos) {
            RoomDefinition north = roomDefinition.connections[kNorth];
            RoomDefinition south = roomDefinition;
            RoomDefinition northUp = north.connections[kUp];
            RoomDefinition southUp = south.connections[kUp];
            if (roomDefinition.index / 25 > 0) {
                generateDefaultFloor(level, cb, 0, 8, open(north, kDown));
                generateDefaultFloor(level, cb, 0, 0, open(south, kDown));
            }
            if (southUp.connections[kUp] == null) {
                generateBoxOnFillOnly(level, cb, 1, 8, 1, 6, 8, 7, BASE_GRAY);
            }
            if (northUp.connections[kUp] == null) {
                generateBoxOnFillOnly(level, cb, 1, 8, 8, 6, 8, 14, BASE_GRAY);
            }
            for (int y = 1; y <= 7; y++) {
                BlockState state = y == 2 || y == 6 ? BASE_GRAY : BASE_LIGHT;
                box(level, cb, state, 0, y, 0, 0, y, 15);
                box(level, cb, state, 7, y, 0, 7, y, 15);
                box(level, cb, state, 1, y, 0, 6, y, 0);
                box(level, cb, state, 1, y, 15, 6, y, 15);
            }
            for (int y = 1; y <= 7; y++) {
                box(level, cb, y == 2 || y == 6 ? LAMP_BLOCK : BASE_BLACK, 3, y, 7, 4, y, 8);
            }
            if (open(south, kSouth)) {
                generateWaterBox(level, cb, 3, 1, 0, 4, 2, 0);
            }
            if (open(south, kEast)) {
                generateWaterBox(level, cb, 7, 1, 3, 7, 2, 4);
            }
            if (open(south, kWest)) {
                generateWaterBox(level, cb, 0, 1, 3, 0, 2, 4);
            }
            if (open(north, kNorth)) {
                generateWaterBox(level, cb, 3, 1, 15, 4, 2, 15);
            }
            if (open(north, kWest)) {
                generateWaterBox(level, cb, 0, 1, 11, 0, 2, 12);
            }
            if (open(north, kEast)) {
                generateWaterBox(level, cb, 7, 1, 11, 7, 2, 12);
            }
            if (open(southUp, kSouth)) {
                generateWaterBox(level, cb, 3, 5, 0, 4, 6, 0);
            }
            if (open(southUp, kEast)) {
                generateWaterBox(level, cb, 7, 5, 3, 7, 6, 4);
                light(level, cb, 5, 4, 2, 6, 4, 5);
                light(level, cb, 6, 1, 2, 6, 3, 2);
                light(level, cb, 6, 1, 5, 6, 3, 5);
            }
            if (open(southUp, kWest)) {
                generateWaterBox(level, cb, 0, 5, 3, 0, 6, 4);
                light(level, cb, 1, 4, 2, 2, 4, 5);
                light(level, cb, 1, 1, 2, 1, 3, 2);
                light(level, cb, 1, 1, 5, 1, 3, 5);
            }
            if (open(northUp, kNorth)) {
                generateWaterBox(level, cb, 3, 5, 15, 4, 6, 15);
            }
            if (open(northUp, kWest)) {
                generateWaterBox(level, cb, 0, 5, 11, 0, 6, 12);
                light(level, cb, 1, 4, 10, 2, 4, 13);
                light(level, cb, 1, 1, 10, 1, 3, 10);
                light(level, cb, 1, 1, 13, 1, 3, 13);
            }
            if (open(northUp, kEast)) {
                generateWaterBox(level, cb, 7, 5, 11, 7, 6, 12);
                light(level, cb, 5, 4, 10, 6, 4, 13);
                light(level, cb, 6, 1, 10, 6, 3, 10);
                light(level, cb, 6, 1, 13, 6, 3, 13);
            }
        }
    }

    public static class OceanMonumentCoreRoom extends OceanMonumentPiece {
        public OceanMonumentCoreRoom(Direction orientation, RoomDefinition definition) {
            super(StructurePieceType.OCEAN_MONUMENT_CORE_ROOM, 1, orientation, definition, 2, 2, 2);
        }

        public OceanMonumentCoreRoom(CompoundTag tag) {
            super(StructurePieceType.OCEAN_MONUMENT_CORE_ROOM, tag);
        }

        public void postProcess(WorldGenLevel level, StructureManager structureManager, ChunkGenerator generator,
                RandomSource random, BoundingBox cb, ChunkPos chunkPos, BlockPos referencePos) {
            generateBoxOnFillOnly(level, cb, 1, 8, 0, 14, 8, 14, BASE_GRAY);
            light(level, cb, 0, 7, 0, 0, 7, 15);
            light(level, cb, 15, 7, 0, 15, 7, 15);
            light(level, cb, 1, 7, 0, 15, 7, 0);
            light(level, cb, 1, 7, 15, 14, 7, 15);
            for (int y = 1; y <= 6; y++) {
                BlockState state = y == 2 || y == 6 ? BASE_GRAY : BASE_LIGHT;
                for (int x = 0; x <= 15; x += 15) {
                    box(level, cb, state, x, y, 0, x, y, 1);
                    box(level, cb, state, x, y, 6, x, y, 9);
                    box(level, cb, state, x, y, 14, x, y, 15);
                }
                box(level, cb, state, 1, y, 0, 1, y, 0);
                box(level, cb, state, 6, y, 0, 9, y, 0);
                box(level, cb, state, 14, y, 0, 14, y, 0);
                box(level, cb, state, 1, y, 15, 14, y, 15);
            }
            dark(level, cb, 6, 3, 6, 9, 6, 9);
            box(level, cb, Blocks.GOLD_BLOCK.defaultBlockState(), 7, 4, 7, 8, 5, 8);
            for (int y = 3; y <= 6; y += 3) {
                for (int x = 6; x <= 9; x += 3) {
                    placeBlock(level, LAMP_BLOCK, x, y, 6, cb);
                    placeBlock(level, LAMP_BLOCK, x, y, 9, cb);
                }
            }
            light(level, cb, 5, 1, 6, 5, 2, 6);
            light(level, cb, 5, 1, 9, 5, 2, 9);
            light(level, cb, 10, 1, 6, 10, 2, 6);
            light(level, cb, 10, 1, 9, 10, 2, 9);
            light(level, cb, 6, 1, 5, 6, 2, 5);
            light(level, cb, 9, 1, 5, 9, 2, 5);
            light(level, cb, 6, 1, 10, 6, 2, 10);
            light(level, cb, 9, 1, 10, 9, 2, 10);
            light(level, cb, 5, 2, 5, 5, 6, 5);
            light(level, cb, 5, 2, 10, 5, 6, 10);
            light(level, cb, 10, 2, 5, 10, 6, 5);
            light(level, cb, 10, 2, 10, 10, 6, 10);
            light(level, cb, 5, 7, 1, 5, 7, 6);
            light(level, cb, 10, 7, 1, 10, 7, 6);
            light(level, cb, 5, 7, 9, 5, 7, 14);
            light(level, cb, 10, 7, 9, 10, 7, 14);
            light(level, cb, 1, 7, 5, 6, 7, 5);
            light(level, cb, 1, 7, 10, 6, 7, 10);
            light(level, cb, 9, 7, 5, 14, 7, 5);
            light(level, cb, 9, 7, 10, 14, 7, 10);
            light(level, cb, 2, 1, 2, 2, 1, 3);
            light(level, cb, 3, 1, 2, 3, 1, 2);
            light(level, cb, 13, 1, 2, 13, 1, 3);
            light(level, cb, 12, 1, 2, 12, 1, 2);
            light(level, cb, 2, 1, 12, 2, 1, 13);
            light(level, cb, 3, 1, 13, 3, 1, 13);
            light(level, cb, 13, 1, 12, 13, 1, 13);
            light(level, cb, 12, 1, 13, 12, 1, 13);
        }
    }

    public static class OceanMonumentWingRoom extends OceanMonumentPiece {
        private int mainDesign;

        public OceanMonumentWingRoom(Direction orientation, BoundingBox boundingBox, int randomValue) {
            super(StructurePieceType.OCEAN_MONUMENT_WING_ROOM, orientation, 1, boundingBox);
            mainDesign = randomValue & 1;
        }

        public OceanMonumentWingRoom(CompoundTag tag) {
            super(StructurePieceType.OCEAN_MONUMENT_WING_ROOM, tag);
        }

        public void postProcess(WorldGenLevel level, StructureManager structureManager, ChunkGenerator generator,
                RandomSource random, BoundingBox cb, ChunkPos chunkPos, BlockPos referencePos) {
            if (mainDesign == 0) {
                for (int i = 0; i < 4; i++) {
                    light(level, cb, 10 - i, 3 - i, 20 - i, 12 + i, 3 - i, 20);
                }
                light(level, cb, 7, 0, 6, 15, 0, 16);
                light(level, cb, 6, 0, 6, 6, 3, 20);
                light(level, cb, 16, 0, 6, 16, 3, 20);
                light(level, cb, 7, 1, 7, 7, 1, 20);
                light(level, cb, 15, 1, 7, 15, 1, 20);
                light(level, cb, 7, 1, 6, 9, 3, 6);
                light(level, cb, 13, 1, 6, 15, 3, 6);
                light(level, cb, 8, 1, 7, 9, 1, 7);
                light(level, cb, 13, 1, 7, 14, 1, 7);
                light(level, cb, 9, 0, 5, 13, 0, 5);
                dark(level, cb, 10, 0, 7, 12, 0, 7);
                dark(level, cb, 8, 0, 10, 8, 0, 12);
                dark(level, cb, 14, 0, 10, 14, 0, 12);
                for (int z = 18; z >= 7; z -= 3) {
                    placeBlock(level, LAMP_BLOCK, 6, 3, z, cb);
                    placeBlock(level, LAMP_BLOCK, 16, 3, z, cb);
                }
                placeBlock(level, LAMP_BLOCK, 10, 0, 10, cb);
                placeBlock(level, LAMP_BLOCK, 12, 0, 10, cb);
                placeBlock(level, LAMP_BLOCK, 10, 0, 12, cb);
                placeBlock(level, LAMP_BLOCK, 12, 0, 12, cb);
                placeBlock(level, LAMP_BLOCK, 8, 3, 6, cb);
                placeBlock(level, LAMP_BLOCK, 14, 3, 6, cb);
                for (int px : new int[] {4, 18}) {
                    for (int pz : new int[] {4, 18}) {
                        placeBlock(level, BASE_LIGHT, px, 2, pz, cb);
                        placeBlock(level, LAMP_BLOCK, px, 1, pz, cb);
                        placeBlock(level, BASE_LIGHT, px, 0, pz, cb);
                    }
                }
                placeBlock(level, BASE_LIGHT, 9, 7, 20, cb);
                placeBlock(level, BASE_LIGHT, 13, 7, 20, cb);
                light(level, cb, 6, 0, 21, 7, 4, 21);
                light(level, cb, 15, 0, 21, 16, 4, 21);
                spawnElder(level, cb, 11, 2, 16);
            } else if (mainDesign == 1) {
                light(level, cb, 9, 3, 18, 13, 3, 20);
                light(level, cb, 9, 0, 18, 9, 2, 18);
                light(level, cb, 13, 0, 18, 13, 2, 18);
                for (int x : new int[] {9, 13}) {
                    placeBlock(level, BASE_LIGHT, x, 6, 20, cb);
                    placeBlock(level, LAMP_BLOCK, x, 5, 20, cb);
                    placeBlock(level, BASE_LIGHT, x, 4, 20, cb);
                }
                light(level, cb, 7, 3, 7, 15, 3, 14);
                for (int x : new int[] {10, 12}) {
                    light(level, cb, x, 0, 10, x, 6, 10);
                    light(level, cb, x, 0, 12, x, 6, 12);
                    placeBlock(level, LAMP_BLOCK, x, 0, 10, cb);
                    placeBlock(level, LAMP_BLOCK, x, 0, 12, cb);
                    placeBlock(level, LAMP_BLOCK, x, 4, 10, cb);
                    placeBlock(level, LAMP_BLOCK, x, 4, 12, cb);
                }
                for (int x : new int[] {8, 14}) {
                    light(level, cb, x, 0, 7, x, 2, 7);
                    light(level, cb, x, 0, 14, x, 2, 14);
                }
                dark(level, cb, 8, 3, 8, 8, 3, 13);
                dark(level, cb, 14, 3, 8, 14, 3, 13);
                spawnElder(level, cb, 11, 5, 13);
            }
        }
    }

    public static class OceanMonumentPenthouse extends OceanMonumentPiece {
        public OceanMonumentPenthouse(Direction orientation, BoundingBox boundingBox) {
            super(StructurePieceType.OCEAN_MONUMENT_PENTHOUSE, orientation, 1, boundingBox);
        }

        public OceanMonumentPenthouse(CompoundTag tag) {
            super(StructurePieceType.OCEAN_MONUMENT_PENTHOUSE, tag);
        }

        public void postProcess(WorldGenLevel level, StructureManager structureManager, ChunkGenerator generator,
                RandomSource random, BoundingBox cb, ChunkPos chunkPos, BlockPos referencePos) {
            light(level, cb, 2, -1, 2, 11, -1, 11);
            gray(level, cb, 0, -1, 0, 1, -1, 11);
            gray(level, cb, 12, -1, 0, 13, -1, 11);
            gray(level, cb, 2, -1, 0, 11, -1, 1);
            gray(level, cb, 2, -1, 12, 11, -1, 13);
            light(level, cb, 0, 0, 0, 0, 0, 13);
            light(level, cb, 13, 0, 0, 13, 0, 13);
            light(level, cb, 1, 0, 0, 12, 0, 0);
            light(level, cb, 1, 0, 13, 12, 0, 13);
            for (int i = 2; i <= 11; i += 3) {
                placeBlock(level, LAMP_BLOCK, 0, 0, i, cb);
                placeBlock(level, LAMP_BLOCK, 13, 0, i, cb);
                placeBlock(level, LAMP_BLOCK, i, 0, 0, cb);
            }
            light(level, cb, 2, 0, 3, 4, 0, 9);
            light(level, cb, 9, 0, 3, 11, 0, 9);
            light(level, cb, 4, 0, 9, 9, 0, 11);
            placeBlock(level, BASE_LIGHT, 5, 0, 8, cb);
            placeBlock(level, BASE_LIGHT, 8, 0, 8, cb);
            placeBlock(level, BASE_LIGHT, 10, 0, 10, cb);
            placeBlock(level, BASE_LIGHT, 3, 0, 10, cb);
            dark(level, cb, 3, 0, 3, 3, 0, 7);
            dark(level, cb, 10, 0, 3, 10, 0, 7);
            dark(level, cb, 6, 0, 10, 7, 0, 10);
            for (int x : new int[] {3, 10}) {
                for (int z = 2; z <= 8; z += 3) {
                    light(level, cb, x, 0, z, x, 2, z);
                }
            }
            light(level, cb, 5, 0, 10, 5, 2, 10);
            light(level, cb, 8, 0, 10, 8, 2, 10);
            dark(level, cb, 6, -1, 7, 7, -1, 8);
            generateWaterBox(level, cb, 6, -1, 3, 7, -1, 4);
            spawnElder(level, cb, 6, 1, 6);
        }
    }

    static class RoomDefinition {
        int index;
        final RoomDefinition[] connections = new RoomDefinition[6];
        final boolean[] hasOpening = new boolean[6];
        boolean claimed;
        boolean isSource;
        int scanIndex;

        public RoomDefinition(int roomIndex) {
            index = roomIndex;
        }

        public void setConnection(Direction direction, RoomDefinition definition) {
            connections[direction.get3DDataValue()] = definition;
            definition.connections[direction.getOpposite().get3DDataValue()] = this;
        }

        public void updateOpenings() {
            for (int i = 0; i < 6; i++) {
                hasOpening[i] = connections[i] != null;
            }
        }

        public boolean findSource(int scan) {
            if (isSource) {
                return true;
            }
            scanIndex = scan;
            for (int i = 0; i < 6; i++) {
                RoomDefinition next = connections[i];
                if (next != null && hasOpening[i] && next.scanIndex != scan && next.findSource(scan)) {
                    return true;
                }
            }
            return false;
        }

        public boolean isSpecial() {
            return index >= 75;
        }

        public int countOpenings() {
            int count = 0;
            for (boolean opening : hasOpening) {
                if (opening) {
                    count++;
                }
            }
            return count;
        }
    }

    private interface MonumentRoomFitter {
        boolean fits(RoomDefinition definition);

        OceanMonumentPiece create(Direction orientation, RoomDefinition definition, RandomSource random);
    }

    private static boolean free(RoomDefinition definition, int direction) {
        return definition.hasOpening[direction] && !definition.connections[direction].claimed;
    }

    private static class FitSimpleRoom implements MonumentRoomFitter {
        public boolean fits(RoomDefinition definition) {
            return true;
        }

        public OceanMonumentPiece create(Direction orientation, RoomDefinition definition, RandomSource random) {
            definition.claimed = true;
            return new OceanMonumentSimpleRoom(orientation, definition, random);
        }
    }

    private static class FitSimpleTopRoom implements MonumentRoomFitter {
        public boolean fits(RoomDefinition definition) {
            return !definition.hasOpening[kWest] && !definition.hasOpening[kEast]
                && !definition.hasOpening[kNorth] && !definition.hasOpening[kSouth] && !definition.hasOpening[kUp];
        }

        public OceanMonumentPiece create(Direction orientation, RoomDefinition definition, RandomSource random) {
            definition.claimed = true;
            return new OceanMonumentSimpleTopRoom(orientation, definition);
        }
    }

    private static class FitDoubleYRoom implements MonumentRoomFitter {
        public boolean fits(RoomDefinition definition) {
            return free(definition, kUp);
        }

        public OceanMonumentPiece create(Direction orientation, RoomDefinition definition, RandomSource random) {
            definition.claimed = true;
            definition.connections[kUp].claimed = true;
            return new OceanMonumentDoubleYRoom(orientation, definition);
        }
    }

    private static class FitDoubleXRoom implements MonumentRoomFitter {
        public boolean fits(RoomDefinition definition) {
            return free(definition, kEast);
        }

        public OceanMonumentPiece create(Direction orientation, RoomDefinition definition, RandomSource random) {
            definition.claimed = true;
            definition.connections[kEast].claimed = true;
            return new OceanMonumentDoubleXRoom(orientation, definition);
        }
    }

    private static class FitDoubleZRoom implements MonumentRoomFitter {
        public boolean fits(RoomDefinition definition) {
            return free(definition, kNorth);
        }

        public OceanMonumentPiece create(Direction orientation, RoomDefinition definition, RandomSource random) {
            RoomDefinition source = definition;
            if (!free(definition, kNorth)) {
                source = definition.connections[kSouth];
            }
            source.claimed = true;
            source.connections[kNorth].claimed = true;
            return new OceanMonumentDoubleZRoom(orientation, source);
        }
    }

    private static class FitDoubleXYRoom implements MonumentRoomFitter {
        public boolean fits(RoomDefinition definition) {
            if (!free(definition, kEast) || !free(definition, kUp)) {
                return false;
            }
            return free(definition.connections[kEast], kUp);
        }

        public OceanMonumentPiece create(Direction orientation, RoomDefinition definition, RandomSource random) {
            RoomDefinition east = definition.connections[kEast];
            definition.claimed = true;
            east.claimed = true;
            definition.connections[kUp].claimed = true;
            east.connections[kUp].claimed = true;
            return new OceanMonumentDoubleXYRoom(orientation, definition);
        }
    }

    private static class FitDoubleYZRoom implements MonumentRoomFitter {
        public boolean fits(RoomDefinition definition) {
            if (!free(definition, kNorth) || !free(definition, kUp)) {
                return false;
            }
            return free(definition.connections[kNorth], kUp);
        }

        public OceanMonumentPiece create(Direction orientation, RoomDefinition definition, RandomSource random) {
            RoomDefinition north = definition.connections[kNorth];
            definition.claimed = true;
            north.claimed = true;
            definition.connections[kUp].claimed = true;
            north.connections[kUp].claimed = true;
            return new OceanMonumentDoubleYZRoom(orientation, definition);
        }
    }
}
