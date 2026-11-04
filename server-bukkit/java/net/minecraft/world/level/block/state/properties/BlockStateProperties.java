// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.block.state.properties;

import net.minecraft.core.Direction;
import net.minecraft.util.StringRepresentable;
import net.minecraft.core.FrontAndTop;
import net.minecraft.world.level.block.CopperGolemStatueBlock;
import net.minecraft.world.level.block.entity.trialspawner.TrialSpawnerState;
import net.minecraft.world.level.block.entity.vault.VaultState;

public final class BlockStateProperties {
    public static final BooleanProperty SNOWY = BooleanProperty.create("snowy");
    public static final EnumProperty<Direction.Axis> AXIS = EnumProperty.create("axis", Direction.Axis.class, Direction.Axis.values());
    public static final EnumProperty<Direction.Axis> HORIZONTAL_AXIS = EnumProperty.create(
            "axis", Direction.Axis.class, Direction.Axis.X, Direction.Axis.Z);
    public static final IntegerProperty STAGE = IntegerProperty.create("stage", 0, 1);
    public static final IntegerProperty AGE_4 = IntegerProperty.create("age", 0, 4);
    public static final IntegerProperty AGE_15 = IntegerProperty.create("age", 0, 15);
    public static final IntegerProperty AGE_7 = IntegerProperty.create("age", 0, 7);
    public static final IntegerProperty AGE_3 = IntegerProperty.create("age", 0, 3);
    public static final IntegerProperty AGE_2 = IntegerProperty.create("age", 0, 2);
    public static final IntegerProperty AGE_1 = IntegerProperty.create("age", 0, 1);
    public static final IntegerProperty AGE_5 = IntegerProperty.create("age", 0, 5);
    public static final IntegerProperty AGE_25 = IntegerProperty.create("age", 0, 25);
    public static final BooleanProperty HANGING = BooleanProperty.create("hanging");
    public static final BooleanProperty WATERLOGGED = BooleanProperty.create("waterlogged");
    public static final IntegerProperty LEVEL_15 = IntegerProperty.create("level", 0, 15);
    public static final IntegerProperty LEVEL_1_3 = IntegerProperty.create("level", 1, 3);
    public static final IntegerProperty LEVEL_8 = IntegerProperty.create("level", 0, 8);
    public static final IntegerProperty DUSTED = IntegerProperty.create("dusted", 0, 3);
    public static final IntegerProperty DISTANCE_1_7 = IntegerProperty.create("distance", 1, 7);
    public static final IntegerProperty DISTANCE_7 = IntegerProperty.create("distance", 0, 7);
    public static final BooleanProperty PERSISTENT = BooleanProperty.create("persistent");
    public static final EnumProperty<Direction> FACING = EnumProperty.create(
            "facing", Direction.class, Direction.NORTH, Direction.EAST, Direction.SOUTH, Direction.WEST, Direction.UP,
            Direction.DOWN);
    public static final EnumProperty<Direction> HORIZONTAL_FACING = EnumProperty.create(
            "facing", Direction.class, Direction.NORTH, Direction.SOUTH, Direction.WEST, Direction.EAST);
    public static final EnumProperty<Direction> FACING_HOPPER = EnumProperty.create(
            "facing", Direction.class, Direction.DOWN, Direction.NORTH, Direction.SOUTH, Direction.WEST,
            Direction.EAST);
    public static final BooleanProperty TRIGGERED = BooleanProperty.create("triggered");
    public static final EnumProperty<NoteBlockInstrument> NOTEBLOCK_INSTRUMENT = EnumProperty.create(
            "instrument", NoteBlockInstrument.class, NoteBlockInstrument.values());
    public static final IntegerProperty NOTE = IntegerProperty.create("note", 0, 24);
    public static final BooleanProperty POWERED = BooleanProperty.create("powered");
    public static final BooleanProperty OCCUPIED = BooleanProperty.create("occupied");
    public static final EnumProperty<BedPart> BED_PART = EnumProperty.create(
            "part", BedPart.class, BedPart.values());
    public static final EnumProperty<RailShape> RAIL_SHAPE_STRAIGHT = EnumProperty.create(
            "shape", RailShape.class, RailShape.NORTH_SOUTH, RailShape.EAST_WEST, RailShape.ASCENDING_EAST,
            RailShape.ASCENDING_WEST, RailShape.ASCENDING_NORTH, RailShape.ASCENDING_SOUTH);
    public static final EnumProperty<StairsShape> STAIRS_SHAPE = EnumProperty.create(
            "shape", StairsShape.class, StairsShape.values());
    public static final EnumProperty<RailShape> RAIL_SHAPE = EnumProperty.create(
            "shape", RailShape.class, RailShape.values());
    public static final BooleanProperty EXTENDED = BooleanProperty.create("extended");
    public static final EnumProperty<DoubleBlockHalf> DOUBLE_BLOCK_HALF = EnumProperty.create(
            "half", DoubleBlockHalf.class, DoubleBlockHalf.values());
    public static final EnumProperty<Half> HALF = EnumProperty.create("half", Half.class, Half.values());
    public static final BooleanProperty SHORT = BooleanProperty.create("short");
    public static final EnumProperty<PistonType> PISTON_TYPE = EnumProperty.create(
            "type", PistonType.class, PistonType.values());
    public static final EnumProperty<SlabType> SLAB_TYPE = EnumProperty.create(
            "type", SlabType.class, SlabType.values());
    public static final EnumProperty<ChestType> CHEST_TYPE = EnumProperty.create(
            "type", ChestType.class, ChestType.values());
    public static final BooleanProperty UNSTABLE = BooleanProperty.create("unstable");
    public static final BooleanProperty SLOT_0_OCCUPIED = BooleanProperty.create("slot_0_occupied");
    public static final BooleanProperty SLOT_1_OCCUPIED = BooleanProperty.create("slot_1_occupied");
    public static final BooleanProperty SLOT_2_OCCUPIED = BooleanProperty.create("slot_2_occupied");
    public static final BooleanProperty SLOT_3_OCCUPIED = BooleanProperty.create("slot_3_occupied");
    public static final BooleanProperty SLOT_4_OCCUPIED = BooleanProperty.create("slot_4_occupied");
    public static final BooleanProperty SLOT_5_OCCUPIED = BooleanProperty.create("slot_5_occupied");
    public static final EnumProperty<SideChainPart> SIDE_CHAIN = EnumProperty.create(
            "side_chain", SideChainPart.class, SideChainPart.values());
    public static final BooleanProperty EAST = BooleanProperty.create("east");
    public static final EnumProperty<RedstoneSide> EAST_REDSTONE = EnumProperty.create(
            "east", RedstoneSide.class, RedstoneSide.values());
    public static final EnumProperty<WallSide> EAST_WALL = EnumProperty.create(
            "east", WallSide.class, WallSide.values());
    public static final BooleanProperty NORTH = BooleanProperty.create("north");
    public static final EnumProperty<RedstoneSide> NORTH_REDSTONE = EnumProperty.create(
            "north", RedstoneSide.class, RedstoneSide.values());
    public static final EnumProperty<WallSide> NORTH_WALL = EnumProperty.create(
            "north", WallSide.class, WallSide.values());
    public static final BooleanProperty SOUTH = BooleanProperty.create("south");
    public static final EnumProperty<RedstoneSide> SOUTH_REDSTONE = EnumProperty.create(
            "south", RedstoneSide.class, RedstoneSide.values());
    public static final EnumProperty<WallSide> SOUTH_WALL = EnumProperty.create(
            "south", WallSide.class, WallSide.values());
    public static final BooleanProperty UP = BooleanProperty.create("up");
    public static final BooleanProperty WEST = BooleanProperty.create("west");
    public static final EnumProperty<RedstoneSide> WEST_REDSTONE = EnumProperty.create(
            "west", RedstoneSide.class, RedstoneSide.values());
    public static final EnumProperty<WallSide> WEST_WALL = EnumProperty.create(
            "west", WallSide.class, WallSide.values());
    public static final EnumProperty<CreakingHeartState> CREAKING_HEART_STATE = EnumProperty.create(
            "creaking_heart_state", CreakingHeartState.class, CreakingHeartState.values());
    public static final BooleanProperty NATURAL = BooleanProperty.create("natural");
    public static final IntegerProperty POWER = IntegerProperty.create("power", 0, 15);
    public static final IntegerProperty MOISTURE = IntegerProperty.create("moisture", 0, 7);
    public static final BooleanProperty LIT = BooleanProperty.create("lit");
    public static final IntegerProperty ROTATION = IntegerProperty.create("rotation", 0, 15);
    public static final EnumProperty<DoorHingeSide> DOOR_HINGE = EnumProperty.create(
            "hinge", DoorHingeSide.class, DoorHingeSide.values());
    public static final BooleanProperty OPEN = BooleanProperty.create("open");
    public static final BooleanProperty ATTACHED = BooleanProperty.create("attached");
    public static final EnumProperty<AttachFace> ATTACH_FACE = EnumProperty.create(
            "face", AttachFace.class, AttachFace.values());
    public static final IntegerProperty LAYERS = IntegerProperty.create("layers", 1, 8);
    public static final BooleanProperty HAS_RECORD = BooleanProperty.create("has_record");
    public static final IntegerProperty BITES = IntegerProperty.create("bites", 0, 6);
    public static final IntegerProperty DELAY = IntegerProperty.create("delay", 1, 4);
    public static final BooleanProperty LOCKED = BooleanProperty.create("locked");
    public static final BooleanProperty DOWN = BooleanProperty.create("down");
    public static final BooleanProperty IN_WALL = BooleanProperty.create("in_wall");
    public static final BooleanProperty HAS_BOTTLE_0 = BooleanProperty.create("has_bottle_0");
    public static final BooleanProperty HAS_BOTTLE_1 = BooleanProperty.create("has_bottle_1");
    public static final BooleanProperty HAS_BOTTLE_2 = BooleanProperty.create("has_bottle_2");
    public static final BooleanProperty EYE = BooleanProperty.create("eye");
    public static final BooleanProperty DISARMED = BooleanProperty.create("disarmed");
    public static final BooleanProperty CONDITIONAL = BooleanProperty.create("conditional");
    public static final EnumProperty<ComparatorMode> COMPARATOR_MODE = EnumProperty.create(
            "mode", ComparatorMode.class, ComparatorMode.values());
    public static final EnumProperty<StructureMode> STRUCTUREBLOCK_MODE = EnumProperty.create(
            "mode", StructureMode.class, StructureMode.values());
    public static final EnumProperty<TestBlockMode> TEST_BLOCK_MODE = EnumProperty.create(
            "mode", TestBlockMode.class, TestBlockMode.values());
    public static final BooleanProperty INVERTED = BooleanProperty.create("inverted");
    public static final BooleanProperty ENABLED = BooleanProperty.create("enabled");
    public static final IntegerProperty EGGS = IntegerProperty.create("eggs", 1, 4);
    public static final IntegerProperty HATCH = IntegerProperty.create("hatch", 0, 2);
    public static final IntegerProperty HYDRATION = IntegerProperty.create("hydration", 0, 3);
    public static final IntegerProperty PICKLES = IntegerProperty.create("pickles", 1, 4);
    public static final EnumProperty<BambooLeaves> LEAVES = EnumProperty.create(
            "leaves", BambooLeaves.class, BambooLeaves.values());
    public static final BooleanProperty DRAG = BooleanProperty.create("drag");
    public static final BooleanProperty BOTTOM = BooleanProperty.create("bottom");
    public static final BooleanProperty HAS_BOOK = BooleanProperty.create("has_book");
    public static final EnumProperty<BellAttachType> BELL_ATTACHMENT = EnumProperty.create(
            "attachment", BellAttachType.class, BellAttachType.values());
    public static final BooleanProperty SIGNAL_FIRE = BooleanProperty.create("signal_fire");
    public static final EnumProperty<OrientationValue> ORIENTATION = EnumProperty.create(
            "orientation", OrientationValue.class, OrientationValue.values());
    public static final IntegerProperty HONEY_LEVEL = IntegerProperty.create("honey_level", 0, 5);
    public static final IntegerProperty CHARGES = IntegerProperty.create("charges", 0, 4);
    public static final IntegerProperty CANDLES = IntegerProperty.create("candles", 1, 4);
    public static final EnumProperty<PotentSulfurState> POTENT_SULFUR_STATE = EnumProperty.create(
            "potent_sulfur_state", PotentSulfurState.class, PotentSulfurState.values());
    public static final EnumProperty<SculkSensorPhase> SCULK_SENSOR_PHASE = EnumProperty.create(
            "sculk_sensor_phase", SculkSensorPhase.class, SculkSensorPhase.values());
    public static final BooleanProperty BLOOM = BooleanProperty.create("bloom");
    public static final BooleanProperty CAN_SUMMON = BooleanProperty.create("can_summon");
    public static final BooleanProperty SHRIEKING = BooleanProperty.create("shrieking");
    public static final EnumProperty<CopperGolemPoseValue> COPPER_GOLEM_POSE = EnumProperty.create(
            "copper_golem_pose", CopperGolemPoseValue.class, CopperGolemPoseValue.values());
    public static final EnumProperty<SpeleothemThickness> THICKNESS = EnumProperty.create(
            "thickness", SpeleothemThickness.class, SpeleothemThickness.values());
    public static final EnumProperty<Direction> VERTICAL_DIRECTION = EnumProperty.create(
            "vertical_direction", Direction.class, Direction.UP, Direction.DOWN);
    public static final BooleanProperty BERRIES = BooleanProperty.create("berries");
    public static final IntegerProperty FLOWER_AMOUNT = IntegerProperty.create("flower_amount", 1, 4);
    public static final IntegerProperty SEGMENT_AMOUNT = IntegerProperty.create("segment_amount", 1, 4);
    public static final EnumProperty<Tilt> TILT = EnumProperty.create("tilt", Tilt.class, Tilt.values());
    public static final BooleanProperty CRACKED = BooleanProperty.create("cracked");
    public static final BooleanProperty CRAFTING = BooleanProperty.create("crafting");
    public static final BooleanProperty OMINOUS = BooleanProperty.create("ominous");
    public static final EnumProperty<TrialSpawnerStateValue> TRIAL_SPAWNER_STATE = EnumProperty.create(
            "trial_spawner_state", TrialSpawnerStateValue.class, TrialSpawnerStateValue.values());
    public static final EnumProperty<VaultStateValue> VAULT_STATE = EnumProperty.create(
            "vault_state", VaultStateValue.class, VaultStateValue.values());
    public static final BooleanProperty TIP = BooleanProperty.create("tip");

