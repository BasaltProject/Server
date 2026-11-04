package org.basaltmc.bukkit.facade;

import java.util.ArrayList;
import java.util.Collections;
import java.util.IdentityHashMap;
import java.util.List;
import java.util.Map;
import net.minecraft.core.registries.BuiltInRegistries;
import net.minecraft.resources.Identifier;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.world.effect.MobEffectInstance;
import net.minecraft.world.entity.LivingEntity;
import net.minecraft.util.RandomSource;
import net.minecraft.world.damagesource.DamageSource;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.item.enchantment.Enchantment;
import net.minecraft.world.item.enchantment.LevelBasedValue;
import net.minecraft.world.item.enchantment.effects.AddValue;
import net.minecraft.world.item.enchantment.effects.EnchantmentValueEffect;
import net.minecraft.world.item.enchantment.effects.MultiplyValue;
import net.minecraft.world.item.enchantment.effects.RemoveBinomial;
import net.minecraft.world.item.enchantment.effects.ScaleExponentially;
import net.minecraft.world.item.enchantment.effects.SetValue;
import org.basaltmc.bukkit.item.TagSets;

/** Evaluates the data-driven value effects of an enchantment from its native definition. */
public final class EnchantmentEffects {
    private static final Map<Enchantment, Map<?, ?>> kRaw = Collections.synchronizedMap(new IdentityHashMap<>());

    private EnchantmentEffects() {}

    static void remember(Enchantment enchantment, Object effects) {
        if (effects instanceof Map<?, ?> map) {
            kRaw.put(enchantment, map);
        }
    }

    private static List<Map<?, ?>> entries(Enchantment enchantment, String component) {
        Map<?, ?> effects = kRaw.get(enchantment);
        List<Map<?, ?>> out = new ArrayList<>();
        if (effects != null && effects.get(component) instanceof List<?> list) {
            for (Object entry : list) {
                if (entry instanceof Map<?, ?> map) {
                    out.add(map);
                }
            }
        }
        return out;
    }

    public static boolean has(Enchantment enchantment, String component) {
        return !entries(enchantment, component).isEmpty();
    }

    public static float apply(Enchantment enchantment, String component, int level, RandomSource random, float value, Entity victim,
                              DamageSource source, net.minecraft.world.item.ItemStack item) {
        float result = value;
        for (Map<?, ?> entry : entries(enchantment, component)) {
            if (entry.get("requirements") instanceof Map<?, ?> requirement && !test(requirement, victim, source, item)) {
                continue;
            }
            result = process(entry.get("effect"), level, random, result);
        }
        return result;
    }

    /** Runs the targeted entity effects of a component (post_attack, projectile_spawned, tick...) against the resolved targets. */
    public static void runEntity(Enchantment enchantment, String component, int level, ServerLevel world, Entity victim, DamageSource source,
                                 Entity enchanted) {
        for (Map<?, ?> entry : entries(enchantment, component)) {
            if (entry.get("requirements") instanceof Map<?, ?> requirement && !test(requirement, victim, source, null)) {
                continue;
            }
            Entity target = switch (String.valueOf(entry.get("affected"))) {
                case "attacker" -> source == null ? null : source.getEntity();
                case "damaging_entity" -> source == null ? null : source.getDirectEntity();
                case "victim" -> victim;
                default -> enchanted != null ? enchanted : victim;
            };
            if (target != null) {
                entityEffect(entry.get("effect"), level, world, target, source);
            }
        }
    }

    private static void entityEffect(Object raw, int level, ServerLevel world, Entity target, DamageSource source) {
        if (!(raw instanceof Map<?, ?> map)) {
            return;
        }
        String type = String.valueOf(map.get("type"));
        switch (type.startsWith("minecraft:") ? type.substring(10) : type) {
            case "all_of" -> {
                if (map.get("effects") instanceof List<?> list) {
                    for (Object effect : list) {
                        entityEffect(effect, level, world, target, source);
                    }
                }
            }
            case "ignite" -> target.igniteForSeconds(LevelBasedValue.fromData(map.get("duration")).calculate(level));
            case "damage_entity" -> {
                float amount = LevelBasedValue.fromData(map.get("min_damage")).calculate(level);
                float max = LevelBasedValue.fromData(map.get("max_damage")).calculate(level);
                float damage = amount + world.getRandom().nextFloat() * (max - amount);
                target.hurtServer(world, source == null ? world.damageSources().generic() : source, damage);
            }
            case "apply_mob_effect" -> {
                if (!(target instanceof LivingEntity living)) {
                    return;
                }
                Object ids = map.get("to_apply");
                List<?> candidates = ids instanceof List<?> list ? list : List.of(String.valueOf(ids));
                if (candidates.isEmpty()) {
                    return;
                }
                String id = String.valueOf(candidates.get(world.getRandom().nextInt(candidates.size())));
                float seconds = lerp(world, level, map.get("min_duration"), map.get("max_duration"));
                int amplifier = Math.round(lerp(world, level, map.get("min_amplifier"), map.get("max_amplifier")));
                BuiltInRegistries.MOB_EFFECT.get(Identifier.parse(id)).ifPresent(holder ->
                    living.addEffect(new MobEffectInstance(holder, Math.max(1, Math.round(seconds * 20.0F)), amplifier)));
            }
            case "apply_impulse" -> impulse(map, level, target);
            case "explode" -> {
                float radius = Math.max(LevelBasedValue.fromData(map.get("radius")).calculate(level), 0.0F);
                double[] offset = vector(map.get("offset"), 0.0, 0.0, 0.0);
                String interaction = String.valueOf(map.get("block_interaction"));
                world.explode(Boolean.TRUE.equals(map.get("attribute_to_user")) ? target : null, target.getX() + offset[0],
                    target.getY() + offset[1], target.getZ() + offset[2], radius, Boolean.TRUE.equals(map.get("create_fire")),
                    net.minecraft.world.level.Level.ExplosionInteraction.valueOf(interaction.toUpperCase(java.util.Locale.ROOT)));
            }
            case "run_function" -> org.bukkit.Bukkit.dispatchCommand(org.bukkit.Bukkit.getConsoleSender(), "execute as " + target.getUUID()
                + " at @s run function " + map.get("function"));
            default -> { }
        }
    }

