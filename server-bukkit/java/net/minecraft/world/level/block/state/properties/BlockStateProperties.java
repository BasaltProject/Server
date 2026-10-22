package net.minecraft.world.level.block.state.properties;

import net.minecraft.core.Direction;
import net.minecraft.util.StringRepresentable;

public final class BlockStateProperties {
    public static final BooleanProperty SNOWY = BooleanProperty.create("snowy");
    public static final EnumProperty<AxisValue> AXIS = EnumProperty.create("axis", AxisValue.class, AxisValue.values());
    public static final EnumProperty<HorizontalAxisValue> HORIZONTAL_AXIS = EnumProperty.create(
            "axis", HorizontalAxisValue.class, HorizontalAxisValue.values());
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
    public static final EnumProperty<NoteblockInstrumentValue> NOTEBLOCK_INSTRUMENT = EnumProperty.create(
            "instrument", NoteblockInstrumentValue.class, NoteblockInstrumentValue.values());
    public static final IntegerProperty NOTE = IntegerProperty.create("note", 0, 24);
    public static final BooleanProperty POWERED = BooleanProperty.create("powered");
    public static final BooleanProperty OCCUPIED = BooleanProperty.create("occupied");
    public static final EnumProperty<BedPartValue> BED_PART = EnumProperty.create(
            "part", BedPartValue.class, BedPartValue.values());
    public static final EnumProperty<RailShapeStraightValue> RAIL_SHAPE_STRAIGHT = EnumProperty.create(
            "shape", RailShapeStraightValue.class, RailShapeStraightValue.values());
    public static final EnumProperty<StairsShapeValue> STAIRS_SHAPE = EnumProperty.create(
            "shape", StairsShapeValue.class, StairsShapeValue.values());
    public static final EnumProperty<RailShapeValue> RAIL_SHAPE = EnumProperty.create(
            "shape", RailShapeValue.class, RailShapeValue.values());
    public static final BooleanProperty EXTENDED = BooleanProperty.create("extended");
    public static final EnumProperty<DoubleBlockHalfValue> DOUBLE_BLOCK_HALF = EnumProperty.create(
            "half", DoubleBlockHalfValue.class, DoubleBlockHalfValue.values());
    public static final EnumProperty<HalfValue> HALF = EnumProperty.create("half", HalfValue.class, HalfValue.values());
    public static final BooleanProperty SHORT = BooleanProperty.create("short");
    public static final EnumProperty<PistonTypeValue> PISTON_TYPE = EnumProperty.create(
            "type", PistonTypeValue.class, PistonTypeValue.values());
    public static final EnumProperty<SlabTypeValue> SLAB_TYPE = EnumProperty.create(
            "type", SlabTypeValue.class, SlabTypeValue.values());
    public static final EnumProperty<ChestTypeValue> CHEST_TYPE = EnumProperty.create(
            "type", ChestTypeValue.class, ChestTypeValue.values());
    public static final BooleanProperty UNSTABLE = BooleanProperty.create("unstable");
    public static final BooleanProperty SLOT_0_OCCUPIED = BooleanProperty.create("slot_0_occupied");
    public static final BooleanProperty SLOT_1_OCCUPIED = BooleanProperty.create("slot_1_occupied");
    public static final BooleanProperty SLOT_2_OCCUPIED = BooleanProperty.create("slot_2_occupied");
    public static final BooleanProperty SLOT_3_OCCUPIED = BooleanProperty.create("slot_3_occupied");
    public static final BooleanProperty SLOT_4_OCCUPIED = BooleanProperty.create("slot_4_occupied");
    public static final BooleanProperty SLOT_5_OCCUPIED = BooleanProperty.create("slot_5_occupied");
    public static final EnumProperty<SideChainValue> SIDE_CHAIN = EnumProperty.create(
            "side_chain", SideChainValue.class, SideChainValue.values());
    public static final BooleanProperty EAST = BooleanProperty.create("east");
    public static final EnumProperty<EastRedstoneValue> EAST_REDSTONE = EnumProperty.create(
            "east", EastRedstoneValue.class, EastRedstoneValue.values());
    public static final EnumProperty<EastWallValue> EAST_WALL = EnumProperty.create(
            "east", EastWallValue.class, EastWallValue.values());
    public static final BooleanProperty NORTH = BooleanProperty.create("north");
    public static final EnumProperty<NorthRedstoneValue> NORTH_REDSTONE = EnumProperty.create(
            "north", NorthRedstoneValue.class, NorthRedstoneValue.values());
    public static final EnumProperty<NorthWallValue> NORTH_WALL = EnumProperty.create(
            "north", NorthWallValue.class, NorthWallValue.values());
    public static final BooleanProperty SOUTH = BooleanProperty.create("south");
    public static final EnumProperty<SouthRedstoneValue> SOUTH_REDSTONE = EnumProperty.create(
            "south", SouthRedstoneValue.class, SouthRedstoneValue.values());
    public static final EnumProperty<SouthWallValue> SOUTH_WALL = EnumProperty.create(
            "south", SouthWallValue.class, SouthWallValue.values());
    public static final BooleanProperty UP = BooleanProperty.create("up");
    public static final BooleanProperty WEST = BooleanProperty.create("west");
    public static final EnumProperty<WestRedstoneValue> WEST_REDSTONE = EnumProperty.create(
            "west", WestRedstoneValue.class, WestRedstoneValue.values());
    public static final EnumProperty<WestWallValue> WEST_WALL = EnumProperty.create(
            "west", WestWallValue.class, WestWallValue.values());
    public static final EnumProperty<CreakingHeartStateValue> CREAKING_HEART_STATE = EnumProperty.create(
            "creaking_heart_state", CreakingHeartStateValue.class, CreakingHeartStateValue.values());
    public static final BooleanProperty NATURAL = BooleanProperty.create("natural");
    public static final IntegerProperty POWER = IntegerProperty.create("power", 0, 15);
    public static final IntegerProperty MOISTURE = IntegerProperty.create("moisture", 0, 7);
    public static final BooleanProperty LIT = BooleanProperty.create("lit");
    public static final IntegerProperty ROTATION = IntegerProperty.create("rotation", 0, 15);
    public static final EnumProperty<DoorHingeValue> DOOR_HINGE = EnumProperty.create(
            "hinge", DoorHingeValue.class, DoorHingeValue.values());
    public static final BooleanProperty OPEN = BooleanProperty.create("open");
    public static final BooleanProperty ATTACHED = BooleanProperty.create("attached");
    public static final EnumProperty<AttachFaceValue> ATTACH_FACE = EnumProperty.create(
            "face", AttachFaceValue.class, AttachFaceValue.values());
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
    public static final EnumProperty<ComparatorModeValue> COMPARATOR_MODE = EnumProperty.create(
            "mode", ComparatorModeValue.class, ComparatorModeValue.values());
    public static final EnumProperty<StructureblockModeValue> STRUCTUREBLOCK_MODE = EnumProperty.create(
            "mode", StructureblockModeValue.class, StructureblockModeValue.values());
    public static final EnumProperty<TestBlockModeValue> TEST_BLOCK_MODE = EnumProperty.create(
            "mode", TestBlockModeValue.class, TestBlockModeValue.values());
    public static final BooleanProperty INVERTED = BooleanProperty.create("inverted");
    public static final BooleanProperty ENABLED = BooleanProperty.create("enabled");
    public static final IntegerProperty EGGS = IntegerProperty.create("eggs", 1, 4);
    public static final IntegerProperty HATCH = IntegerProperty.create("hatch", 0, 2);
    public static final IntegerProperty HYDRATION = IntegerProperty.create("hydration", 0, 3);
    public static final IntegerProperty PICKLES = IntegerProperty.create("pickles", 1, 4);
    public static final EnumProperty<LeavesValue> LEAVES = EnumProperty.create(
            "leaves", LeavesValue.class, LeavesValue.values());
    public static final BooleanProperty DRAG = BooleanProperty.create("drag");
    public static final BooleanProperty BOTTOM = BooleanProperty.create("bottom");
    public static final BooleanProperty HAS_BOOK = BooleanProperty.create("has_book");
    public static final EnumProperty<BellAttachmentValue> BELL_ATTACHMENT = EnumProperty.create(
            "attachment", BellAttachmentValue.class, BellAttachmentValue.values());
    public static final BooleanProperty SIGNAL_FIRE = BooleanProperty.create("signal_fire");
    public static final EnumProperty<OrientationValue> ORIENTATION = EnumProperty.create(
            "orientation", OrientationValue.class, OrientationValue.values());
    public static final IntegerProperty HONEY_LEVEL = IntegerProperty.create("honey_level", 0, 5);
    public static final IntegerProperty CHARGES = IntegerProperty.create("charges", 0, 4);
    public static final IntegerProperty CANDLES = IntegerProperty.create("candles", 1, 4);
    public static final EnumProperty<PotentSulfurStateValue> POTENT_SULFUR_STATE = EnumProperty.create(
            "potent_sulfur_state", PotentSulfurStateValue.class, PotentSulfurStateValue.values());
    public static final EnumProperty<SculkSensorPhaseValue> SCULK_SENSOR_PHASE = EnumProperty.create(
            "sculk_sensor_phase", SculkSensorPhaseValue.class, SculkSensorPhaseValue.values());
    public static final BooleanProperty BLOOM = BooleanProperty.create("bloom");
    public static final BooleanProperty CAN_SUMMON = BooleanProperty.create("can_summon");
    public static final BooleanProperty SHRIEKING = BooleanProperty.create("shrieking");
    public static final EnumProperty<CopperGolemPoseValue> COPPER_GOLEM_POSE = EnumProperty.create(
            "copper_golem_pose", CopperGolemPoseValue.class, CopperGolemPoseValue.values());
    public static final EnumProperty<ThicknessValue> THICKNESS = EnumProperty.create(
            "thickness", ThicknessValue.class, ThicknessValue.values());
    public static final EnumProperty<Direction> VERTICAL_DIRECTION = EnumProperty.create(
            "vertical_direction", Direction.class, Direction.UP, Direction.DOWN);
    public static final BooleanProperty BERRIES = BooleanProperty.create("berries");
    public static final IntegerProperty FLOWER_AMOUNT = IntegerProperty.create("flower_amount", 1, 4);
    public static final IntegerProperty SEGMENT_AMOUNT = IntegerProperty.create("segment_amount", 1, 4);
    public static final EnumProperty<TiltValue> TILT = EnumProperty.create("tilt", TiltValue.class, TiltValue.values());
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

