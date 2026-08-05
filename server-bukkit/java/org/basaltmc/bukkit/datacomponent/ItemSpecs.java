package org.basaltmc.bukkit.datacomponent;

import io.papermc.paper.datacomponent.item.AttackRange;
import io.papermc.paper.datacomponent.item.BlocksAttacks;
import io.papermc.paper.datacomponent.item.Consumable;
import io.papermc.paper.datacomponent.item.DeathProtection;
import io.papermc.paper.datacomponent.item.Equippable;
import io.papermc.paper.datacomponent.item.ItemAttributeModifiers;
import io.papermc.paper.datacomponent.item.KineticWeapon;
import io.papermc.paper.datacomponent.item.PiercingWeapon;
import io.papermc.paper.datacomponent.item.SwingAnimation;
import io.papermc.paper.datacomponent.item.Tool;
import io.papermc.paper.datacomponent.item.UseEffects;
import io.papermc.paper.datacomponent.item.Weapon;
import io.papermc.paper.datacomponent.item.attribute.AttributeModifierDisplay;
import io.papermc.paper.datacomponent.item.blocksattacks.DamageReduction;
import io.papermc.paper.datacomponent.item.blocksattacks.ItemDamageFunction;
import io.papermc.paper.datacomponent.item.consumable.ConsumeEffect;
import io.papermc.paper.datacomponent.item.consumable.ItemUseAnimation;
import io.papermc.paper.registry.RegistryKey;
import java.util.ArrayList;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Locale;
import java.util.Map;
import net.kyori.adventure.key.Key;
import net.kyori.adventure.text.Component;
import net.kyori.adventure.text.ComponentLike;
import net.kyori.adventure.util.TriState;
import org.basaltmc.bukkit.datacomponent.Dyn.Spec;
import org.basaltmc.bukkit.item.Convert;
import org.basaltmc.bukkit.item.MetaAttributes;
import org.basaltmc.bukkit.item.MetaComponents;
import org.basaltmc.bukkit.item.Nbt;
import org.basaltmc.bukkit.item.Texts;
import org.bukkit.attribute.Attribute;
import org.bukkit.attribute.AttributeModifier;
import org.bukkit.inventory.EquipmentSlot;
import org.bukkit.inventory.EquipmentSlotGroup;

public final class ItemSpecs {
    private ItemSpecs() {}

    private static String lower(Enum<?> value) {
        return value.name().toLowerCase(Locale.ROOT);
    }

    private static Object real(Map<String, Object> from, String name, double fallback) {
        return (float) Nbt.real(from, name, fallback);
    }

    private static Map<String, Object> mapOf(Object stored) {
        Map<String, Object> map = Nbt.map(stored);
        return map == null ? Nbt.compound() : map;
    }

    public static final Spec RULE = new Spec(Tool.Rule.class, null).def("correctForDrops", TriState.NOT_SET).nbt(
        fields -> {
            Map<String, Object> out = Nbt.compound();
            out.put("blocks", Wrap.setNbt((io.papermc.paper.registry.set.RegistryKeySet<?>) fields.get("blocks")));
            Wrap.put(out, "speed", fields.get("speed"));
            TriState correct = (TriState) fields.get("correctForDrops");
            if (correct != TriState.NOT_SET) {
                out.put("correct_for_drops", Wrap.bool(correct == TriState.TRUE));
            }
            return out;
        }, stored -> {
            Map<String, Object> map = mapOf(stored);
            Map<String, Object> fields = new LinkedHashMap<>();
            fields.put("blocks", Wrap.set(RegistryKey.BLOCK, map.get("blocks")));
            if (map.get("speed") instanceof Number speed) {
                fields.put("speed", speed.floatValue());
            }
            if (map.containsKey("correct_for_drops")) {
                fields.put("correctForDrops", TriState.byBoolean(Nbt.flag(map, "correct_for_drops", false)));
            }
            return fields;
        });

