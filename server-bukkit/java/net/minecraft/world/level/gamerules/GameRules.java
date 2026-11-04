// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.gamerules;

import com.mojang.brigadier.arguments.ArgumentType;
import com.mojang.brigadier.arguments.BoolArgumentType;
import com.mojang.brigadier.arguments.IntegerArgumentType;
import com.mojang.serialization.Codec;
import java.util.List;
import java.util.Objects;
import java.util.function.ToIntFunction;
import java.util.stream.Stream;
import net.minecraft.core.Holder;
import net.minecraft.core.Registry;
import net.minecraft.core.registries.BuiltInRegistries;
import net.minecraft.server.MinecraftServer;
import net.minecraft.world.flag.FeatureFlagSet;
import org.basaltmc.bukkit.WorldNatives;

public class GameRules {
    public static final GameRule<Boolean> ADVANCE_TIME = bool("advance_time", GameRuleCategory.UPDATES, true);
    public static final GameRule<Boolean> ADVANCE_WEATHER = bool("advance_weather", GameRuleCategory.UPDATES, true);
    public static final GameRule<Boolean> ALLOW_ENTERING_NETHER_USING_PORTALS = bool("allow_entering_nether_using_portals", GameRuleCategory.MISC, true);
    public static final GameRule<Boolean> BLOCK_DROPS = bool("block_drops", GameRuleCategory.DROPS, true);
    public static final GameRule<Boolean> BLOCK_EXPLOSION_DROP_DECAY = bool("block_explosion_drop_decay", GameRuleCategory.DROPS, true);
    public static final GameRule<Boolean> COMMAND_BLOCKS_WORK = bool("command_blocks_work", GameRuleCategory.MISC, true);
    public static final GameRule<Boolean> COMMAND_BLOCK_OUTPUT = bool("command_block_output", GameRuleCategory.CHAT, true);
    public static final GameRule<Boolean> DROWNING_DAMAGE = bool("drowning_damage", GameRuleCategory.PLAYER, true);
    public static final GameRule<Boolean> ELYTRA_MOVEMENT_CHECK = bool("elytra_movement_check", GameRuleCategory.PLAYER, true);
    public static final GameRule<Boolean> ENDER_PEARLS_VANISH_ON_DEATH = bool("ender_pearls_vanish_on_death", GameRuleCategory.PLAYER, true);
    public static final GameRule<Boolean> ENTITY_DROPS = bool("entity_drops", GameRuleCategory.DROPS, true);
    public static final GameRule<Boolean> FALL_DAMAGE = bool("fall_damage", GameRuleCategory.PLAYER, true);
    public static final GameRule<Boolean> FIRE_DAMAGE = bool("fire_damage", GameRuleCategory.PLAYER, true);
    public static final GameRule<Integer> FIRE_SPREAD_RADIUS_AROUND_PLAYER = integer("fire_spread_radius_around_player", GameRuleCategory.UPDATES, 128, -1, Integer.MAX_VALUE);
    public static final GameRule<Boolean> FORGIVE_DEAD_PLAYERS = bool("forgive_dead_players", GameRuleCategory.MOBS, true);
    public static final GameRule<Boolean> FREEZE_DAMAGE = bool("freeze_damage", GameRuleCategory.PLAYER, true);
    public static final GameRule<Boolean> GLOBAL_SOUND_EVENTS = bool("global_sound_events", GameRuleCategory.MISC, true);
    public static final GameRule<Boolean> IMMEDIATE_RESPAWN = bool("immediate_respawn", GameRuleCategory.PLAYER, false);
    public static final GameRule<Boolean> KEEP_INVENTORY = bool("keep_inventory", GameRuleCategory.PLAYER, false);
    public static final GameRule<Boolean> LAVA_SOURCE_CONVERSION = bool("lava_source_conversion", GameRuleCategory.UPDATES, false);
    public static final GameRule<Boolean> LIMITED_CRAFTING = bool("limited_crafting", GameRuleCategory.PLAYER, false);
    public static final GameRule<Boolean> LOCATOR_BAR = bool("locator_bar", GameRuleCategory.PLAYER, true);
    public static final GameRule<Boolean> LOG_ADMIN_COMMANDS = bool("log_admin_commands", GameRuleCategory.CHAT, true);
    public static final GameRule<Integer> MAX_BLOCK_MODIFICATIONS = integer("max_block_modifications", GameRuleCategory.MISC, 32768, 1, Integer.MAX_VALUE);
    public static final GameRule<Integer> MAX_COMMAND_FORKS = integer("max_command_forks", GameRuleCategory.MISC, 65536, 0, Integer.MAX_VALUE);
    public static final GameRule<Integer> MAX_COMMAND_SEQUENCE_LENGTH = integer("max_command_sequence_length", GameRuleCategory.MISC, 65536, 0, Integer.MAX_VALUE);
    public static final GameRule<Integer> MAX_ENTITY_CRAMMING = integer("max_entity_cramming", GameRuleCategory.MOBS, 24, 0, Integer.MAX_VALUE);
    public static final GameRule<Integer> MAX_MINECART_SPEED = integer("max_minecart_speed", GameRuleCategory.MISC, 8, 1, 1000);
    public static final GameRule<Integer> MAX_SNOW_ACCUMULATION_HEIGHT = integer("max_snow_accumulation_height", GameRuleCategory.UPDATES, 1, 0, 8);
    public static final GameRule<Boolean> MOB_DROPS = bool("mob_drops", GameRuleCategory.DROPS, true);
    public static final GameRule<Boolean> MOB_EXPLOSION_DROP_DECAY = bool("mob_explosion_drop_decay", GameRuleCategory.DROPS, true);
    public static final GameRule<Boolean> MOB_GRIEFING = bool("mob_griefing", GameRuleCategory.MOBS, true);
    public static final GameRule<Boolean> NATURAL_HEALTH_REGENERATION = bool("natural_health_regeneration", GameRuleCategory.PLAYER, true);
    public static final GameRule<Boolean> PLAYER_MOVEMENT_CHECK = bool("player_movement_check", GameRuleCategory.PLAYER, true);
    public static final GameRule<Integer> PLAYERS_NETHER_PORTAL_CREATIVE_DELAY = integer("players_nether_portal_creative_delay", GameRuleCategory.PLAYER, 0, 0, Integer.MAX_VALUE);
    public static final GameRule<Integer> PLAYERS_NETHER_PORTAL_DEFAULT_DELAY = integer("players_nether_portal_default_delay", GameRuleCategory.PLAYER, 80, 0, Integer.MAX_VALUE);
    public static final GameRule<Integer> PLAYERS_SLEEPING_PERCENTAGE = integer("players_sleeping_percentage", GameRuleCategory.PLAYER, 100, 0, Integer.MAX_VALUE);
    public static final GameRule<Boolean> PROJECTILES_CAN_BREAK_BLOCKS = bool("projectiles_can_break_blocks", GameRuleCategory.DROPS, true);
    public static final GameRule<Boolean> PVP = bool("pvp", GameRuleCategory.PLAYER, true);
    public static final GameRule<Boolean> RAIDS = bool("raids", GameRuleCategory.MOBS, true);
    public static final GameRule<Integer> RANDOM_TICK_SPEED = integer("random_tick_speed", GameRuleCategory.UPDATES, 3, 0, Integer.MAX_VALUE);
    public static final GameRule<Boolean> REDUCED_DEBUG_INFO = bool("reduced_debug_info", GameRuleCategory.MISC, false);
    public static final GameRule<Integer> RESPAWN_RADIUS = integer("respawn_radius", GameRuleCategory.PLAYER, 10, 0, Integer.MAX_VALUE);
    public static final GameRule<Boolean> SEND_COMMAND_FEEDBACK = bool("send_command_feedback", GameRuleCategory.CHAT, true);
    public static final GameRule<Boolean> SHOW_ADVANCEMENT_MESSAGES = bool("show_advancement_messages", GameRuleCategory.CHAT, true);
    public static final GameRule<Boolean> SHOW_DEATH_MESSAGES = bool("show_death_messages", GameRuleCategory.CHAT, true);
    public static final GameRule<Boolean> SPAWNER_BLOCKS_WORK = bool("spawner_blocks_work", GameRuleCategory.MISC, true);
    public static final GameRule<Boolean> SPAWN_MOBS = bool("spawn_mobs", GameRuleCategory.SPAWNING, true);
    public static final GameRule<Boolean> SPAWN_MONSTERS = bool("spawn_monsters", GameRuleCategory.SPAWNING, true);
    public static final GameRule<Boolean> SPAWN_PATROLS = bool("spawn_patrols", GameRuleCategory.SPAWNING, true);
    public static final GameRule<Boolean> SPAWN_PHANTOMS = bool("spawn_phantoms", GameRuleCategory.SPAWNING, true);
    public static final GameRule<Boolean> SPAWN_WANDERING_TRADERS = bool("spawn_wandering_traders", GameRuleCategory.SPAWNING, true);
    public static final GameRule<Boolean> SPAWN_WARDENS = bool("spawn_wardens", GameRuleCategory.SPAWNING, true);
    public static final GameRule<Boolean> SPECTATORS_GENERATE_CHUNKS = bool("spectators_generate_chunks", GameRuleCategory.PLAYER, true);
    public static final GameRule<Boolean> SPREAD_VINES = bool("spread_vines", GameRuleCategory.UPDATES, true);
    public static final GameRule<Boolean> TNT_EXPLODES = bool("tnt_explodes", GameRuleCategory.MISC, true);
    public static final GameRule<Boolean> TNT_EXPLOSION_DROP_DECAY = bool("tnt_explosion_drop_decay", GameRuleCategory.DROPS, false);
    public static final GameRule<Boolean> UNIVERSAL_ANGER = bool("universal_anger", GameRuleCategory.MOBS, false);
    public static final GameRule<Boolean> WATER_SOURCE_CONVERSION = bool("water_source_conversion", GameRuleCategory.UPDATES, true);