    public enum AxisValue implements StringRepresentable {
        X, Y, Z;

        @Override
        public String getSerializedName() {
            return name().toLowerCase(java.util.Locale.ROOT);
        }

        @Override
        public String toString() {
            return getSerializedName();
        }
    }

    public enum HorizontalAxisValue implements StringRepresentable {
        X, Z;

        @Override
        public String getSerializedName() {
            return name().toLowerCase(java.util.Locale.ROOT);
        }

        @Override
        public String toString() {
            return getSerializedName();
        }
    }

    public enum NoteblockInstrumentValue implements StringRepresentable {
        HARP, BASEDRUM, SNARE, HAT, BASS, FLUTE, BELL, GUITAR, CHIME, XYLOPHONE, IRON_XYLOPHONE, COW_BELL, DIDGERIDOO, BIT, BANJO, PLING, TRUMPET, TRUMPET_EXPOSED, TRUMPET_OXIDIZED, TRUMPET_WEATHERED, ZOMBIE, SKELETON, CREEPER, DRAGON, WITHER_SKELETON, PIGLIN, CUSTOM_HEAD;

        @Override
        public String getSerializedName() {
            return name().toLowerCase(java.util.Locale.ROOT);
        }

        @Override
        public String toString() {
            return getSerializedName();
        }
    }