    public static final Spec TOOL = new Spec(Tool.class, Tool.Builder.class).def("defaultMiningSpeed", 1.0F)
        .def("damagePerBlock", 1).def("canDestroyBlocksInCreative", true).add("addRule", "rules")
        .addAll("addRules", "rules").nbt(fields -> {
            Map<String, Object> out = Nbt.compound();
            List<Object> rules = new ArrayList<>();
            for (Object rule : Dyn.list(fields, "rules")) {
                rules.add(((Dyn.Value) rule).nbt());
            }
            out.put("rules", rules);
            out.put("default_mining_speed", fields.get("defaultMiningSpeed"));
            out.put("damage_per_block", fields.get("damagePerBlock"));
            out.put("can_destroy_blocks_in_creative", Wrap.bool((Boolean) fields.get("canDestroyBlocksInCreative")));
            return out;
        }, stored -> {
            Map<String, Object> map = mapOf(stored);
            Map<String, Object> fields = new LinkedHashMap<>();
            List<Object> rules = new ArrayList<>();
            List<Object> stor = Nbt.list(map.get("rules"));
            for (Object rule : stor == null ? List.<Object>of() : stor) {
                rules.add(RULE.read(rule));
            }
            fields.put("rules", rules);
            fields.put("defaultMiningSpeed", real(map, "default_mining_speed", 1.0));
            fields.put("damagePerBlock", Nbt.integer(map, "damage_per_block", 1));
            fields.put("canDestroyBlocksInCreative", Nbt.flag(map, "can_destroy_blocks_in_creative", true));
            return fields;
        });

    public static final Spec EQUIPPABLE = new Spec(Equippable.class, Equippable.Builder.class)
        .def("equipSound", Key.key("minecraft:item.armor.equip_generic")).def("dispensable", true)
        .def("swappable", true).def("damageOnHurt", true).def("equipOnInteract", false).def("canBeSheared", false)
        .def("shearSound", Key.key("minecraft:item.shears.snip")).nbt(fields -> {
            Map<String, Object> out = Nbt.compound();
            out.put("slot", MetaComponents.slotName((EquipmentSlot) fields.get("slot")));
            Wrap.putKey(out, "equip_sound", (Key) fields.get("equipSound"));
            Wrap.putKey(out, "asset_id", (Key) fields.get("assetId"));
            Wrap.putKey(out, "camera_overlay", (Key) fields.get("cameraOverlay"));
            if (fields.get("allowedEntities") != null) {
                out.put("allowed_entities",
                    Wrap.setNbt((io.papermc.paper.registry.set.RegistryKeySet<?>) fields.get("allowedEntities")));
            }
            out.put("dispensable", Wrap.bool((Boolean) fields.get("dispensable")));
            out.put("swappable", Wrap.bool((Boolean) fields.get("swappable")));
            out.put("damage_on_hurt", Wrap.bool((Boolean) fields.get("damageOnHurt")));
            out.put("equip_on_interact", Wrap.bool((Boolean) fields.get("equipOnInteract")));
            out.put("can_be_sheared", Wrap.bool((Boolean) fields.get("canBeSheared")));
            Wrap.putKey(out, "shearing_sound", (Key) fields.get("shearSound"));
            return out;
        }, stored -> {
            Map<String, Object> map = mapOf(stored);
            Map<String, Object> fields = new LinkedHashMap<>();
            fields.put("slot", MetaComponents.slotOf(Nbt.string(map, "slot", "head")));
            put(fields, "equipSound", map.get("equip_sound"));
            put(fields, "assetId", map.get("asset_id"));
            put(fields, "cameraOverlay", map.get("camera_overlay"));
            if (map.get("allowed_entities") != null) {
                fields.put("allowedEntities", Wrap.set(RegistryKey.ENTITY_TYPE, map.get("allowed_entities")));
            }
            fields.put("dispensable", Nbt.flag(map, "dispensable", true));
            fields.put("swappable", Nbt.flag(map, "swappable", true));
            fields.put("damageOnHurt", Nbt.flag(map, "damage_on_hurt", true));
            fields.put("equipOnInteract", Nbt.flag(map, "equip_on_interact", false));
            fields.put("canBeSheared", Nbt.flag(map, "can_be_sheared", false));
            put(fields, "shearSound", map.get("shearing_sound"));
            return fields;
        });

