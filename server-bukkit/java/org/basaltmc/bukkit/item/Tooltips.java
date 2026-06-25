package org.basaltmc.bukkit.item;

import io.papermc.paper.inventory.tooltip.TooltipContext;
import java.util.ArrayList;
import java.util.List;
import java.util.Map;
import net.kyori.adventure.text.Component;
import net.kyori.adventure.text.format.NamedTextColor;
import net.kyori.adventure.text.format.TextDecoration;
import org.bukkit.inventory.ItemStack;

/** The lines a stack lists under its name: enchantments, lore, wear and, when asked for, the id. */
public final class Tooltips {
    private static final String[] kRomans = {"", "I", "II", "III", "IV", "V", "VI", "VII", "VIII", "IX", "X"};

    private Tooltips() {}

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

    public static List<Component> lines(ItemStack stack, Map<String, Object> components, Component name,
            TooltipContext context) {
        List<Component> lines = new ArrayList<>();
        Map<String, Object> display = Nbt.map(components.get("minecraft:tooltip_display"));
        if (display != null && Nbt.flag(display, "hide_tooltip", false)) {
            return lines;
        }
        NamedTextColor color = switch (Nbt.string(components, "minecraft:rarity", "common")) {
            case "uncommon" -> NamedTextColor.YELLOW;
            case "rare" -> NamedTextColor.AQUA;
            case "epic" -> NamedTextColor.LIGHT_PURPLE;
            default -> NamedTextColor.WHITE;
        };
        Component title = name.colorIfAbsent(color);
        if (components.containsKey("minecraft:custom_name")) {
            title = title.decorationIfAbsent(TextDecoration.ITALIC, TextDecoration.State.TRUE);
        }
        lines.add(title);
        if (!hidden(components, "minecraft:enchantments")) {
            for (Map.Entry<org.bukkit.enchantments.Enchantment, Integer> entry : stack.getEnchantments().entrySet()) {
                org.bukkit.enchantments.Enchantment enchantment = entry.getKey();
                Component one = Component.translatable(enchantment.translationKey())
                    .append(level(entry.getValue(), enchantment.getMaxLevel()));
                lines.add(one.colorIfAbsent(enchantment.isCursed() ? NamedTextColor.RED : NamedTextColor.GRAY));
            }
        }
        if (stack.lore() != null) {
            for (Component line : stack.lore()) {
                lines.add(line.colorIfAbsent(NamedTextColor.DARK_PURPLE).decorationIfAbsent(TextDecoration.ITALIC,
                    TextDecoration.State.TRUE));
            }
        }
        if (!hidden(components, "minecraft:unbreakable") && components.containsKey("minecraft:unbreakable")) {
            lines.add(Component.translatable("item.unbreakable").color(NamedTextColor.BLUE));
        }
        if (context.isAdvanced()) {
            int max = stack.getType().getMaxDurability();
            if (max > 0 && !components.containsKey("minecraft:unbreakable")) {
                int wear = Nbt.integer(components, "minecraft:damage", 0);
                lines.add(Component.translatable("item.durability", Component.text(max - wear), Component.text(max)));
            }
            lines.add(Component.text(stack.getType().getKey().toString()).color(NamedTextColor.DARK_GRAY));
        }
        return lines;
    }
}
