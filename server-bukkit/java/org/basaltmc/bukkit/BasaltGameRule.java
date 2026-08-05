package org.basaltmc.bukkit;

import java.util.ArrayList;
import java.util.List;
import java.util.Map;
import java.util.function.Function;
import org.bukkit.GameRule;
import org.bukkit.NamespacedKey;

public final class BasaltGameRule<T> extends GameRule<T> {
    private static final Map<String, String> kLegacy = Map.ofEntries(
        Map.entry("doDaylightCycle", "advance_time"), Map.entry("doWeatherCycle", "advance_weather"),
        Map.entry("announceAdvancements", "show_advancement_messages"),
        Map.entry("commandBlockOutput", "command_block_output"), Map.entry("doEntityDrops", "entity_drops"),
        Map.entry("doImmediateRespawn", "immediate_respawn"), Map.entry("doInsomnia", "spawn_phantoms"),
        Map.entry("doLimitedCrafting", "limited_crafting"), Map.entry("doMobLoot", "mob_drops"),
        Map.entry("doMobSpawning", "spawn_mobs"), Map.entry("doPatrolSpawning", "spawn_patrols"),
        Map.entry("doTileDrops", "block_drops"), Map.entry("doTraderSpawning", "spawn_wandering_traders"),
        Map.entry("doVinesSpread", "spread_vines"), Map.entry("doWardenSpawning", "spawn_wardens"),
        Map.entry("drowningDamage", "drowning_damage"), Map.entry("enderPearlsVanishOnDeath",
            "ender_pearls_vanish_on_death"), Map.entry("fallDamage", "fall_damage"),
        Map.entry("fireDamage", "fire_damage"), Map.entry("forgiveDeadPlayers", "forgive_dead_players"),
        Map.entry("freezeDamage", "freeze_damage"), Map.entry("globalSoundEvents", "global_sound_events"),
        Map.entry("keepInventory", "keep_inventory"), Map.entry("lavaSourceConversion", "lava_source_conversion"),
        Map.entry("locatorBar", "locator_bar"), Map.entry("logAdminCommands", "log_admin_commands"),
        Map.entry("maxCommandChainLength", "max_command_sequence_length"),
        Map.entry("maxCommandForkCount", "max_command_forks"), Map.entry("maxEntityCramming", "max_entity_cramming"),
        Map.entry("mobExplosionDropDecay", "mob_explosion_drop_decay"), Map.entry("mobGriefing", "mob_griefing"),
        Map.entry("naturalRegeneration", "natural_health_regeneration"),
        Map.entry("playersNetherPortalCreativeDelay", "players_nether_portal_creative_delay"),
        Map.entry("playersNetherPortalDefaultDelay", "players_nether_portal_default_delay"),
        Map.entry("playersSleepingPercentage", "players_sleeping_percentage"),
        Map.entry("projectilesCanBreakBlocks", "projectiles_can_break_blocks"), Map.entry("pvp", "pvp"),
        Map.entry("randomTickSpeed", "random_tick_speed"), Map.entry("reducedDebugInfo", "reduced_debug_info"),
        Map.entry("sendCommandFeedback", "send_command_feedback"), Map.entry("showDeathMessages", "show_death_messages"),
        Map.entry("snowAccumulationHeight", "max_snow_accumulation_height"), Map.entry("spawnRadius", "respawn_radius"),
        Map.entry("spectatorsGenerateChunks", "spectators_generate_chunks"), Map.entry("tntExplodes", "tnt_explodes"),
        Map.entry("tntExplosionDropDecay", "tnt_explosion_drop_decay"), Map.entry("universalAnger", "universal_anger"),
        Map.entry("blockExplosionDropDecay", "block_explosion_drop_decay"),
        Map.entry("commandBlocksEnabled", "command_blocks_work"), Map.entry("spawnerBlocksEnabled",
            "spawner_blocks_work"), Map.entry("disableRaids", "raids"), Map.entry("minecartMaxSpeed", "max_minecart_speed"));

    private final NamespacedKey key;
    private final Class<T> type;
    private final T fallback;

    @SuppressWarnings("unchecked")
    BasaltGameRule(String id, String kind, String start) {
        this.key = NamespacedKey.minecraft(id);
        this.type = (Class<T>) (kind.equals("boolean") ? Boolean.class : Integer.class);
        this.fallback = (T) (kind.equals("boolean") ? (Object) Boolean.parseBoolean(start) : (Object) Integer.parseInt(start));
    }

    /** The server's rule named in the game's own words, or in the older camel case Bukkit used. */
    static String modernName(String name) {
        String mapped = kLegacy.get(name);
        return mapped != null ? mapped : name;
    }

    static String legacyName(String id) {
        for (Map.Entry<String, String> entry : kLegacy.entrySet()) {
            if (entry.getValue().equals(id)) {
                return entry.getKey();
            }
        }
        return id;
    }

    static List<BasaltGameRule<?>> all() {
        String[] specs = WorldEnvNatives.gameRuleSpecs();
        List<BasaltGameRule<?>> out = new ArrayList<>();
        for (int i = 0; specs != null && i + 2 < specs.length; i += 3) {
            out.add(new BasaltGameRule<>(specs[i], specs[i + 1], specs[i + 2]));
        }
        return out;
    }

    @Override
    public NamespacedKey getKey() {
        return key;
    }

    public String id() {
        return key.getKey();
    }

    @Override
    public String getName() {
        return key.getKey();
    }

    @Override
    public Class<T> getType() {
        return type;
    }

    @Override
    public T getDefaultValue() {
        return fallback;
    }

    @Override
    public String translationKey() {
        return "gamerule." + key.getKey();
    }

    @Override
    public String toString() {
        return "GameRule{" + key + "}";
    }

    /** A rule under the name and type an older Bukkit gave it, read and written through the rule it became. */
    static final class Legacy<M, L> extends GameRule<L> {
        final GameRule<M> modern;
        final Function<L, M> toModern;
        final Function<M, L> fromModern;
        private final Class<L> type;

        Legacy(GameRule<M> modern, Function<L, M> toModern, Function<M, L> fromModern, Class<L> type) {
            this.modern = modern;
            this.toModern = toModern;
            this.fromModern = fromModern;
            this.type = type;
        }

        @Override
        public NamespacedKey getKey() {
            return modern.getKey();
        }

        @Override
        public String getName() {
            return legacyName(modern.getKey().getKey());
        }

        @Override
        public Class<L> getType() {
            return type;
        }

        @Override
        public L getDefaultValue() {
            return fromModern.apply(modern.getDefaultValue());
        }

        @Override
        public String translationKey() {
            return modern.translationKey();
        }
    }
}