    private final GameRuleMap rules;
    private final String world;

    public static Codec<GameRules> codec(FeatureFlagSet enabledFeatures) {
        return GameRuleMap.CODEC.xmap(map -> new GameRules(enabledFeatures, map), gameRules -> gameRules.snapshot());
    }

    public GameRules(FeatureFlagSet enabledFeatures, GameRuleMap map) {
        BuiltInRegistries.GAME_RULE.stream().forEach(rule -> {
            if (rule.isEnabled(enabledFeatures)) {
                if (!map.has(rule)) {
                    map.reset(rule);
                }
            } else if (map.has(rule)) {
                map.remove(rule);
            }
        });
        this.rules = map;
        this.world = null;
    }

    public GameRules(FeatureFlagSet enabledFeatures) {
        this.rules = GameRuleMap.of(BuiltInRegistries.GAME_RULE.filterFeatures(enabledFeatures).listElements().map(Holder::value));
        this.world = null;
    }

    public GameRules(List<GameRule<?>> rules) {
        this.rules = GameRuleMap.of(rules.stream());
        this.world = null;
    }

    private GameRules(String world) {
        this.rules = null;
        this.world = world;
    }

    /** The rules of a loaded world, read and written through the native game rule service. */
    public static GameRules forWorld(String world) {
        return new GameRules(world);
    }