    private static void put(Map<String, Object> fields, String name, Object stored) {
        if (stored != null) {
            fields.put(name, Wrap.key(stored));
        }
    }

    public static Object equippable(EquipmentSlot slot) {
        Map<String, Object> fields = new LinkedHashMap<>();
        fields.put("slot", slot);
        return Dyn.builder(EQUIPPABLE, fields);
    }

    public static final Spec APPLY_EFFECTS = new Spec(ConsumeEffect.ApplyStatusEffects.class, null)
        .def("probability", 1.0F).nbt(fields -> {
            Map<String, Object> out = Nbt.compound();
            out.put("type", "minecraft:apply_effects");
            List<Object> effects = new ArrayList<>();
            for (Object effect : Dyn.list(fields, "effects")) {
                effects.add(Convert.effect((org.bukkit.potion.PotionEffect) effect));
            }
            out.put("effects", effects);
            out.put("probability", fields.get("probability"));
            return out;
        }, stored -> {
            Map<String, Object> map = mapOf(stored);
            Map<String, Object> fields = new LinkedHashMap<>();
            fields.put("effects", new ArrayList<Object>(Convert.effects(map.get("effects"))));
            fields.put("probability", real(map, "probability", 1.0));
            return fields;
        });

    public static final Spec REMOVE_EFFECTS = new Spec(ConsumeEffect.RemoveStatusEffects.class, null).nbt(fields -> {
        Map<String, Object> out = Nbt.compound();
        out.put("type", "minecraft:remove_effects");
        out.put("effects", Wrap.setNbt((io.papermc.paper.registry.set.RegistryKeySet<?>) fields.get("removeEffects")));
        return out;
    }, stored -> {
        Map<String, Object> fields = new LinkedHashMap<>();
        fields.put("removeEffects", Wrap.set(RegistryKey.MOB_EFFECT, mapOf(stored).get("effects")));
        return fields;
    });

    public static final Spec CLEAR_EFFECTS = new Spec(ConsumeEffect.ClearAllStatusEffects.class, null).nbt(fields -> {
        Map<String, Object> out = Nbt.compound();
        out.put("type", "minecraft:clear_all_effects");
        return out;
    }, stored -> new LinkedHashMap<>());

    public static final Spec PLAY_SOUND = new Spec(ConsumeEffect.PlaySound.class, null).nbt(fields -> {
        Map<String, Object> out = Nbt.compound();
        out.put("type", "minecraft:play_sound");
        Wrap.putKey(out, "sound", (Key) fields.get("sound"));
        return out;
    }, stored -> {
        Map<String, Object> fields = new LinkedHashMap<>();
        put(fields, "sound", mapOf(stored).get("sound"));
        return fields;
    });

    public static final Spec TELEPORT = new Spec(ConsumeEffect.TeleportRandomly.class, null).def("diameter", 16.0F)
        .def("directionalParticles", true).nbt(fields -> {
            Map<String, Object> out = Nbt.compound();
            out.put("type", "minecraft:teleport_randomly");
            out.put("diameter", fields.get("diameter"));
            out.put("directional_particles", Wrap.bool((Boolean) fields.get("directionalParticles")));
            return out;
        }, stored -> {
            Map<String, Object> map = mapOf(stored);
            Map<String, Object> fields = new LinkedHashMap<>();
            fields.put("diameter", real(map, "diameter", 16.0));
            fields.put("directionalParticles", Nbt.flag(map, "directional_particles", true));
            return fields;
        });

    static Object consumeEffect(Object stored) {
        Map<String, Object> map = mapOf(stored);
        switch (Nbt.string(map, "type", "")) {
            case "minecraft:apply_effects", "apply_effects":
                return APPLY_EFFECTS.read(map);
            case "minecraft:remove_effects", "remove_effects":
                return REMOVE_EFFECTS.read(map);
            case "minecraft:clear_all_effects", "clear_all_effects":
                return CLEAR_EFFECTS.read(map);
            case "minecraft:play_sound", "play_sound":
                return PLAY_SOUND.read(map);
            default:
                return TELEPORT.read(map);
        }
    }

