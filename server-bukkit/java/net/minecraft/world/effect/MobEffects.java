// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.effect;

import net.minecraft.core.Holder;
import net.minecraft.core.MappedRegistry;
import net.minecraft.core.registries.Registries;
import net.minecraft.resources.Identifier;
import net.minecraft.resources.ResourceKey;
import net.minecraft.world.entity.ai.attributes.Attribute;
import net.minecraft.world.entity.ai.attributes.AttributeModifier;
import net.minecraft.world.entity.ai.attributes.Attributes;

public class MobEffects {
    @SuppressWarnings({"unchecked", "rawtypes"})
    private static final MappedRegistry<MobEffect> REGISTRY = new MappedRegistry<>((ResourceKey) Registries.MOB_EFFECT);

    public static final Holder<MobEffect> SPEED = effect("speed", MobEffectCategory.BENEFICIAL, 3402751)
        .attr(Attributes.MOVEMENT_SPEED, "effect.speed", 0.2, AttributeModifier.Operation.ADD_MULTIPLIED_TOTAL).get();
    public static final Holder<MobEffect> SLOWNESS = effect("slowness", MobEffectCategory.HARMFUL, 9154528)
        .attr(Attributes.MOVEMENT_SPEED, "effect.slowness", -0.15, AttributeModifier.Operation.ADD_MULTIPLIED_TOTAL).get();
    public static final Holder<MobEffect> HASTE = effect("haste", MobEffectCategory.BENEFICIAL, 14270531)
        .attr(Attributes.ATTACK_SPEED, "effect.haste", 0.1, AttributeModifier.Operation.ADD_MULTIPLIED_TOTAL).get();
    public static final Holder<MobEffect> MINING_FATIGUE = effect("mining_fatigue", MobEffectCategory.HARMFUL, 4866583)
        .attr(Attributes.ATTACK_SPEED, "effect.mining_fatigue", -0.1, AttributeModifier.Operation.ADD_MULTIPLIED_TOTAL).get();
    public static final Holder<MobEffect> STRENGTH = effect("strength", MobEffectCategory.BENEFICIAL, 16762624)
        .attr(Attributes.ATTACK_DAMAGE, "effect.strength", 3.0, AttributeModifier.Operation.ADD_VALUE).get();
    public static final Holder<MobEffect> INSTANT_HEALTH = effect("instant_health", MobEffectCategory.BENEFICIAL, 16262179).get();
    public static final Holder<MobEffect> INSTANT_DAMAGE = effect("instant_damage", MobEffectCategory.HARMFUL, 11101546).get();
    public static final Holder<MobEffect> JUMP_BOOST = effect("jump_boost", MobEffectCategory.BENEFICIAL, 16646020)
        .attr(Attributes.SAFE_FALL_DISTANCE, "effect.jump_boost", 1.0, AttributeModifier.Operation.ADD_VALUE).get();
    public static final Holder<MobEffect> NAUSEA = effect("nausea", MobEffectCategory.HARMFUL, 5578058).get();
    public static final Holder<MobEffect> REGENERATION = effect("regeneration", MobEffectCategory.BENEFICIAL, 13458603).get();
    public static final Holder<MobEffect> RESISTANCE = effect("resistance", MobEffectCategory.BENEFICIAL, 9520880).get();
    public static final Holder<MobEffect> FIRE_RESISTANCE = effect("fire_resistance", MobEffectCategory.BENEFICIAL, 16750848).get();
    public static final Holder<MobEffect> WATER_BREATHING = effect("water_breathing", MobEffectCategory.BENEFICIAL, 10017472).get();
    public static final Holder<MobEffect> INVISIBILITY = effect("invisibility", MobEffectCategory.BENEFICIAL, 16185078)
        .attr(Attributes.WAYPOINT_TRANSMIT_RANGE, "effect.waypoint_transmit_range_hide", -1.0, AttributeModifier.Operation.ADD_MULTIPLIED_TOTAL).get();
    public static final Holder<MobEffect> BLINDNESS = effect("blindness", MobEffectCategory.HARMFUL, 2039587).get();
    public static final Holder<MobEffect> NIGHT_VISION = effect("night_vision", MobEffectCategory.BENEFICIAL, 12779366).get();
    public static final Holder<MobEffect> HUNGER = effect("hunger", MobEffectCategory.HARMFUL, 5797459).get();
    public static final Holder<MobEffect> WEAKNESS = effect("weakness", MobEffectCategory.HARMFUL, 4738376)
        .attr(Attributes.ATTACK_DAMAGE, "effect.weakness", -4.0, AttributeModifier.Operation.ADD_VALUE).get();
    public static final Holder<MobEffect> POISON = effect("poison", MobEffectCategory.HARMFUL, 8889187).get();
    public static final Holder<MobEffect> WITHER = effect("wither", MobEffectCategory.HARMFUL, 7561558).get();
    public static final Holder<MobEffect> HEALTH_BOOST = effect("health_boost", MobEffectCategory.BENEFICIAL, 16284963)
        .attr(Attributes.MAX_HEALTH, "effect.health_boost", 4.0, AttributeModifier.Operation.ADD_VALUE).get();
    public static final Holder<MobEffect> ABSORPTION = effect("absorption", MobEffectCategory.BENEFICIAL, 2445989)
        .attr(Attributes.MAX_ABSORPTION, "effect.absorption", 4.0, AttributeModifier.Operation.ADD_VALUE).get();
    public static final Holder<MobEffect> SATURATION = effect("saturation", MobEffectCategory.BENEFICIAL, 16262179).get();
    public static final Holder<MobEffect> GLOWING = effect("glowing", MobEffectCategory.NEUTRAL, 9740385).get();
    public static final Holder<MobEffect> LEVITATION = effect("levitation", MobEffectCategory.HARMFUL, 13565951).get();
    public static final Holder<MobEffect> LUCK = effect("luck", MobEffectCategory.BENEFICIAL, 5882118)
        .attr(Attributes.LUCK, "effect.luck", 1.0, AttributeModifier.Operation.ADD_VALUE).get();
    public static final Holder<MobEffect> UNLUCK = effect("unluck", MobEffectCategory.HARMFUL, 12624973)
        .attr(Attributes.LUCK, "effect.unluck", -1.0, AttributeModifier.Operation.ADD_VALUE).get();
    public static final Holder<MobEffect> SLOW_FALLING = effect("slow_falling", MobEffectCategory.BENEFICIAL, 15978425).get();
    public static final Holder<MobEffect> CONDUIT_POWER = effect("conduit_power", MobEffectCategory.BENEFICIAL, 1950417).get();
    public static final Holder<MobEffect> DOLPHINS_GRACE = effect("dolphins_grace", MobEffectCategory.BENEFICIAL, 8954814).get();
    public static final Holder<MobEffect> BAD_OMEN = effect("bad_omen", MobEffectCategory.NEUTRAL, 745784).get();
    public static final Holder<MobEffect> HERO_OF_THE_VILLAGE = effect("hero_of_the_village", MobEffectCategory.BENEFICIAL, 4521796).get();
    public static final Holder<MobEffect> DARKNESS = effect("darkness", MobEffectCategory.HARMFUL, 2696993)
        .blend(22).get();
    public static final Holder<MobEffect> TRIAL_OMEN = effect("trial_omen", MobEffectCategory.NEUTRAL, 1484454).get();
    public static final Holder<MobEffect> RAID_OMEN = effect("raid_omen", MobEffectCategory.NEUTRAL, 14565464).get();
    public static final Holder<MobEffect> WIND_CHARGED = effect("wind_charged", MobEffectCategory.HARMFUL, 12438015).get();
    public static final Holder<MobEffect> WEAVING = effect("weaving", MobEffectCategory.HARMFUL, 7891290).get();
    public static final Holder<MobEffect> OOZING = effect("oozing", MobEffectCategory.HARMFUL, 10092451).get();
    public static final Holder<MobEffect> INFESTED = effect("infested", MobEffectCategory.HARMFUL, 9214860).get();
    public static final Holder<MobEffect> BREATH_OF_THE_NAUTILUS = effect("breath_of_the_nautilus", MobEffectCategory.BENEFICIAL, 65518).get();