    private static double[] vector(Object raw, double x, double y, double z) {
        if (raw instanceof List<?> list && list.size() == 3) {
            return new double[] {((Number) list.get(0)).doubleValue(), ((Number) list.get(1)).doubleValue(), ((Number) list.get(2)).doubleValue()};
        }
        return new double[] {x, y, z};
    }

    private static void impulse(Map<?, ?> map, int level, Entity target) {
        double[] direction = vector(map.get("direction"), 0.0, 0.0, 0.0);
        double[] scale = vector(map.get("coordinate_scale"), 1.0, 1.0, 1.0);
        double magnitude = LevelBasedValue.fromData(map.get("magnitude")).calculate(level);
        double pitch = Math.toRadians(target.getXRot());
        double yaw = Math.toRadians(-target.getYRot());
        double y1 = direction[1] * Math.cos(pitch) - direction[2] * Math.sin(pitch);
        double z1 = direction[1] * Math.sin(pitch) + direction[2] * Math.cos(pitch);
        double x2 = direction[0] * Math.cos(yaw) + z1 * Math.sin(yaw);
        double z2 = -direction[0] * Math.sin(yaw) + z1 * Math.cos(yaw);
        target.addDeltaMovement(new net.minecraft.world.phys.Vec3(x2 * scale[0] * magnitude, y1 * scale[1] * magnitude, z2 * scale[2] * magnitude));
    }

    private static float lerp(ServerLevel world, int level, Object min, Object max) {
        float low = LevelBasedValue.fromData(min).calculate(level);
        float high = max == null ? low : LevelBasedValue.fromData(max).calculate(level);
        return low + world.getRandom().nextFloat() * (high - low);
    }

    public static boolean anyMatches(Enchantment enchantment, String component, Entity victim, DamageSource source) {
        for (Map<?, ?> entry : entries(enchantment, component)) {
            if (!(entry.get("requirements") instanceof Map<?, ?> requirement) || test(requirement, victim, source, null)) {
                return true;
            }
        }
        return false;
    }

    public static EnchantmentValueEffect valueEffect(Object raw) {
        if (!(raw instanceof Map<?, ?> map)) {
            return null;
        }
        String type = String.valueOf(map.get("type"));
        return switch (type.startsWith("minecraft:") ? type.substring(10) : type) {
            case "add" -> new AddValue(LevelBasedValue.fromData(map.get("value")));
            case "multiply" -> new MultiplyValue(LevelBasedValue.fromData(map.get("factor")));
            case "set" -> new SetValue(LevelBasedValue.fromData(map.get("value")));
            case "remove_binomial" -> new RemoveBinomial(LevelBasedValue.fromData(map.get("chance")));
            case "scale_exponentially" -> new ScaleExponentially(LevelBasedValue.fromData(map.get("base")),
                LevelBasedValue.fromData(map.get("exponent")));
            default -> null;
        };
    }

    private static float process(Object raw, int level, RandomSource random, float value) {
        if (raw instanceof Map<?, ?> map && String.valueOf(map.get("type")).endsWith("all_of") && map.get("effects") instanceof List<?> list) {
            float result = value;
            for (Object effect : list) {
                result = process(effect, level, random, result);
            }
            return result;
        }
        EnchantmentValueEffect effect = valueEffect(raw);
        return effect == null ? value : effect.process(level, random, value);
    }

    private static boolean test(Map<?, ?> requirement, Entity victim, DamageSource source, net.minecraft.world.item.ItemStack item) {
        return Boolean.TRUE.equals(evaluate(requirement, victim, source, item));
    }