    private static List<Object> effectsNbt(List<Object> effects) {
        List<Object> out = new ArrayList<>();
        for (Object effect : effects) {
            out.add(((Dyn.Value) effect).nbt());
        }
        return out;
    }

    private static List<Object> effectsOf(Object stored) {
        List<Object> out = new ArrayList<>();
        List<Object> list = Nbt.list(stored);
        for (Object entry : list == null ? List.<Object>of() : list) {
            out.add(consumeEffect(entry));
        }
        return out;
    }

    public static final Spec CONSUMABLE = new Spec(Consumable.class, Consumable.Builder.class)
        .def("consumeSeconds", 1.6F).def("animation", ItemUseAnimation.EAT)
        .def("sound", Key.key("minecraft:entity.generic.eat")).def("hasConsumeParticles", true)
        .set("effects", "consumeEffects").add("addEffect", "consumeEffects").addAll("addEffects", "consumeEffects")
        .nbt(fields -> {
            Map<String, Object> out = Nbt.compound();
            out.put("consume_seconds", fields.get("consumeSeconds"));
            out.put("animation", lower((ItemUseAnimation) fields.get("animation")));
            Wrap.putKey(out, "sound", (Key) fields.get("sound"));
            out.put("has_consume_particles", Wrap.bool((Boolean) fields.get("hasConsumeParticles")));
            out.put("on_consume_effects", effectsNbt(Dyn.list(fields, "consumeEffects")));
            return out;
        }, stored -> {
            Map<String, Object> map = mapOf(stored);
            Map<String, Object> fields = new LinkedHashMap<>();
            fields.put("consumeSeconds", real(map, "consume_seconds", 1.6));
            fields.put("animation", animation(Nbt.string(map, "animation", "eat")));
            put(fields, "sound", map.get("sound"));
            fields.put("hasConsumeParticles", Nbt.flag(map, "has_consume_particles", true));
            fields.put("consumeEffects", effectsOf(map.get("on_consume_effects")));
            return fields;
        });

    private static ItemUseAnimation animation(String name) {
        try {
            return ItemUseAnimation.valueOf(name.toUpperCase(Locale.ROOT));
        } catch (IllegalArgumentException unknown) {
            return ItemUseAnimation.EAT;
        }
    }

    public static final Spec DEATH_PROTECTION = new Spec(DeathProtection.class, DeathProtection.Builder.class)
        .add("addEffect", "deathEffects").addAll("addEffects", "deathEffects").nbt(fields -> {
            Map<String, Object> out = Nbt.compound();
            out.put("death_effects", effectsNbt(Dyn.list(fields, "deathEffects")));
            return out;
        }, stored -> {
            Map<String, Object> fields = new LinkedHashMap<>();
            fields.put("deathEffects", effectsOf(mapOf(stored).get("death_effects")));
            return fields;
        });

    public static final Spec DISPLAY_DEFAULT = new Spec(AttributeModifierDisplay.Default.class, null).nbt(fields -> {
        Map<String, Object> out = Nbt.compound();
        out.put("type", "default");
        return out;
    }, stored -> new LinkedHashMap<>());

    public static final Spec DISPLAY_HIDDEN = new Spec(AttributeModifierDisplay.Hidden.class, null).nbt(fields -> {
        Map<String, Object> out = Nbt.compound();
        out.put("type", "hidden");
        return out;
    }, stored -> new LinkedHashMap<>());

    public static final Spec DISPLAY_OVERRIDE = new Spec(AttributeModifierDisplay.OverrideText.class, null)
        .nbt(fields -> {
            Map<String, Object> out = Nbt.compound();
            out.put("type", "override");
            out.put("value", Texts.toNbt((Component) fields.get("text")));
            return out;
        }, stored -> {
            Map<String, Object> fields = new LinkedHashMap<>();
            fields.put("text", Texts.fromNbt(mapOf(stored).get("value")));
            return fields;
        });

    static AttributeModifierDisplay display(Object stored) {
        Map<String, Object> map = mapOf(stored);
        return (AttributeModifierDisplay) switch (Nbt.string(map, "type", "default")) {
            case "hidden" -> DISPLAY_HIDDEN.read(map);
            case "override" -> DISPLAY_OVERRIDE.read(map);
            default -> DISPLAY_DEFAULT.read(map);
        };
    }