    public static MappedRegistry<MobEffect> registry() {
        return REGISTRY;
    }

    private static Builder effect(String name, MobEffectCategory category, int color) {
        MobEffect made = switch (name) {
            case "poison" -> new PoisonMobEffect(category, color);
            case "wither" -> new WitherMobEffect(category, color);
            case "regeneration" -> new RegenerationMobEffect(category, color);
            case "hunger" -> new HungerMobEffect(category, color);
            case "saturation" -> new SaturationMobEffect(category, color);
            case "absorption" -> new AbsorptionMobEffect(category, color);
            default -> new MobEffect(category, color) {};
        };
        return new Builder(name, made);
    }

    public static Holder<MobEffect> bootstrap(net.minecraft.core.Registry<MobEffect> registry) {
        return SPEED;
    }

    private static final class Builder {
        private final String name;
        private final MobEffect effect;

        Builder(String name, MobEffect effect) {
            this.name = name;
            this.effect = effect;
        }

        Builder attr(Holder<Attribute> attribute, String id, double amount, AttributeModifier.Operation operation) {
            effect.addAttributeModifier(attribute, Identifier.withDefaultNamespace(id), amount, operation);
            return this;
        }

        Builder blend(int ticks) {
            effect.setBlendDuration(ticks);
            return this;
        }

        Holder<MobEffect> get() {
            return REGISTRY.register(Identifier.withDefaultNamespace(name), effect);
        }
    }
}
