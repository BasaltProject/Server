package org.basaltmc.bukkit.item;

import io.papermc.paper.inventory.tooltip.TooltipContext;
import java.text.DecimalFormat;
import java.text.DecimalFormatSymbols;
import java.util.ArrayList;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Locale;
import java.util.Map;
import net.kyori.adventure.text.Component;
import net.kyori.adventure.text.format.NamedTextColor;
import net.kyori.adventure.text.format.TextDecoration;
import org.bukkit.entity.Player;
import org.bukkit.inventory.ItemStack;
import org.bukkit.potion.PotionEffect;
import org.bukkit.potion.PotionEffectType;

/** The lines a stack lists under its name, in the order the game's own tooltip gives them. */
public final class Tooltips {
    private static final String[] kSlotGroups = {"any", "mainhand", "offhand", "hand", "feet", "legs", "chest", "head",
        "armor", "body", "saddle"};
    private static final java.util.Set<String> kNeutralAttributes = java.util.Set.of("gravity", "scale",
        "waypoint_transmit_range", "waypoint_receive_range");
    private static final java.util.Set<String> kNegativeAttributes = java.util.Set.of("burning_time",
        "fall_damage_multiplier");
    private static final String[][] kEffectModifiers = {
        {"speed", "movement_speed", "0.2", "2"}, {"slowness", "movement_speed", "-0.15", "2"},
        {"haste", "attack_speed", "0.1", "2"}, {"mining_fatigue", "attack_speed", "-0.1", "2"},
        {"strength", "attack_damage", "3.0", "0"}, {"weakness", "attack_damage", "-4.0", "0"},
        {"jump_boost", "safe_fall_distance", "1.0", "0"}, {"health_boost", "max_health", "4.0", "0"},
        {"absorption", "max_absorption", "4.0", "0"}, {"luck", "luck", "1.0", "0"}, {"unluck", "luck", "-1.0", "0"},
        {"invisibility", "waypoint_transmit_range", "-1.0", "2"}};

    private Tooltips() {}

    private static DecimalFormat format() {
        return new DecimalFormat("#.##", DecimalFormatSymbols.getInstance(Locale.ROOT));
    }

    private static boolean hidden(Map<String, Object> components, String component) {
        Map<String, Object> display = Nbt.map(components.get("minecraft:tooltip_display"));
        if (display == null) {
            return false;
        }
        if (display.get("hidden_components") instanceof List<?> names) {
            return names.contains(component);
        }
        return false;
    }

    private static Component level(int level, int max) {
        if (level == 1 && max == 1) {
            return Component.empty();
        }
        return Component.text(" ").append(Component.translatable("enchantment.level." + level));
    }

    private static NamedTextColor rarityColor(Map<String, Object> components, boolean enchanted) {
        String rarity = Nbt.string(components, "minecraft:rarity", "common");
        if (enchanted && (rarity.equals("common") || rarity.equals("uncommon"))) {
            rarity = "rare";
        } else if (enchanted && rarity.equals("rare")) {
            rarity = "epic";
        }
        return switch (rarity) {
            case "uncommon" -> NamedTextColor.YELLOW;
            case "rare" -> NamedTextColor.AQUA;
            case "epic" -> NamedTextColor.LIGHT_PURPLE;
            default -> NamedTextColor.WHITE;
        };
    }

    private static void enchantmentLines(List<Component> lines, Map<org.bukkit.enchantments.Enchantment, Integer> held) {
        for (Map.Entry<org.bukkit.enchantments.Enchantment, Integer> entry : held.entrySet()) {
            org.bukkit.enchantments.Enchantment enchantment = entry.getKey();
            Component one = Component.translatable(enchantment.translationKey())
                .append(level(entry.getValue(), enchantment.getMaxLevel()));
            lines.add(one.colorIfAbsent(enchantment.isCursed() ? NamedTextColor.RED : NamedTextColor.GRAY));
        }
    }

    private static String sign(double amount) {
        return amount > 0 ? "plus" : "take";
    }

    private static Component modifierLine(String attribute, String operation, double amount, boolean positiveBlue) {
        double shown = operation.equals("0") ? amount : amount * 100.0;
        String text = format().format(Math.abs(shown));
        return Component.translatable("attribute.modifier." + sign(amount) + "." + operation, Component.text(text),
            Component.translatable("attribute.name." + attribute));
    }

    private static NamedTextColor sentiment(String attribute, boolean increase) {
        if (kNeutralAttributes.contains(attribute)) {
            return NamedTextColor.GRAY;
        }
        boolean negative = kNegativeAttributes.contains(attribute);
        return increase != negative ? NamedTextColor.BLUE : NamedTextColor.RED;
    }

    private static String operationId(String name) {
        return switch (name) {
            case "add_multiplied_base" -> "1";
            case "add_multiplied_total" -> "2";
            default -> "0";
        };
    }

    private static String pathOf(String id) {
        return id.substring(id.indexOf(':') + 1);
    }