    public enum BedPartValue implements StringRepresentable {
        HEAD, FOOT;

        @Override
        public String getSerializedName() {
            return name().toLowerCase(java.util.Locale.ROOT);
        }

        @Override
        public String toString() {
            return getSerializedName();
        }
    }

    public enum RailShapeStraightValue implements StringRepresentable {
        NORTH_SOUTH, EAST_WEST, ASCENDING_EAST, ASCENDING_WEST, ASCENDING_NORTH, ASCENDING_SOUTH;

        @Override
        public String getSerializedName() {
            return name().toLowerCase(java.util.Locale.ROOT);
        }

        @Override
        public String toString() {
            return getSerializedName();
        }
    }

    public enum StairsShapeValue implements StringRepresentable {
        STRAIGHT, INNER_LEFT, INNER_RIGHT, OUTER_LEFT, OUTER_RIGHT;

        @Override
        public String getSerializedName() {
            return name().toLowerCase(java.util.Locale.ROOT);
        }

        @Override
        public String toString() {
            return getSerializedName();
        }
    }

    public enum RailShapeValue implements StringRepresentable {
        NORTH_SOUTH, EAST_WEST, ASCENDING_EAST, ASCENDING_WEST, ASCENDING_NORTH, ASCENDING_SOUTH, SOUTH_EAST, SOUTH_WEST, NORTH_WEST, NORTH_EAST;