    private BlockStateProperties() {}

    public static void init() {}






























    public enum OrientationValue implements StringRepresentable {
        DOWN_EAST, DOWN_NORTH, DOWN_SOUTH, DOWN_WEST, UP_EAST, UP_NORTH, UP_SOUTH, UP_WEST, WEST_UP, EAST_UP, NORTH_UP, SOUTH_UP;

        @Override
        public String getSerializedName() {
            return name().toLowerCase(java.util.Locale.ROOT);
        }

        @Override
        public String toString() {
            return getSerializedName();
        }
    }



    public enum CopperGolemPoseValue implements StringRepresentable {
        STANDING, SITTING, RUNNING, STAR;

        @Override
        public String getSerializedName() {
            return name().toLowerCase(java.util.Locale.ROOT);
        }

        @Override
        public String toString() {
            return getSerializedName();
        }
    }



    public enum TrialSpawnerStateValue implements StringRepresentable {
        INACTIVE, WAITING_FOR_PLAYERS, ACTIVE, WAITING_FOR_REWARD_EJECTION, EJECTING_REWARD, COOLDOWN;

        @Override
        public String getSerializedName() {
            return name().toLowerCase(java.util.Locale.ROOT);
        }

        @Override
        public String toString() {
            return getSerializedName();
        }
    }

