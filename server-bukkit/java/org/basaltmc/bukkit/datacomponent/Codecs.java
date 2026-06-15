package org.basaltmc.bukkit.datacomponent;

import io.papermc.paper.inventory.ItemRarity;
import java.util.List;
import java.util.Map;
import net.kyori.adventure.key.Key;
import net.kyori.adventure.text.Component;
import org.basaltmc.bukkit.item.Convert;
import org.basaltmc.bukkit.item.Nbt;
import org.basaltmc.bukkit.item.Texts;
import org.bukkit.Color;
import org.bukkit.DyeColor;

/** Reads and writes the values of the components the Paper API can hold, between their stored form and objects. */
final class Codecs {
    private Codecs() {}

    static boolean supported(String name) {
        switch (name) {
            case "max_stack_size", "max_damage", "damage", "repair_cost", "enchantment_glint_override", "item_model",
                "tooltip_style", "note_block_sound", "custom_name", "item_name", "lore", "enchantments",
                "stored_enchantments", "custom_model_data", "dyed_color", "tooltip_display", "enchantable", "map_id",
                "ominous_bottle_amplifier", "use_cooldown", "food", "repairable", "damage_resistant", "use_remainder",
                "rarity", "dye", "base_color":
                return true;
            default:
                return false;
        }
    }

    static Object decode(String name, Object nbt) {
        switch (name) {
            case "max_stack_size", "max_damage", "damage", "repair_cost":
                return nbt instanceof Number number ? number.intValue() : 0;
            case "enchantment_glint_override":
                return nbt instanceof Number number && number.intValue() != 0;
            case "item_model", "tooltip_style", "note_block_sound":
                return Key.key(String.valueOf(nbt));
            case "custom_name", "item_name":
                return Texts.fromNbt(nbt);
            case "lore":
                return PaperValues.Lore.of(nbt);
            case "enchantments", "stored_enchantments":
                return PaperValues.Enchants.of(nbt);
            case "custom_model_data": {
                Map<String, Object> map = Nbt.map(nbt);
                return new PaperValues.Model(map == null ? Nbt.compound() : Nbt.deepCopy(map));
            }
            case "dyed_color":
                return new PaperValues.Dyed(Color.fromRGB(nbt instanceof Number number ? number.intValue() & 0xFFFFFF : 0));
            case "tooltip_display":
                return PaperValues.Tooltip.of(nbt);
            case "enchantable":
                return new PaperValues.EnchantableValue(Nbt.integer(Nbt.map(nbt), "value", 1));
            case "map_id":
                return new PaperValues.MapIdValue(nbt instanceof Number number ? number.intValue() : 0);
            case "ominous_bottle_amplifier":
                return new PaperValues.AmplifierValue(nbt instanceof Number number ? number.intValue() : 0);
            case "use_cooldown": {
                Map<String, Object> map = Nbt.map(nbt);
                String group = Nbt.string(map, "cooldown_group", null);
                return new PaperValues.Cooldown((float) Nbt.real(map, "seconds", 0), group == null ? null : Key.key(group));
            }
            case "food": {
                Map<String, Object> map = Nbt.map(nbt);
                return new PaperValues.Food(Nbt.integer(map, "nutrition", 0), (float) Nbt.real(map, "saturation", 0),
                    Nbt.flag(map, "can_always_eat", false));
            }
            case "repairable":
                return PaperValues.RepairableValue.of(nbt);
            case "damage_resistant":
                return PaperValues.DamageResistantValue.of(nbt);
            case "use_remainder":
                return PaperValues.RemainderValue.of(nbt);
            case "rarity":
                return ItemRarity.valueOf(String.valueOf(nbt).toUpperCase(java.util.Locale.ROOT));
            case "dye", "base_color":
                return Convert.dye(String.valueOf(nbt));
            default:
                throw org.basaltmc.bukkit.Unsupported.hit("data component " + name);
        }
    }

    static Object encode(String name, Object value) {
        if (value instanceof PaperValues.Stored stored) {
            return stored.nbt();
        }
        if (value instanceof Integer || value instanceof Long) {
            return ((Number) value).intValue();
        }
        if (value instanceof Boolean flag) {
            return Nbt.bool(flag);
        }
        if (value instanceof Key key) {
            return key.asString();
        }
        if (value instanceof Component component) {
            return Texts.toNbt(component);
        }
        if (value instanceof ItemRarity rarity) {
            return rarity.name().toLowerCase(java.util.Locale.ROOT);
        }
        if (value instanceof DyeColor color) {
            return Convert.dye(color);
        }
        if (value instanceof List<?> || value instanceof Map<?, ?>) {
            return value;
        }
        throw org.basaltmc.bukkit.Unsupported.hit("data component " + name + " of " + value.getClass().getName());
    }
}