    private GameRuleMap snapshot() {
        if (world == null) {
            return rules;
        }
        GameRuleMap copy = GameRuleMap.of();
        availableRules().forEach(rule -> copyInto(copy, rule));
        return copy;
    }

    private <T> void copyInto(GameRuleMap target, GameRule<T> rule) {
        target.set(rule, get(rule));
    }

    private boolean nativeHas(GameRule<?> rule) {
        return WorldNatives.gameRule(world, rule.id()) != null;
    }

    public Stream<GameRule<?>> availableRules() {
        if (world == null) {
            return rules.keySet().stream();
        }
        return BuiltInRegistries.GAME_RULE.stream().filter(this::nativeHas).map(rule -> (GameRule<?>) rule);
    }

    public <T> T get(GameRule<T> gameRule) {
        if (world == null) {
            T value = rules.get(gameRule);
            if (value == null) {
                throw new IllegalArgumentException("Tried to access invalid game rule " + gameRule.getIdentifierWithFallback());
            }
            return value;
        }
        String text = WorldNatives.gameRule(world, gameRule.id());
        if (text == null) {
            throw new IllegalArgumentException("Tried to access invalid game rule " + gameRule.getIdentifierWithFallback());
        }
        return gameRule.deserialize(text).result().orElse(gameRule.defaultValue());
    }