    public static AttributeModifierDisplay.OverrideText override(ComponentLike text) {
        Map<String, Object> fields = new LinkedHashMap<>();
        fields.put("text", text.asComponent());
        return (AttributeModifierDisplay.OverrideText) DISPLAY_OVERRIDE.value(fields);
    }

    public static final Spec ENTRY = new Spec(ItemAttributeModifiers.Entry.class, null)
        .call("getGroup", (fields, args) -> fields.get("group")).nbt(fields -> {
            Map<String, Object> out = MetaAttributes.entry((Attribute) fields.get("attribute"),
                (AttributeModifier) fields.get("modifier"));
            out.put("slot", fields.get("group").toString());
            AttributeModifierDisplay display = (AttributeModifierDisplay) fields.get("display");
            if (!(display instanceof AttributeModifierDisplay.Default)) {
                out.put("display", ((Dyn.Value) display).nbt());
            }
            return out;
        }, stored -> {
            Map<String, Object> map = mapOf(stored);
            AttributeModifier modifier = MetaAttributes.modifier(map);
            Map<String, Object> fields = new LinkedHashMap<>();
            fields.put("attribute", MetaAttributes.attribute(Nbt.string(map, "type", "")));
            fields.put("modifier", modifier);
            fields.put("group", modifier.getSlotGroup());
            fields.put("display", map.containsKey("display") ? display(map.get("display")) : DISPLAY_DEFAULT.value(
                new LinkedHashMap<>()));
            return fields;
        });

    public static final Spec MODIFIERS = new Spec(ItemAttributeModifiers.class, ItemAttributeModifiers.Builder.class)
        .op("addModifier", (fields, args) -> {
            Attribute attribute = (Attribute) args[0];
            AttributeModifier modifier = (AttributeModifier) args[1];
            EquipmentSlotGroup group = modifier.getSlotGroup();
            AttributeModifierDisplay display = null;
            for (int at = 2; at < args.length; at++) {
                if (args[at] instanceof EquipmentSlotGroup given) {
                    group = given;
                } else if (args[at] instanceof AttributeModifierDisplay given) {
                    display = given;
                }
            }
            Map<String, Object> entry = new LinkedHashMap<>();
            entry.put("attribute", attribute);
            entry.put("modifier", group == modifier.getSlotGroup() ? modifier : new AttributeModifier(modifier.getKey(),
                modifier.getAmount(), modifier.getOperation(), group));
            entry.put("group", group);
            entry.put("display", display != null ? display : DISPLAY_DEFAULT.value(new LinkedHashMap<>()));
            Dyn.list(fields, "modifiers").add(ENTRY.value(entry));
        }).nbt(fields -> {
            List<Object> out = new ArrayList<>();
            for (Object entry : Dyn.list(fields, "modifiers")) {
                out.add(((Dyn.Value) entry).nbt());
            }
            return out;
        }, stored -> {
            Map<String, Object> fields = new LinkedHashMap<>();
            List<Object> entries = new ArrayList<>();
            List<Object> list = Nbt.list(stored);
            for (Object entry : list == null ? List.<Object>of() : list) {
                entries.add(ENTRY.read(entry));
            }
            fields.put("modifiers", entries);
            return fields;
        });

    public static final Spec WEAPON = new Spec(Weapon.class, Weapon.Builder.class).def("itemDamagePerAttack", 1)
        .def("disableBlockingForSeconds", 0.0F).nbt(fields -> {
            Map<String, Object> out = Nbt.compound();
            out.put("item_damage_per_attack", fields.get("itemDamagePerAttack"));
            out.put("disable_blocking_for_seconds", fields.get("disableBlockingForSeconds"));
            return out;
        }, stored -> {
            Map<String, Object> map = mapOf(stored);
            Map<String, Object> fields = new LinkedHashMap<>();
            fields.put("itemDamagePerAttack", Nbt.integer(map, "item_damage_per_attack", 1));
            fields.put("disableBlockingForSeconds", real(map, "disable_blocking_for_seconds", 0));
            return fields;
        });