    public enum VaultStateValue implements StringRepresentable {
        INACTIVE, ACTIVE, UNLOCKING, EJECTING;

        @Override
        public String getSerializedName() {
            return name().toLowerCase(java.util.Locale.ROOT);
        }

        @Override
        public String toString() {
            return getSerializedName();
        }
    }

    public static final BooleanProperty FALLING = BooleanProperty.create("falling");
    public static final EnumProperty<SideChainPart> SIDE_CHAIN_PART = SIDE_CHAIN;
    public static final int MAX_AGE_1 = 1;
    public static final int MAX_AGE_2 = 2;
    public static final int MAX_AGE_3 = 3;
    public static final int MAX_AGE_4 = 4;
    public static final int MAX_AGE_5 = 5;
    public static final int MAX_AGE_7 = 7;
    public static final int MAX_AGE_15 = 15;
    public static final int MAX_AGE_25 = 25;
    public static final int MAX_DISTANCE = 7;
    public static final IntegerProperty DISTANCE = DISTANCE_1_7;
    public static final int MIN_LEVEL = 0;
    public static final int MIN_LEVEL_CAULDRON = 1;
    public static final int MAX_LEVEL_3 = 3;
    public static final int MAX_LEVEL_8 = 8;
    public static final IntegerProperty LEVEL_CAULDRON = LEVEL_1_3;
    public static final IntegerProperty LEVEL_COMPOSTER = LEVEL_8;
    public static final IntegerProperty LEVEL_FLOWING = IntegerProperty.create("level", 1, 8);
    public static final IntegerProperty LEVEL_HONEY = HONEY_LEVEL;
    public static final int MAX_LEVEL_15 = 15;
    public static final IntegerProperty LEVEL = LEVEL_15;
    public static final int STABILITY_MAX_DISTANCE = 7;
    public static final IntegerProperty STABILITY_DISTANCE = DISTANCE_7;
    public static final int MIN_RESPAWN_ANCHOR_CHARGES = 0;
    public static final int MAX_RESPAWN_ANCHOR_CHARGES = 4;
    public static final IntegerProperty RESPAWN_ANCHOR_CHARGES = CHARGES;
    public static final IntegerProperty DRIED_GHAST_HYDRATION_LEVELS = HYDRATION;
    public static final IntegerProperty ROTATION_16 = ROTATION;
    public static final EnumProperty<ComparatorMode> MODE_COMPARATOR = COMPARATOR_MODE;
    public static final EnumProperty<BambooLeaves> BAMBOO_LEAVES = LEAVES;
    public static final EnumProperty<SpeleothemThickness> SPELEOTHEM_THICKNESS = THICKNESS;
    public static final BooleanProperty MAP = BooleanProperty.create("map");
}