        @Override
        public String getSerializedName() {
            return name().toLowerCase(java.util.Locale.ROOT);
        }

        @Override
        public String toString() {
            return getSerializedName();
        }
    }

    public enum DoubleBlockHalfValue implements StringRepresentable {
        UPPER, LOWER;

        @Override
        public String getSerializedName() {
            return name().toLowerCase(java.util.Locale.ROOT);
        }

        @Override
        public String toString() {
            return getSerializedName();
        }
    }

    public enum HalfValue implements StringRepresentable {
        TOP, BOTTOM;

        @Override
        public String getSerializedName() {
            return name().toLowerCase(java.util.Locale.ROOT);
        }

        @Override
        public String toString() {
            return getSerializedName();
        }
    }

    public enum PistonTypeValue implements StringRepresentable {
        NORMAL, STICKY;

        @Override
        public String getSerializedName() {
            return name().toLowerCase(java.util.Locale.ROOT);
        }

        @Override
        public String toString() {
            return getSerializedName();
        }
    }

    public enum SlabTypeValue implements StringRepresentable {
        TOP, BOTTOM, DOUBLE;

        @Override
        public String getSerializedName() {
            return name().toLowerCase(java.util.Locale.ROOT);
        }

        @Override
        public String toString() {
            return getSerializedName();
        }
    }

    public enum ChestTypeValue implements StringRepresentable {
        SINGLE, LEFT, RIGHT;

        @Override
        public String getSerializedName() {
            return name().toLowerCase(java.util.Locale.ROOT);
        }

        @Override
        public String toString() {
            return getSerializedName();
        }
    }

    public enum SideChainValue implements StringRepresentable {
        UNCONNECTED, RIGHT, CENTER, LEFT;

        @Override
        public String getSerializedName() {
            return name().toLowerCase(java.util.Locale.ROOT);
        }

        @Override
        public String toString() {
            return getSerializedName();
        }
    }

    public enum EastRedstoneValue implements StringRepresentable {
        UP, SIDE, NONE;

        @Override
        public String getSerializedName() {
            return name().toLowerCase(java.util.Locale.ROOT);
        }

        @Override
        public String toString() {
            return getSerializedName();
        }
    }

    public enum EastWallValue implements StringRepresentable {
        NONE, LOW, TALL;

        @Override
        public String getSerializedName() {
            return name().toLowerCase(java.util.Locale.ROOT);
        }

        @Override
        public String toString() {
            return getSerializedName();
        }
    }

    public enum NorthRedstoneValue implements StringRepresentable {
        UP, SIDE, NONE;

        @Override
        public String getSerializedName() {
            return name().toLowerCase(java.util.Locale.ROOT);
        }

        @Override
        public String toString() {
            return getSerializedName();
        }
    }

    public enum NorthWallValue implements StringRepresentable {
        NONE, LOW, TALL;

        @Override
        public String getSerializedName() {
            return name().toLowerCase(java.util.Locale.ROOT);
        }

        @Override
        public String toString() {
            return getSerializedName();
        }
    }

    public enum SouthRedstoneValue implements StringRepresentable {
        UP, SIDE, NONE;

        @Override
        public String getSerializedName() {
            return name().toLowerCase(java.util.Locale.ROOT);
        }