    public static final Spec ATTACK_RANGE = new Spec(AttackRange.class, AttackRange.Builder.class)
        .def("minReach", 0.0F).def("maxReach", 3.0F).def("minCreativeReach", 0.0F).def("maxCreativeReach", 5.0F)
        .def("hitboxMargin", 0.3F).def("mobFactor", 1.0F).nbt(fields -> {
            Map<String, Object> out = Nbt.compound();
            out.put("min_reach", fields.get("minReach"));
            out.put("max_reach", fields.get("maxReach"));
            out.put("min_creative_reach", fields.get("minCreativeReach"));
            out.put("max_creative_reach", fields.get("maxCreativeReach"));
            out.put("hitbox_margin", fields.get("hitboxMargin"));
            out.put("mob_factor", fields.get("mobFactor"));
            return out;
        }, stored -> {
            Map<String, Object> map = mapOf(stored);
            Map<String, Object> fields = new LinkedHashMap<>();
            fields.put("minReach", real(map, "min_reach", 0));
            fields.put("maxReach", real(map, "max_reach", 3));
            fields.put("minCreativeReach", real(map, "min_creative_reach", 0));
            fields.put("maxCreativeReach", real(map, "max_creative_reach", 5));
            fields.put("hitboxMargin", real(map, "hitbox_margin", 0.3));
            fields.put("mobFactor", real(map, "mob_factor", 1));
            return fields;
        });

    public static final Spec USE_EFFECTS = new Spec(UseEffects.class, UseEffects.Builder.class).def("canSprint", false)
        .def("interactVibrations", true).def("speedMultiplier", 0.2F).nbt(fields -> {
            Map<String, Object> out = Nbt.compound();
            out.put("can_sprint", Wrap.bool((Boolean) fields.get("canSprint")));
            out.put("interact_vibrations", Wrap.bool((Boolean) fields.get("interactVibrations")));
            out.put("speed_multiplier", fields.get("speedMultiplier"));
            return out;
        }, stored -> {
            Map<String, Object> map = mapOf(stored);
            Map<String, Object> fields = new LinkedHashMap<>();
            fields.put("canSprint", Nbt.flag(map, "can_sprint", false));
            fields.put("interactVibrations", Nbt.flag(map, "interact_vibrations", true));
            fields.put("speedMultiplier", real(map, "speed_multiplier", 0.2));
            return fields;
        });

    public static final Spec PIERCING = new Spec(PiercingWeapon.class, PiercingWeapon.Builder.class)
        .def("dealsKnockback", true).def("dismounts", false).nbt(fields -> {
            Map<String, Object> out = Nbt.compound();
            out.put("deals_knockback", Wrap.bool((Boolean) fields.get("dealsKnockback")));
            out.put("dismounts", Wrap.bool((Boolean) fields.get("dismounts")));
            Wrap.putKey(out, "sound", (Key) fields.get("sound"));
            Wrap.putKey(out, "hit_sound", (Key) fields.get("hitSound"));
            return out;
        }, stored -> {
            Map<String, Object> map = mapOf(stored);
            Map<String, Object> fields = new LinkedHashMap<>();
            fields.put("dealsKnockback", Nbt.flag(map, "deals_knockback", true));
            fields.put("dismounts", Nbt.flag(map, "dismounts", false));
            put(fields, "sound", map.get("sound"));
            put(fields, "hitSound", map.get("hit_sound"));
            return fields;
        });

    public static final Spec CONDITION = new Spec(KineticWeapon.Condition.class, null).nbt(fields -> {
        Map<String, Object> out = Nbt.compound();
        out.put("max_duration_ticks", fields.get("maxDurationTicks"));
        out.put("min_speed", fields.get("minSpeed"));
        out.put("min_relative_speed", fields.get("minRelativeSpeed"));
        return out;
    }, stored -> {
        Map<String, Object> map = mapOf(stored);
        Map<String, Object> fields = new LinkedHashMap<>();
        fields.put("maxDurationTicks", Nbt.integer(map, "max_duration_ticks", 0));
        fields.put("minSpeed", real(map, "min_speed", 0));
        fields.put("minRelativeSpeed", real(map, "min_relative_speed", 0));
        return fields;
    });