    /** True or false when the predicate is understood, null when it uses a condition this service cannot evaluate. */
    private static Boolean evaluate(Map<?, ?> requirement, Entity victim, DamageSource source, net.minecraft.world.item.ItemStack item) {
        String type = String.valueOf(requirement.get("type"));
        switch (type.startsWith("minecraft:") ? type.substring(10) : type) {
            case "all_of":
            case "any_of": {
                boolean all = type.endsWith("all_of");
                boolean unknown = false;
                if (requirement.get("terms") instanceof List<?> terms) {
                    for (Object term : terms) {
                        Boolean result = term instanceof Map<?, ?> map ? evaluate(map, victim, source, item) : null;
                        if (result == null) {
                            unknown = true;
                        } else if (result != all) {
                            return !all;
                        }
                    }
                }
                return unknown ? null : all;
            }
            case "inverted": {
                Boolean inner = requirement.get("term") instanceof Map<?, ?> term ? evaluate(term, victim, source, item) : null;
                return inner == null ? null : !inner;
            }
            case "entity_properties":
                return entityMatches(requirement.get("predicate"), "this".equals(String.valueOf(requirement.get("entity"))) ? victim : null);
            case "match_tool":
                return item == null || !(requirement.get("predicate") instanceof Map<?, ?> predicate) || predicate.size() != 1
                    || !predicate.containsKey("items") ? null
                    : TagSets.resolve("items", predicate.get("items")).contains(item.getItem().itemKey().toString());
            case "damage_source_properties":
                return sourceMatches(requirement.get("predicate"), source);
            default:
                return null;
        }
    }

    private static Boolean entityMatches(Object predicate, Entity entity) {
        if (!(predicate instanceof Map<?, ?> map) || entity == null) {
            return null;
        }
        for (Map.Entry<?, ?> entry : map.entrySet()) {
            if (!"minecraft:entity_type".equals(entry.getKey()) && !"type".equals(entry.getKey())) {
                return null;
            }
            String id = BuiltInRegistries.ENTITY_TYPE.getKey(entity.getType()).toString();
            if (!TagSets.resolve("entity_type", entry.getValue()).contains(id)) {
                return false;
            }
        }
        return true;
    }

    private static Boolean sourceMatches(Object predicate, DamageSource source) {
        if (!(predicate instanceof Map<?, ?> map) || source == null) {
            return null;
        }
        for (Map.Entry<?, ?> entry : map.entrySet()) {
            if (!"tags".equals(entry.getKey()) || !(entry.getValue() instanceof List<?> tags)) {
                return null;
            }
            for (Object tag : tags) {
                if (!(tag instanceof Map<?, ?> spec)) {
                    return null;
                }
                boolean expected = !(spec.get("expected") instanceof Number number) || number.intValue() != 0;
                if (source.isInTag(String.valueOf(spec.get("id"))) != expected) {
                    return false;
                }
            }
        }
        return true;
    }

    public static void attributes(Enchantment enchantment, int level, String suffix,
                                  java.util.function.BiConsumer<net.minecraft.core.Holder<net.minecraft.world.entity.ai.attributes.Attribute>,
                                      net.minecraft.world.entity.ai.attributes.AttributeModifier> sink) {
        for (Map<?, ?> entry : entries(enchantment, "minecraft:attributes")) {
            var attribute = net.minecraft.core.registries.BuiltInRegistries.ATTRIBUTE.get(net.minecraft.resources.Identifier.parse(String.valueOf(entry.get("attribute"))));
            if (attribute.isEmpty()) {
                continue;
            }
            String operation = String.valueOf(entry.get("operation"));
            var kind = net.minecraft.world.entity.ai.attributes.AttributeModifier.Operation.valueOf(operation.toUpperCase(java.util.Locale.ROOT));
            double amount = LevelBasedValue.fromData(entry.get("amount")).calculate(level);
            net.minecraft.resources.Identifier id = net.minecraft.resources.Identifier.parse(entry.get("id") + "/" + suffix);
            sink.accept(attribute.get(), new net.minecraft.world.entity.ai.attributes.AttributeModifier(id, amount, kind));
        }
    }


    private static final java.util.Set<String> kValueComponents = java.util.Set.of("damage_protection", "damage", "smash_damage_per_fallen_block",
        "knockback", "armor_effectiveness", "item_damage", "ammo_use", "projectile_piercing", "block_experience", "mob_experience", "repair_with_xp",
        "trident_return_acceleration", "fishing_time_reduction", "fishing_luck_bonus", "projectile_count", "projectile_spread", "crossbow_charge_time",
        "trident_spin_attack_strength");

    /** Typed view of a value-effect component; other components have no typed model yet and report null. */
    public static List<net.minecraft.world.item.enchantment.ConditionalEffect<EnchantmentValueEffect>> valueEffects(Enchantment enchantment, String name) {
        String bare = name.startsWith("minecraft:") ? name.substring(10) : name;
        if (!kValueComponents.contains(bare)) {
            return null;
        }
        List<net.minecraft.world.item.enchantment.ConditionalEffect<EnchantmentValueEffect>> out = new ArrayList<>();
        for (Map<?, ?> entry : entries(enchantment, "minecraft:" + bare)) {
            EnchantmentValueEffect effect = valueEffect(entry.get("effect"));
            if (effect != null) {
                out.add(new net.minecraft.world.item.enchantment.ConditionalEffect<>(effect, java.util.Optional.empty()));
            }
        }
        return out;
    }

}
