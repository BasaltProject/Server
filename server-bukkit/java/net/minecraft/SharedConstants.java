// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft;

import io.netty.util.ResourceLeakDetector;
import io.netty.util.ResourceLeakDetector.Level;
import net.minecraft.world.level.ChunkPos;

public class SharedConstants {
    private static final WorldVersion kVersion = new WorldVersion() {
        private final net.minecraft.world.level.storage.DataVersion data =
            new net.minecraft.world.level.storage.DataVersion(5023, "main");

        @Override
        public net.minecraft.world.level.storage.DataVersion dataVersion() {
            return data;
        }

        @Override
        public String id() {
            return "26.3";
        }

        @Override
        public String name() {
            return "26.3";
        }

        @Override
        public int protocolVersion() {
            return 777;
        }
    };

    private SharedConstants() {}

    public static WorldVersion getCurrentVersion() {
        return kVersion;
    }
    public static final boolean SNAPSHOT = false;
    public static final int WORLD_VERSION = 5023;
    public static final String SERIES = "main";
    public static final int RELEASE_NETWORK_PROTOCOL_VERSION = 777;
    public static final int SNAPSHOT_NETWORK_PROTOCOL_VERSION = 338;
    public static final int SNBT_NAG_VERSION = 4997;
    public static final boolean CRASH_EAGERLY = false;
    public static final int RESOURCE_PACK_FORMAT_MAJOR = 97;
    public static final int RESOURCE_PACK_FORMAT_MINOR = 1;
    public static final int DATA_PACK_FORMAT_MAJOR = 121;
    public static final int DATA_PACK_FORMAT_MINOR = 0;
    public static final String RPC_MANAGEMENT_SERVER_API_VERSION = "3.1.0";
    public static final int LANGUAGE_FORMAT = 1;
    public static final int REPORT_FORMAT_VERSION = 1;
    public static final String DATA_VERSION_TAG = "DataVersion";
    public static final String DEBUG_FLAG_PREFIX = "MC_DEBUG_";
    public static final boolean DEBUG_ENABLED = false;
    public static final boolean FIX_TNT_DUPE = false;
    public static final boolean FIX_SAND_DUPE = false;
    public static final boolean DEBUG_OPEN_INCOMPATIBLE_WORLDS = false;
    public static final boolean DEBUG_ALLOW_LOW_SIM_DISTANCE = false;
    public static final boolean DEBUG_HOTKEYS = false;
    public static final boolean DEBUG_UI_NARRATION = false;
    public static final boolean DEBUG_SHUFFLE_UI_RENDERING_ORDER = false;
    public static final boolean DEBUG_SHUFFLE_MODELS = false;
    public static final boolean DEBUG_RENDER_UI_LAYERING_RECTANGLES = false;
    public static final boolean DEBUG_PATHFINDING = false;
    public static final boolean DEBUG_SHOW_LOCAL_SERVER_ENTITY_HIT_BOXES = false;
    public static final boolean DEBUG_SHAPES = false;
    public static final boolean DEBUG_NEIGHBORSUPDATE = false;
    public static final boolean DEBUG_EXPERIMENTAL_REDSTONEWIRE_UPDATE_ORDER = false;
    public static final boolean DEBUG_STRUCTURES = false;
    public static final boolean DEBUG_GAME_EVENT_LISTENERS = false;
    public static final boolean DEBUG_DUMP_TEXTURE_ATLAS = false;
    public static final boolean DEBUG_STRUCTURE_EDIT_MODE = false;
    public static final boolean DEBUG_SAVE_STRUCTURES_AS_SNBT = false;
    public static final boolean DEBUG_SYNCHRONOUS_GL_LOGS = false;
    public static final boolean DEBUG_VERBOSE_SERVER_EVENTS = false;
    public static final boolean DEBUG_NAMED_RUNNABLES = false;
    public static final boolean DEBUG_GOAL_SELECTOR = false;
    public static final boolean DEBUG_VILLAGE_SECTIONS = false;
    public static final boolean DEBUG_BRAIN = false;
    public static final boolean DEBUG_POI = false;
    public static final boolean DEBUG_BEES = false;
    public static final boolean DEBUG_RAIDS = false;
    public static final boolean DEBUG_BLOCK_BREAK = false;
    public static final boolean DEBUG_MONITOR_TICK_TIMES = false;
    public static final boolean DEBUG_KEEP_JIGSAW_BLOCKS_DURING_STRUCTURE_GEN = false;
    public static final boolean DEBUG_DONT_SAVE_WORLD = false;
    public static final boolean DEBUG_LARGE_DRIPSTONE = false;
    public static final boolean DEBUG_ORE_VEINS = false;
    public static final boolean DEBUG_SCULK_CATALYST = false;
    public static final boolean DEBUG_BYPASS_REALMS_VERSION_CHECK = false;
    public static final boolean DEBUG_SOCIAL_INTERACTIONS = false;
    public static final boolean DEBUG_CHAT_DISABLED = false;
    public static final boolean DEBUG_CHAT_FRIENDS_ONLY = false;
    public static final boolean DEBUG_VALIDATE_RESOURCE_PATH_CASE = false;
    public static final boolean DEBUG_UNLOCK_ALL_TRADES = false;
    public static final boolean DEBUG_BREEZE_MOB = false;
    public static final boolean DEBUG_TRIAL_SPAWNER_DETECTS_SHEEP_AS_PLAYERS = false;
    public static final boolean DEBUG_VAULT_DETECTS_SHEEP_AS_PLAYERS = false;
    public static final boolean DEBUG_FORCE_ONBOARDING_SCREEN = false;
    public static final boolean DEBUG_CURSOR_POS = false;
    public static final boolean DEBUG_DEFAULT_SKIN_OVERRIDE = false;
    public static final boolean DEBUG_PANORAMA_SCREENSHOT = false;
    public static final boolean DEBUG_CHASE_COMMAND = false;
    public static final boolean DEBUG_VERBOSE_COMMAND_ERRORS = false;
    public static final boolean DEBUG_DEV_COMMANDS = false;
    public static final boolean DEBUG_ACTIVE_TEXT_AREAS = false;
    public static final boolean DEBUG_SIMULATE_LIBRARY_LOAD_FAILURE = false;
    public static final boolean DEBUG_IGNORE_LOCAL_MOB_CAP = false;
    public static final boolean DEBUG_DISABLE_LIQUID_SPREADING = false;
    public static final boolean DEBUG_AQUIFERS = false;
    public static final boolean DEBUG_JFR_PROFILING_ENABLE_LEVEL_LOADING = false;
    public static final boolean DEBUG_ENTITY_BLOCK_INTERSECTION = false;
    public static boolean debugGenerateSquareTerrainWithoutNoise;
    public static final boolean DEBUG_ONLY_GENERATE_HALF_THE_WORLD = false;
    public static final boolean DEBUG_DISABLE_FLUID_GENERATION = false;
    public static final boolean DEBUG_DISABLE_AQUIFERS = false;
    public static final boolean DEBUG_DISABLE_SURFACE = false;
    public static final boolean DEBUG_DISABLE_CARVERS = false;
    public static final boolean DEBUG_DISABLE_STRUCTURES = false;
    public static final boolean DEBUG_DISABLE_FEATURES = false;
    public static final boolean DEBUG_DISABLE_ORE_VEINS = false;
    public static final boolean DEBUG_DISABLE_BLENDING = false;
    public static final boolean DEBUG_DISABLE_BELOW_ZERO_RETROGENERATION = false;
    public static final int DEFAULT_MINECRAFT_PORT = 25565;
    public static final boolean DEBUG_SUBTITLES = false;
    public static final int DEBUG_FAKE_LATENCY_MS = 0;
    public static final int DEBUG_FAKE_JITTER_MS = 0;
    public static final ResourceLeakDetector.Level NETTY_LEAK_DETECTION = null;
    public static final boolean COMMAND_STACK_TRACES = false;
    public static final boolean DEBUG_WORLD_RECREATE = false;
    public static final boolean DEBUG_SHOW_SERVER_DEBUG_VALUES = false;
    public static final boolean DEBUG_FEATURE_COUNT = false;
    public static final boolean DEBUG_CALCULATE_SOLID = false;
    public static final boolean DEBUG_FORCE_TELEMETRY = false;
    public static final boolean DEBUG_DONT_SEND_TELEMETRY_TO_BACKEND = false;
    public static final boolean DEBUG_ENABLE_FARLANDS = false;
    public static final long MAXIMUM_TICK_TIME_NANOS = 0;
    public static final float MAXIMUM_BLOCK_EXPLOSION_RESISTANCE = 3600000.0F;
    public static final boolean USE_DEVONLY = false;
    public static boolean CHECK_DATA_FIXER_SCHEMA;
    public static boolean IS_RUNNING_IN_IDE;
    public static boolean IS_RENDERDOC_ATTACHED;
    public static final int WORLD_RESOLUTION = 16;
    public static final int MAX_CHAT_LENGTH = 256;
    public static final int MAX_USER_INPUT_COMMAND_LENGTH = 32500;
    public static final int MAX_FUNCTION_COMMAND_LENGTH = 2000000;
    public static final int MAX_PLAYER_NAME_LENGTH = 16;
    public static final int MAX_CHAINED_NEIGHBOR_UPDATES = 1000000;
    public static final int MAX_RENDER_DISTANCE = 32;
    public static final int MAX_CLOUD_DISTANCE = 128;
    public static final char[] ILLEGAL_FILE_CHARACTERS = null;
    public static final int TICKS_PER_SECOND = 20;
    public static final int MILLIS_PER_TICK = 50;
    public static final int TICKS_PER_MINUTE = 1200;
    public static final int TICKS_PER_GAME_DAY = 24000;
    public static final int DEFAULT_RANDOM_TICK_SPEED = 3;
    public static final float AVERAGE_GAME_TICKS_PER_RANDOM_TICK_PER_BLOCK = 1365.3334F;
    public static final float AVERAGE_RANDOM_TICKS_PER_BLOCK_PER_MINUTE = 0.87890625F;
    public static final float AVERAGE_RANDOM_TICKS_PER_BLOCK_PER_GAME_DAY = 17.578125F;
    public static final int WORLD_ICON_SIZE = 64;
    public static void setVersion(WorldVersion version) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.SharedConstants#setVersion");
    }
    public static void tryDetectVersion() {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.SharedConstants#tryDetectVersion");
    }
    public static int getProtocolVersion() {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.SharedConstants#getProtocolVersion");
    }
    public static boolean debugVoidTerrain(ChunkPos pos) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.SharedConstants#debugVoidTerrain");
    }
}