    public static final Spec KINETIC = new Spec(KineticWeapon.class, KineticWeapon.Builder.class)
        .def("contactCooldownTicks", 10).def("delayTicks", 0).def("forwardMovement", 0.0F)
        .def("damageMultiplier", 1.0F).nbt(fields -> {
            Map<String, Object> out = Nbt.compound();
            out.put("contact_cooldown_ticks", fields.get("contactCooldownTicks"));
            out.put("delay_ticks", fields.get("delayTicks"));
            conditionOut(out, "dismount_conditions", fields.get("dismountConditions"));
            conditionOut(out, "knockback_conditions", fields.get("knockbackConditions"));
            conditionOut(out, "damage_conditions", fields.get("damageConditions"));
            out.put("forward_movement", fields.get("forwardMovement"));
            out.put("damage_multiplier", fields.get("damageMultiplier"));
            Wrap.putKey(out, "sound", (Key) fields.get("sound"));
            Wrap.putKey(out, "hit_sound", (Key) fields.get("hitSound"));
            return out;
        }, stored -> {
            Map<String, Object> map = mapOf(stored);
            Map<String, Object> fields = new LinkedHashMap<>();
            fields.put("contactCooldownTicks", Nbt.integer(map, "contact_cooldown_ticks", 10));
            fields.put("delayTicks", Nbt.integer(map, "delay_ticks", 0));
            conditionIn(fields, "dismountConditions", map.get("dismount_conditions"));
            conditionIn(fields, "knockbackConditions", map.get("knockback_conditions"));
            conditionIn(fields, "damageConditions", map.get("damage_conditions"));
            fields.put("forwardMovement", real(map, "forward_movement", 0));
            fields.put("damageMultiplier", real(map, "damage_multiplier", 1));
            put(fields, "sound", map.get("sound"));
            put(fields, "hitSound", map.get("hit_sound"));
            return fields;
        });

    private static void conditionOut(Map<String, Object> out, String name, Object condition) {
        if (condition != null) {
            out.put(name, ((Dyn.Value) condition).nbt());
        }
    }

    private static void conditionIn(Map<String, Object> fields, String name, Object stored) {
        if (stored != null) {
            fields.put(name, CONDITION.read(stored));
        }
    }

    public static Object condition(int maxDurationTicks, float minSpeed, float minRelativeSpeed) {
        Map<String, Object> fields = new LinkedHashMap<>();
        fields.put("maxDurationTicks", maxDurationTicks);
        fields.put("minSpeed", minSpeed);
        fields.put("minRelativeSpeed", minRelativeSpeed);
        return CONDITION.value(fields);
    }

    public static final Spec SWING = new Spec(SwingAnimation.class, SwingAnimation.Builder.class)
        .def("type", SwingAnimation.Animation.WHACK).def("duration", 6).nbt(fields -> {
            Map<String, Object> out = Nbt.compound();
            out.put("type", lower((SwingAnimation.Animation) fields.get("type")));
            out.put("duration", fields.get("duration"));
            return out;
        }, stored -> {
            Map<String, Object> map = mapOf(stored);
            Map<String, Object> fields = new LinkedHashMap<>();
            try {
                fields.put("type", SwingAnimation.Animation.valueOf(Nbt.string(map, "type", "whack")
                    .toUpperCase(Locale.ROOT)));
            } catch (IllegalArgumentException unknown) {
                fields.put("type", SwingAnimation.Animation.WHACK);
            }
            fields.put("duration", Nbt.integer(map, "duration", 6));
            return fields;
        });

    public static final Spec REDUCTION = new Spec(DamageReduction.class, DamageReduction.Builder.class)
        .def("horizontalBlockingAngle", 90.0F).nbt(fields -> {
            Map<String, Object> out = Nbt.compound();
            out.put("horizontal_blocking_angle", fields.get("horizontalBlockingAngle"));
            if (fields.get("type") != null) {
                out.put("type", Wrap.setNbt((io.papermc.paper.registry.set.RegistryKeySet<?>) fields.get("type")));
            }
            out.put("base", fields.get("base"));
            out.put("factor", fields.get("factor"));
            return out;
        }, stored -> {
            Map<String, Object> map = mapOf(stored);
            Map<String, Object> fields = new LinkedHashMap<>();
            fields.put("horizontalBlockingAngle", real(map, "horizontal_blocking_angle", 90));
            if (map.get("type") != null) {
                fields.put("type", Wrap.set(RegistryKey.DAMAGE_TYPE, map.get("type")));
            }
            fields.put("base", real(map, "base", 0));
            fields.put("factor", real(map, "factor", 1));
            return fields;
        });

