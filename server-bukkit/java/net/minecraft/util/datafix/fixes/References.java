// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util.datafix.fixes;

import com.mojang.datafixers.DSL;

public class References {
    public static final DSL.TypeReference LEVEL = reference("level");
    public static final DSL.TypeReference PLAYER = reference("player");
    public static final DSL.TypeReference CHUNK = reference("chunk");
    public static final DSL.TypeReference BLOCK_ENTITY = reference("block_entity");
    public static final DSL.TypeReference ENTITY = reference("entity");
    public static final DSL.TypeReference ITEM_STACK = reference("item_stack");
    public static final DSL.TypeReference OPTIONS = reference("options");
    public static final DSL.TypeReference STRUCTURE = reference("structure");
    public static final DSL.TypeReference BLOCK_STATE = reference("block_state");
    public static final DSL.TypeReference BIOME = reference("biome");
    public static final DSL.TypeReference ITEM_NAME = reference("item_name");

    private References() {}

    private static DSL.TypeReference reference(String name) {
        return () -> name;
    }
    public static final DSL.TypeReference LIGHTWEIGHT_LEVEL = null;
    public static final DSL.TypeReference HOTBAR = null;
    public static final DSL.TypeReference STATS = null;
    public static final DSL.TypeReference SAVED_DATA_COMMAND_STORAGE = null;
    public static final DSL.TypeReference SAVED_DATA_CUSTOM_BOSS_EVENTS = null;
    public static final DSL.TypeReference SAVED_DATA_ENDER_DRAGON_FIGHT = null;
    public static final DSL.TypeReference SAVED_DATA_GAME_RULES = null;
    public static final DSL.TypeReference SAVED_DATA_TICKETS = null;
    public static final DSL.TypeReference SAVED_DATA_MAP_DATA = null;
    public static final DSL.TypeReference SAVED_DATA_MAP_INDEX = null;
    public static final DSL.TypeReference SAVED_DATA_RAIDS = null;
    public static final DSL.TypeReference SAVED_DATA_RANDOM_SEQUENCES = null;
    public static final DSL.TypeReference SAVED_DATA_SCHEDULED_EVENTS = null;
    public static final DSL.TypeReference SAVED_DATA_SCOREBOARD = null;
    public static final DSL.TypeReference SAVED_DATA_STOPWATCHES = null;
    public static final DSL.TypeReference SAVED_DATA_STRUCTURE_FEATURE_INDICES = null;
    public static final DSL.TypeReference SAVED_DATA_WANDERING_TRADER = null;
    public static final DSL.TypeReference SAVED_DATA_WEATHER = null;
    public static final DSL.TypeReference SAVED_DATA_WORLD_BORDER = null;
    public static final DSL.TypeReference SAVED_DATA_WORLD_CLOCKS = null;
    public static final DSL.TypeReference SAVED_DATA_WORLD_GEN_SETTINGS = null;
    public static final DSL.TypeReference ADVANCEMENTS = null;
    public static final DSL.TypeReference POI_CHUNK = null;
    public static final DSL.TypeReference ENTITY_CHUNK = null;
    public static final DSL.TypeReference DEBUG_PROFILE = null;
    public static final DSL.TypeReference FLAT_BLOCK_STATE = null;
    public static final DSL.TypeReference DATA_COMPONENTS = null;
    public static final DSL.TypeReference VILLAGER_TRADE = null;
    public static final DSL.TypeReference PARTICLE = null;
    public static final DSL.TypeReference TEXT_COMPONENT = null;
    public static final DSL.TypeReference ENTITY_EQUIPMENT = null;
    public static final DSL.TypeReference ENTITY_NAME = null;
    public static final DSL.TypeReference ENTITY_TREE = null;
    public static final DSL.TypeReference BLOCK_NAME = null;
    public static final DSL.TypeReference GAME_EVENT_NAME = null;
    public static final DSL.TypeReference UNTAGGED_SPAWNER = null;
    public static final DSL.TypeReference STRUCTURE_FEATURE = null;
    public static final DSL.TypeReference OBJECTIVE = null;
    public static final DSL.TypeReference TEAM = null;
    public static final DSL.TypeReference RECIPE = null;
    public static final DSL.TypeReference MULTI_NOISE_BIOME_SOURCE_PARAMETER_LIST = null;
    public static final DSL.TypeReference WORLD_GEN_SETTINGS = null;
}
