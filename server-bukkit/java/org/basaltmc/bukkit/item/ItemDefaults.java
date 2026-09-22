package org.basaltmc.bukkit.item;

import java.io.IOException;
import java.io.InputStream;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.Collections;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import net.kyori.adventure.text.Component;
import org.basaltmc.bukkit.Natives;

public final class ItemDefaults {
    private static final Map<String, Map<String, Object>> cache = new HashMap<>();
    private static volatile Map<String, String> rarities;

    private ItemDefaults() {}

    public static synchronized void invalidate() {
        cache.clear();
        rarities = null;
    }

    public static synchronized Map<String, Object> of(String id) {
        Map<String, Object> found = cache.get(id);
        if (found == null) {
            found = Collections.unmodifiableMap(build(id));
            cache.put(id, found);
        }
        return found;
    }

    public static String rarity(String id) {
        if (rarities == null) {
            synchronized (ItemDefaults.class) {
                if (rarities == null) {
                    Map<String, String> read = new HashMap<>();
                    try (InputStream in = ItemDefaults.class.getResourceAsStream("/item_rarity.txt")) {
                        if (in != null) {
                            for (String line : new String(in.readAllBytes(), StandardCharsets.UTF_8).split("\\n")) {
                                String[] pair = line.strip().split(" ");
                                if (pair.length == 2) {
                                    read.put("minecraft:" + pair[0], pair[1]);
                                }
                            }
                        }
                    } catch (IOException failure) {
                        Natives.log(2, "Bukkit", "Could not read the item rarity list: " + failure);
                    }
                    rarities = read;
                }
            }
        }
        return rarities.getOrDefault(id, "common");
    }

    private static String bare(String id) {
        return id.substring(id.indexOf(':') + 1);
    }

    private static String equipSound(String name) {
        for (String material : new String[] {"leather", "chainmail", "iron", "golden", "diamond", "netherite",
            "turtle", "copper"}) {
            if (name.startsWith(material + "_")) {
                return "minecraft:item.armor.equip_" + switch (material) {
                    case "chainmail" -> "chain";
                    case "golden" -> "gold";
                    default -> material;
                };
            }
        }
        return name.equals("elytra") ? "minecraft:item.armor.equip_elytra" : "minecraft:item.armor.equip_generic";
    }

    private static Map<String, Object> modifier(String attribute, String id, double amount, String slot) {
        Map<String, Object> entry = Nbt.compound();
        entry.put("type", attribute);
        entry.put("id", id);
        entry.put("amount", amount);
        entry.put("operation", "add_value");
        entry.put("slot", slot);
        return entry;
    }

    private static String armourType(String slot) {
        return switch (slot) {
            case "head" -> "helmet";
            case "chest" -> "chestplate";
            case "legs" -> "leggings";
            case "feet" -> "boots";
            default -> "body";
        };
    }

    private static Map<String, Object> build(String id) {
        ItemData data = ItemData.of(id);
        Map<String, Object> out = Nbt.compound();
        String name = bare(id);
        out.put("minecraft:max_stack_size", data.maxStackSize());
        if (data.maxDamage() > 0) {
            out.put("minecraft:max_damage", data.maxDamage());
            out.put("minecraft:damage", 0);
        }
        out.put("minecraft:lore", new ArrayList<Object>());
        out.put("minecraft:enchantments", Nbt.compound());
        out.put("minecraft:repair_cost", 0);
        out.put("minecraft:rarity", rarity(id));
        out.put("minecraft:item_model", id);
        out.put("minecraft:tooltip_display", Nbt.compound());
        String prefix = data.block() != null ? "block." : "item.";
        out.put("minecraft:item_name", Texts.toNbt(Component.translatable(prefix + "minecraft." + name)));
        List<Object> modifiers = new ArrayList<>();
        Map<String, Object> armor = data.armor();
        if (armor != null) {
            String slot = Nbt.string(armor, "slot", "chest");
            String modifierId = "minecraft:armor." + armourType(slot);
            modifiers.add(modifier("minecraft:armor", modifierId, Nbt.real(armor, "armor", 0), slot));
            modifiers.add(modifier("minecraft:armor_toughness", modifierId, Nbt.real(armor, "toughness", 0), slot));
            if (Nbt.real(armor, "knockback_resistance", 0) > 0) {
                modifiers.add(modifier("minecraft:knockback_resistance", modifierId,
                    Nbt.real(armor, "knockback_resistance", 0), slot));
            }
            Map<String, Object> equippable = Nbt.compound();
            equippable.put("slot", slot);
            equippable.put("equip_sound", equipSound(name));
            out.put("minecraft:equippable", equippable);
        }
        Map<String, Object> weapon = data.weapon();
        if (weapon != null) {
            modifiers.add(modifier("minecraft:attack_damage", "minecraft:base_attack_damage",
                Nbt.real(weapon, "attack_damage", 0), "mainhand"));
            modifiers.add(modifier("minecraft:attack_speed", "minecraft:base_attack_speed",
                Nbt.real(weapon, "attack_speed", 0), "mainhand"));
        }
        if (!modifiers.isEmpty()) {
            out.put("minecraft:attribute_modifiers", modifiers);
        }
        Map<String, Object> food = data.food();
        if (food != null) {
            Map<String, Object> edible = Nbt.compound();
            edible.put("nutrition", Nbt.integer(food, "nutrition", 0));
            edible.put("saturation", (float) Nbt.real(food, "saturation", 0));
            edible.put("can_always_eat", Nbt.bool(Nbt.flag(food, "can_always_eat", false)));
            out.put("minecraft:food", edible);
            Map<String, Object> consumable = Nbt.compound();
            consumable.put("consume_seconds", (float) Nbt.real(food, "consume_seconds", 1.6));
            out.put("minecraft:consumable", consumable);
        }
        Map<String, Object> tool = data.tool();
        if (tool != null) {
            Map<String, Object> copy = Nbt.deepCopy(tool);
            copy.put("default_mining_speed", 1.0F);
            out.put("minecraft:tool", copy);
        }
        if (data.useCooldownTicks() > 0) {
            Map<String, Object> cooldown = Nbt.compound();
            cooldown.put("seconds", data.useCooldownTicks() / 20.0F);
            out.put("minecraft:use_cooldown", cooldown);
        }
        return out;
    }
}