    public static final Spec ITEM_DAMAGE = new Spec(ItemDamageFunction.class, ItemDamageFunction.Builder.class)
        .call("damageToApply", (fields, args) -> {
            float damage = ((Number) args[0]).floatValue();
            float threshold = ((Number) fields.get("threshold")).floatValue();
            return damage < threshold ? 0 : (int) Math.floor(((Number) fields.get("base")).floatValue()
                + ((Number) fields.get("factor")).floatValue() * damage);
        }).nbt(fields -> {
            Map<String, Object> out = Nbt.compound();
            out.put("threshold", fields.get("threshold"));
            out.put("base", fields.get("base"));
            out.put("factor", fields.get("factor"));
            return out;
        }, stored -> {
            Map<String, Object> map = mapOf(stored);
            Map<String, Object> fields = new LinkedHashMap<>();
            fields.put("threshold", real(map, "threshold", 1));
            fields.put("base", real(map, "base", 0));
            fields.put("factor", real(map, "factor", 1));
            return fields;
        });

    public static final Spec BLOCKS_ATTACKS = new Spec(BlocksAttacks.class, BlocksAttacks.Builder.class)
        .def("blockDelaySeconds", 0.0F).def("disableCooldownScale", 1.0F)
        .def("itemDamage", ITEM_DAMAGE.value(new LinkedHashMap<>(Map.of("threshold", 1.0F, "base", 0.0F, "factor", 1.0F))))
        .add("addDamageReduction", "damageReductions")
        .set("damageReductions", "damageReductions").nbt(fields -> {
            Map<String, Object> out = Nbt.compound();
            out.put("block_delay_seconds", fields.get("blockDelaySeconds"));
            out.put("disable_cooldown_scale", fields.get("disableCooldownScale"));
            List<Object> reductions = new ArrayList<>();
            for (Object reduction : Dyn.list(fields, "damageReductions")) {
                reductions.add(((Dyn.Value) reduction).nbt());
            }
            out.put("damage_reductions", reductions);
            if (fields.get("itemDamage") != null) {
                out.put("item_damage", ((Dyn.Value) fields.get("itemDamage")).nbt());
            }
            if (fields.get("bypassedBy") != null) {
                out.put("bypassed_by",
                    Wrap.setNbt((io.papermc.paper.registry.set.RegistryKeySet<?>) fields.get("bypassedBy")));
            }
            Wrap.putKey(out, "block_sound", (Key) fields.get("blockSound"));
            Wrap.putKey(out, "disabled_sound", (Key) fields.get("disableSound"));
            return out;
        }, stored -> {
            Map<String, Object> map = mapOf(stored);
            Map<String, Object> fields = new LinkedHashMap<>();
            fields.put("blockDelaySeconds", real(map, "block_delay_seconds", 0));
            fields.put("disableCooldownScale", real(map, "disable_cooldown_scale", 1));
            List<Object> reductions = new ArrayList<>();
            List<Object> list = Nbt.list(map.get("damage_reductions"));
            for (Object reduction : list == null ? List.<Object>of() : list) {
                reductions.add(REDUCTION.read(reduction));
            }
            fields.put("damageReductions", reductions);
            if (map.get("item_damage") != null) {
                fields.put("itemDamage", ITEM_DAMAGE.read(map.get("item_damage")));
            }
            if (map.get("bypassed_by") != null) {
                fields.put("bypassedBy", Wrap.set(RegistryKey.DAMAGE_TYPE, map.get("bypassed_by")));
            }
            put(fields, "blockSound", map.get("block_sound"));
            put(fields, "disableSound", map.get("disabled_sound"));
            return fields;
        });
}