    private static void potionLines(List<Component> lines, ItemStack stack, Map<String, Object> components) {
        List<PotionEffect> effects = new ArrayList<>();
        if (stack.getItemMeta() instanceof org.bukkit.inventory.meta.PotionMeta meta) {
            if (meta.getBasePotionType() != null) {
                effects.addAll(meta.getBasePotionType().getPotionEffects());
            }
            effects.addAll(meta.getCustomEffects());
        }
        double scale = Nbt.real(components, "minecraft:potion_duration_scale", 1.0);
        List<String[]> modifiers = new ArrayList<>();
        for (PotionEffect effect : effects) {
            PotionEffectType type = effect.getType();
            String path = type.getKey().getKey();
            int amplifier = effect.getAmplifier();
            for (String[] template : kEffectModifiers) {
                if (template[0].equals(path)) {
                    modifiers.add(new String[] {template[1], template[3],
                        Double.toString(Double.parseDouble(template[2]) * (amplifier + 1))});
                }
            }
            Component line = Component.translatable(type.translationKey());
            if (amplifier > 0) {
                line = Component.translatable("potion.withAmplifier", line,
                    Component.translatable("potion.potency." + amplifier));
            }
            boolean infinite = effect.getDuration() < 0;
            if (infinite || effect.getDuration() > 20) {
                Component time;
                if (infinite) {
                    time = Component.translatable("effect.duration.infinite");
                } else {
                    int seconds = (int) Math.floor((float) Math.floor(effect.getDuration() * scale) / 20.0F);
                    int minutes = seconds / 60;
                    seconds %= 60;
                    int hours = minutes / 60;
                    minutes %= 60;
                    time = Component.text(hours > 0 ? String.format(Locale.ROOT, "%02d:%02d:%02d", hours, minutes, seconds)
                        : String.format(Locale.ROOT, "%02d:%02d", minutes, seconds));
                }
                line = Component.translatable("potion.withDuration", line, time);
            }
            lines.add(line.color(type.getEffectCategory() == PotionEffectType.Category.HARMFUL ? NamedTextColor.RED
                : NamedTextColor.BLUE));
        }
        if (effects.isEmpty()) {
            lines.add(Component.translatable("effect.none").color(NamedTextColor.GRAY));
        }
        if (!modifiers.isEmpty()) {
            lines.add(Component.empty());
            lines.add(Component.translatable("potion.whenDrank").color(NamedTextColor.DARK_PURPLE));
            for (String[] modifier : modifiers) {
                double amount = Double.parseDouble(modifier[2]);
                if (amount != 0.0) {
                    lines.add(modifierLine(modifier[0], modifier[1], amount, true)
                        .color(amount > 0 ? NamedTextColor.BLUE : NamedTextColor.RED));
                }
            }
        }
    }

    private static void attributeLines(List<Component> lines, Map<String, Object> components, Player player) {
        List<Object> entries = Nbt.list(components.get("minecraft:attribute_modifiers"));
        if (entries == null) {
            return;
        }
        for (String group : kSlotGroups) {
            boolean first = true;
            for (Object raw : entries) {
                Map<String, Object> entry = Nbt.map(raw);
                if (entry == null || !Nbt.string(entry, "slot", "any").equals(group)) {
                    continue;
                }
                Map<String, Object> display = Nbt.map(entry.get("display"));
                String kind = display == null ? "default" : Nbt.string(display, "type", "default");
                if (kind.equals("hidden")) {
                    continue;
                }
                if (first) {
                    lines.add(Component.empty());
                    lines.add(Component.translatable("item.modifiers." + group).color(NamedTextColor.GRAY));
                    first = false;
                }
                if (kind.equals("override_text")) {
                    lines.add(Texts.fromNbt(display.get("value")));
                    continue;
                }
                String attribute = pathOf(Nbt.string(entry, "type", ""));
                String operationName = Nbt.string(entry, "operation", "add_value");
                String operation = operationId(operationName);
                double amount = Nbt.real(entry, "amount", 0.0);
                String id = Nbt.string(entry, "id", "");
                boolean withBase = false;
                if (player != null && id.equals("minecraft:base_attack_damage")) {
                    amount += baseOf(player, "attack_damage", 1.0);
                    withBase = true;
                } else if (player != null && id.equals("minecraft:base_attack_speed")) {
                    amount += baseOf(player, "attack_speed", 4.0);
                    withBase = true;
                }
                double shown;
                if (operation.equals("0")) {
                    shown = attribute.equals("knockback_resistance") ? amount * 10.0 : amount;
                } else {
                    shown = amount * 100.0;
                }
                Component name = Component.translatable("attribute.name." + attribute);
                if (withBase) {
                    lines.add(Component.text(" ").append(Component.translatable("attribute.modifier.equals." + operation,
                        Component.text(format().format(shown)), name)).color(NamedTextColor.DARK_GREEN));
                } else if (amount > 0) {
                    lines.add(Component.translatable("attribute.modifier.plus." + operation,
                        Component.text(format().format(shown)), name).color(sentiment(attribute, true)));
                } else if (amount < 0) {
                    lines.add(Component.translatable("attribute.modifier.take." + operation,
                        Component.text(format().format(-shown)), name).color(sentiment(attribute, false)));
                }
            }
        }
    }

