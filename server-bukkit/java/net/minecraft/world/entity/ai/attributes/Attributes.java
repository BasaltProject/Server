// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.ai.attributes;

import net.minecraft.core.Holder;
import net.minecraft.core.MappedRegistry;
import net.minecraft.core.Registry;
import net.minecraft.core.registries.Registries;
import net.minecraft.resources.Identifier;

public class Attributes {
    public static final double DEFAULT_ATTACK_SPEED = 4.0;

    @SuppressWarnings({"unchecked", "rawtypes"})
    private static final MappedRegistry<Attribute> REGISTRY = new MappedRegistry<>((net.minecraft.resources.ResourceKey) Registries.ATTRIBUTE);

    public static final Holder<Attribute> AIR_DRAG_MODIFIER = register("air_drag_modifier", 1, 0, 2048, true, Attribute.Sentiment.POSITIVE);
    public static final Holder<Attribute> ARMOR = register("armor", 0, 0, 30, true, Attribute.Sentiment.POSITIVE);
    public static final Holder<Attribute> ARMOR_TOUGHNESS = register("armor_toughness", 0, 0, 20, true, Attribute.Sentiment.POSITIVE);
    public static final Holder<Attribute> ATTACK_DAMAGE = register("attack_damage", 2, 0, 2048, false, Attribute.Sentiment.POSITIVE);
    public static final Holder<Attribute> ATTACK_KNOCKBACK = register("attack_knockback", 0, 0, 5, false, Attribute.Sentiment.POSITIVE);
    public static final Holder<Attribute> ATTACK_SPEED = register("attack_speed", 4, 0, 1024, true, Attribute.Sentiment.POSITIVE);
    public static final Holder<Attribute> BELOW_NAME_DISTANCE = register("below_name_distance", 10, 0, 512, true, Attribute.Sentiment.POSITIVE);
    public static final Holder<Attribute> BLOCK_BREAK_SPEED = register("block_break_speed", 1, 0, 1024, true, Attribute.Sentiment.POSITIVE);
    public static final Holder<Attribute> BLOCK_INTERACTION_RANGE = register("block_interaction_range", 4.5, 0, 64, true, Attribute.Sentiment.POSITIVE);
    public static final Holder<Attribute> BOUNCINESS = register("bounciness", 0, 0, 1, true, Attribute.Sentiment.POSITIVE);
    public static final Holder<Attribute> BURNING_TIME = register("burning_time", 1, 0, 1024, true, Attribute.Sentiment.NEGATIVE);
    public static final Holder<Attribute> CAMERA_DISTANCE = register("camera_distance", 4, 0, 32, true, Attribute.Sentiment.POSITIVE);
    public static final Holder<Attribute> EXPLOSION_KNOCKBACK_RESISTANCE = register("explosion_knockback_resistance", 0, 0, 1, true, Attribute.Sentiment.POSITIVE);
    public static final Holder<Attribute> ENTITY_INTERACTION_RANGE = register("entity_interaction_range", 3, 0, 64, true, Attribute.Sentiment.POSITIVE);
    public static final Holder<Attribute> FALL_DAMAGE_MULTIPLIER = register("fall_damage_multiplier", 1, 0, 100, true, Attribute.Sentiment.NEGATIVE);
    public static final Holder<Attribute> FLYING_SPEED = register("flying_speed", 0.4, 0, 1024, true, Attribute.Sentiment.POSITIVE);
    public static final Holder<Attribute> FOLLOW_RANGE = register("follow_range", 32, 0, 2048, false, Attribute.Sentiment.POSITIVE);
    public static final Holder<Attribute> FRICTION_MODIFIER = register("friction_modifier", 1, 0, 2048, true, Attribute.Sentiment.POSITIVE);
    public static final Holder<Attribute> GRAVITY = register("gravity", 0.08, -1, 1, true, Attribute.Sentiment.NEUTRAL);
    public static final Holder<Attribute> JUMP_STRENGTH = register("jump_strength", 0.41999998688697815, 0, 32, true, Attribute.Sentiment.POSITIVE);
    public static final Holder<Attribute> KNOCKBACK_RESISTANCE = register("knockback_resistance", 0, -2, 1, false, Attribute.Sentiment.POSITIVE);
    public static final Holder<Attribute> LUCK = register("luck", 0, -1024, 1024, true, Attribute.Sentiment.POSITIVE);
    public static final Holder<Attribute> MAX_ABSORPTION = register("max_absorption", 0, 0, 2048, true, Attribute.Sentiment.POSITIVE);
    public static final Holder<Attribute> MAX_HEALTH = register("max_health", 20, 1, 1024, true, Attribute.Sentiment.POSITIVE);
    public static final Holder<Attribute> MINING_EFFICIENCY = register("mining_efficiency", 0, 0, 1024, true, Attribute.Sentiment.POSITIVE);
    public static final Holder<Attribute> MOVEMENT_EFFICIENCY = register("movement_efficiency", 0, 0, 1, true, Attribute.Sentiment.POSITIVE);
    public static final Holder<Attribute> MOVEMENT_SPEED = register("movement_speed", 0.7, 0, 1024, true, Attribute.Sentiment.POSITIVE);
    public static final Holder<Attribute> NAME_TAG_DISTANCE = register("name_tag_distance", 64, 0, 512, true, Attribute.Sentiment.POSITIVE);
    public static final Holder<Attribute> OXYGEN_BONUS = register("oxygen_bonus", 0, 0, 1024, true, Attribute.Sentiment.POSITIVE);
    public static final Holder<Attribute> SAFE_FALL_DISTANCE = register("safe_fall_distance", 3, -1024, 1024, true, Attribute.Sentiment.POSITIVE);
    public static final Holder<Attribute> SCALE = register("scale", 1, 0.0625, 16, true, Attribute.Sentiment.NEUTRAL);
    public static final Holder<Attribute> SNEAKING_SPEED = register("sneaking_speed", 0.3, 0, 1, true, Attribute.Sentiment.POSITIVE);
    public static final Holder<Attribute> SPAWN_REINFORCEMENTS_CHANCE = register("spawn_reinforcements", 0, 0, 1, false, Attribute.Sentiment.POSITIVE);
    public static final Holder<Attribute> STEP_HEIGHT = register("step_height", 0.6, 0, 10, true, Attribute.Sentiment.POSITIVE);
    public static final Holder<Attribute> SUBMERGED_MINING_SPEED = register("submerged_mining_speed", 0.2, 0, 20, true, Attribute.Sentiment.POSITIVE);
    public static final Holder<Attribute> SWEEPING_DAMAGE_RATIO = register("sweeping_damage_ratio", 0, 0, 1, true, Attribute.Sentiment.POSITIVE);
    public static final Holder<Attribute> TEMPT_RANGE = register("tempt_range", 10, 0, 2048, false, Attribute.Sentiment.POSITIVE);
    public static final Holder<Attribute> WATER_MOVEMENT_EFFICIENCY = register("water_movement_efficiency", 0, 0, 1, true, Attribute.Sentiment.POSITIVE);
    public static final Holder<Attribute> WAYPOINT_TRANSMIT_RANGE = register("waypoint_transmit_range", 0, 0, 6.0E7, false, Attribute.Sentiment.NEUTRAL);
    public static final Holder<Attribute> WAYPOINT_RECEIVE_RANGE = register("waypoint_receive_range", 0, 0, 6.0E7, false, Attribute.Sentiment.NEUTRAL);

    private static Holder<Attribute> register(String name, double def, double min, double max, boolean syncable,
        Attribute.Sentiment sentiment) {
        Attribute attribute = new RangedAttribute("attribute.name." + name, def, min, max).setSyncable(syncable).setSentiment(sentiment);
        return REGISTRY.register(Identifier.withDefaultNamespace(name), attribute);
    }

    public static Registry<Attribute> registry() {
        return REGISTRY;
    }

    static String nameOf(Holder<Attribute> attribute) {
        return attribute.unwrapKey().map(key -> key.identifier().toString()).orElse("[unregistered]");
    }

    public static Holder<Attribute> bootstrap(Registry<Attribute> registry) {
        return MAX_HEALTH;
    }
}