    public <T> void set(GameRule<T> gameRule, T value, MinecraftServer server) {
        boolean known = world == null ? rules.has(gameRule) : nativeHas(gameRule);
        if (!known) {
            return;
        }
        if (world == null) {
            rules.set(gameRule, value);
        } else if (!WorldNatives.setGameRule(world, gameRule.id(), gameRule.serialize(value))) {
            return;
        }
        if (server != null) {
            server.onGameRuleChanged(gameRule, value);
        }
    }

    public GameRules copy(FeatureFlagSet enabledFeatures) {
        return new GameRules(enabledFeatures, GameRuleMap.copyOf(snapshot()));
    }

    public void setAll(GameRules other, MinecraftServer server) {
        setAll(other.snapshot(), server);
    }

    public void setAll(GameRuleMap gameRulesMap, MinecraftServer server) {
        gameRulesMap.keySet().forEach(rule -> setFrom(gameRulesMap, rule, server));
    }

    private <T> void setFrom(GameRuleMap map, GameRule<T> rule, MinecraftServer server) {
        set(rule, Objects.requireNonNull(map.get(rule)), server);
    }

    public void visitGameRuleTypes(GameRuleTypeVisitor visitor) {
        availableRules().forEach(rule -> {
            visitor.visit(rule);
            rule.callVisitor(visitor);
        });
    }

    public static GameRule<?> bootstrap(Registry<GameRule<?>> registry) {
        return ADVANCE_TIME;
    }

    public <T> String getAsString(GameRule<T> gameRule) {
        return gameRule.serialize(get(gameRule));
    }

    private static GameRule<Boolean> bool(String id, GameRuleCategory category, boolean defaultValue) {
        return register(id, category, GameRuleType.BOOL, BoolArgumentType.bool(), Codec.BOOL, defaultValue, GameRuleTypeVisitor::visitBoolean, flag -> flag ? 1 : 0);
    }

    private static GameRule<Integer> integer(String id, GameRuleCategory category, int defaultValue, int min, int max) {
        return register(id, category, GameRuleType.INT, IntegerArgumentType.integer(min, max), Codec.intRange(min, max), defaultValue, GameRuleTypeVisitor::visitInteger, number -> number);
    }

    private static <T> GameRule<T> register(String id, GameRuleCategory category, GameRuleType type, ArgumentType<T> argument, Codec<T> codec, T defaultValue, VisitorCaller<T> caller, ToIntFunction<T> commandResult) {
        return Registry.register(BuiltInRegistries.GAME_RULE, id, new GameRule<>(category, type, argument, caller, codec, commandResult, defaultValue, FeatureFlagSet.of()));
    }

    public interface VisitorCaller<T> {
        void call(GameRuleTypeVisitor visitor, GameRule<T> key);
    }
}