    private static double baseOf(Player player, String attribute, double fallback) {
        org.bukkit.attribute.Attribute found = org.bukkit.Registry.ATTRIBUTE.get(
            org.bukkit.NamespacedKey.minecraft(attribute));
        org.bukkit.attribute.AttributeInstance instance = found == null ? null : player.getAttribute(found);
        return instance == null ? fallback : instance.getBaseValue();
    }

    private static Map<String, Object> merged(ItemStack stack, Map<String, Object> own) {
        Map<String, Object> all = new LinkedHashMap<>(ItemDefaults.of(stack.getType().getKey().toString()));
        all.putAll(own);
        return all;
    }

    public static List<Component> lines(ItemStack stack, Map<String, Object> own, Component name,
            TooltipContext context, Player player) {
        Map<String, Object> components = merged(stack, own);
        List<Component> lines = new ArrayList<>();
        Map<String, Object> display = Nbt.map(components.get("minecraft:tooltip_display"));
        if (display != null && Nbt.flag(display, "hide_tooltip", false)) {
            return lines;
        }
        boolean enchanted = !stack.getEnchantments().isEmpty();
        Component title = name.colorIfAbsent(rarityColor(components, enchanted));
        if (components.containsKey("minecraft:custom_name")) {
            title = title.decorationIfAbsent(TextDecoration.ITALIC, TextDecoration.State.TRUE);
        }
        lines.add(title);
        if (stack.getItemMeta() instanceof org.bukkit.inventory.meta.BookMeta book
            && components.containsKey("minecraft:written_book_content")) {
            if (book.getAuthor() != null && !book.getAuthor().isBlank()) {
                lines.add(Component.translatable("book.byAuthor", Component.text(book.getAuthor()))
                    .color(NamedTextColor.GRAY));
            }
            Map<String, Object> content = Nbt.map(components.get("minecraft:written_book_content"));
            int generation = content == null ? 0 : Nbt.integer(content, "generation", 0);
            lines.add(Component.translatable("book.generation." + generation).color(NamedTextColor.GRAY));
        }
        if (components.containsKey("minecraft:potion_contents") && !hidden(components, "minecraft:potion_contents")) {
            potionLines(lines, stack, components);
        }
        if (stack.getItemMeta() instanceof org.bukkit.inventory.meta.EnchantmentStorageMeta stored
            && !hidden(components, "minecraft:stored_enchantments")) {
            enchantmentLines(lines, stored.getStoredEnchants());
        }
        if (!hidden(components, "minecraft:enchantments")) {
            enchantmentLines(lines, stack.getEnchantments());
        }
        if (components.containsKey("minecraft:dyed_color") && !hidden(components, "minecraft:dyed_color")) {
            Object dyed = components.get("minecraft:dyed_color");
            Map<String, Object> asMap = Nbt.map(dyed);
            int rgb = asMap != null ? Nbt.integer(asMap, "rgb", 0) : dyed instanceof Number number ? number.intValue() : 0;
            lines.add(context.isAdvanced()
                ? Component.translatable("item.color", Component.text(String.format(Locale.ROOT, "#%06X", rgb & 0xFFFFFF)))
                    .color(NamedTextColor.GRAY)
                : Component.translatable("item.dyed").color(NamedTextColor.GRAY).decorate(TextDecoration.ITALIC));
        }
        if (stack.lore() != null) {
            for (Component line : stack.lore()) {
                lines.add(line.colorIfAbsent(NamedTextColor.DARK_PURPLE).decorationIfAbsent(TextDecoration.ITALIC,
                    TextDecoration.State.TRUE));
            }
        }
        if (!hidden(components, "minecraft:attribute_modifiers")) {
            attributeLines(lines, components, player);
        }
        if (!hidden(components, "minecraft:unbreakable") && components.containsKey("minecraft:unbreakable")) {
            lines.add(Component.translatable("item.unbreakable").color(NamedTextColor.BLUE));
        }
        if (context.isAdvanced()) {
            int max = stack.getType().getMaxDurability();
            int wear = Nbt.integer(components, "minecraft:damage", 0);
            if (max > 0 && wear > 0 && !components.containsKey("minecraft:unbreakable")
                && !hidden(components, "minecraft:damage")) {
                lines.add(Component.translatable("item.durability", Component.text(max - wear), Component.text(max)));
            }
            lines.add(Component.text(stack.getType().getKey().toString()).color(NamedTextColor.DARK_GRAY));
            if (!components.isEmpty()) {
                lines.add(Component.translatable("item.components", Component.text(components.size()))
                    .color(NamedTextColor.DARK_GRAY));
            }
        }
        return lines;
    }
}