        @Override
        public String toString() {
            return getSerializedName();
        }
    }

    public enum SouthWallValue implements StringRepresentable {
        NONE, LOW, TALL;

        @Override
        public String getSerializedName() {
            return name().toLowerCase(java.util.Locale.ROOT);
        }

        @Override
        public String toString() {
            return getSerializedName();
        }
    }

    public enum WestRedstoneValue implements StringRepresentable {
        UP, SIDE, NONE;

        @Override
        public String getSerializedName() {
            return name().toLowerCase(java.util.Locale.ROOT);
        }

        @Override
        public String toString() {
            return getSerializedName();
        }
    }

    public enum WestWallValue implements StringRepresentable {
        NONE, LOW, TALL;

        @Override
        public String getSerializedName() {
            return name().toLowerCase(java.util.Locale.ROOT);
        }

        @Override
        public String toString() {
            return getSerializedName();
        }
    }

    public enum CreakingHeartStateValue implements StringRepresentable {
        UPROOTED, DORMANT, AWAKE;

        @Override
        public String getSerializedName() {
            return name().toLowerCase(java.util.Locale.ROOT);
        }

        @Override
        public String toString() {
            return getSerializedName();
        }
    }

    public enum DoorHingeValue implements StringRepresentable {
        LEFT, RIGHT;

        @Override
        public String getSerializedName() {
            return name().toLowerCase(java.util.Locale.ROOT);
        }

        @Override
        public String toString() {
            return getSerializedName();
        }
    }

    public enum AttachFaceValue implements StringRepresentable {
        FLOOR, WALL, CEILING;

        @Override
        public String getSerializedName() {
            return name().toLowerCase(java.util.Locale.ROOT);
        }

        @Override
        public String toString() {
            return getSerializedName();
        }
    }

    public enum ComparatorModeValue implements StringRepresentable {
        COMPARE, SUBTRACT;

        @Override
        public String getSerializedName() {
            return name().toLowerCase(java.util.Locale.ROOT);
        }

        @Override
        public String toString() {
            return getSerializedName();
        }
    }

    public enum StructureblockModeValue implements StringRepresentable {
        SAVE, LOAD, CORNER, DATA;

        @Override
        public String getSerializedName() {
            return name().toLowerCase(java.util.Locale.ROOT);
        }

        @Override
        public String toString() {
            return getSerializedName();
        }
    }

    public enum TestBlockModeValue implements StringRepresentable {
        START, LOG, FAIL, ACCEPT;

        @Override
        public String getSerializedName() {
            return name().toLowerCase(java.util.Locale.ROOT);
        }

        @Override
        public String toString() {
            return getSerializedName();
        }
    }

    public enum LeavesValue implements StringRepresentable {
        NONE, SMALL, LARGE;

        @Override
        public String getSerializedName() {
            return name().toLowerCase(java.util.Locale.ROOT);
        }

        @Override
        public String toString() {
            return getSerializedName();
        }
    }

    public enum BellAttachmentValue implements StringRepresentable {
        FLOOR, CEILING, SINGLE_WALL, DOUBLE_WALL;

        @Override
        public String getSerializedName() {
            return name().toLowerCase(java.util.Locale.ROOT);
        }

        @Override
        public String toString() {
            return getSerializedName();
        }
    }

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

    public enum PotentSulfurStateValue implements StringRepresentable {
        DRY, WET, DORMANT, ERUPTING, CONTINUOUS;

        @Override
        public String getSerializedName() {
            return name().toLowerCase(java.util.Locale.ROOT);
        }

        @Override
        public String toString() {
            return getSerializedName();
        }
    }

    public enum SculkSensorPhaseValue implements StringRepresentable {
        INACTIVE, ACTIVE, COOLDOWN;

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

    public enum ThicknessValue implements StringRepresentable {
        TIP_MERGE, TIP, FRUSTUM, MIDDLE, BASE;

        @Override
        public String getSerializedName() {
            return name().toLowerCase(java.util.Locale.ROOT);
        }

        @Override
        public String toString() {
            return getSerializedName();
        }
    }

    public enum TiltValue implements StringRepresentable {
        NONE, UNSTABLE, PARTIAL, FULL;

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

}
