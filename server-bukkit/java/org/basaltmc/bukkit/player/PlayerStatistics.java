package org.basaltmc.bukkit.player;

import java.util.Locale;
import java.util.Map;
import org.bukkit.Material;
import org.bukkit.Statistic;
import org.bukkit.entity.EntityType;

public final class PlayerStatistics {
    private static final Map<Statistic, String> kRenamed = Map.ofEntries(
        Map.entry(Statistic.PLAY_ONE_MINUTE, "play_time"),
        Map.entry(Statistic.CAKE_SLICES_EATEN, "eat_cake_slice"),
        Map.entry(Statistic.CAULDRON_FILLED, "fill_cauldron"),
        Map.entry(Statistic.CAULDRON_USED, "use_cauldron"),
        Map.entry(Statistic.ARMOR_CLEANED, "clean_armor"),
        Map.entry(Statistic.BANNER_CLEANED, "clean_banner"),
        Map.entry(Statistic.BREWINGSTAND_INTERACTION, "interact_with_brewingstand"),
        Map.entry(Statistic.BEACON_INTERACTION, "interact_with_beacon"),
        Map.entry(Statistic.CRAFTING_TABLE_INTERACTION, "interact_with_crafting_table"),
        Map.entry(Statistic.FURNACE_INTERACTION, "interact_with_furnace"),
        Map.entry(Statistic.DROP_COUNT, "drop"),
        Map.entry(Statistic.ITEM_ENCHANTED, "enchant_item"),
        Map.entry(Statistic.RECORD_PLAYED, "play_record"),
        Map.entry(Statistic.NOTEBLOCK_PLAYED, "play_noteblock"),
        Map.entry(Statistic.NOTEBLOCK_TUNED, "tune_noteblock"),
        Map.entry(Statistic.FLOWER_POTTED, "pot_flower"),
        Map.entry(Statistic.TRAPPED_CHEST_TRIGGERED, "trigger_trapped_chest"),
        Map.entry(Statistic.ENDERCHEST_OPENED, "open_enderchest"),
        Map.entry(Statistic.CHEST_OPENED, "open_chest"),
        Map.entry(Statistic.SHULKER_BOX_OPENED, "open_shulker_box"),
        Map.entry(Statistic.DISPENSER_INSPECTED, "inspect_dispenser"),
        Map.entry(Statistic.DROPPER_INSPECTED, "inspect_dropper"),
        Map.entry(Statistic.HOPPER_INSPECTED, "inspect_hopper"),
        Map.entry(Statistic.LEAVE_GAME, "leave_game"));

    private PlayerStatistics() {}

    static String untyped(Statistic statistic) {
        if (statistic.getType() != Statistic.Type.UNTYPED) {
            throw new IllegalArgumentException("Must supply additional paramater for this statistic");
        }
        String name = kRenamed.getOrDefault(statistic, statistic.name().toLowerCase(Locale.ROOT));
        return "minecraft:custom/minecraft:" + name;
    }

    private static String typed(Statistic statistic) {
        return switch (statistic) {
            case MINE_BLOCK -> "minecraft:mined/";
            case USE_ITEM -> "minecraft:used/";
            case BREAK_ITEM -> "minecraft:broken/";
            case CRAFT_ITEM -> "minecraft:crafted/";
            case PICKUP -> "minecraft:picked_up/";
            case DROP -> "minecraft:dropped/";
            case KILL_ENTITY -> "minecraft:killed/";
            case ENTITY_KILLED_BY -> "minecraft:killed_by/";
            default -> throw new IllegalArgumentException("Wrong parameter type for this statistic");
        };
    }

    static String of(Statistic statistic, Material material) {
        if (statistic.getType() != Statistic.Type.BLOCK && statistic.getType() != Statistic.Type.ITEM) {
            throw new IllegalArgumentException("This statistic does not take a Material parameter");
        }
        if (statistic.getType() == Statistic.Type.BLOCK && !material.isBlock()
            || statistic.getType() == Statistic.Type.ITEM && !material.isItem()) {
            throw new IllegalArgumentException("Invalid material for this statistic");
        }
        return typed(statistic) + material.getKey();
    }

    static String of(Statistic statistic, EntityType entity) {
        if (statistic.getType() != Statistic.Type.ENTITY) {
            throw new IllegalArgumentException("This statistic does not take an EntityType parameter");
        }
        if (entity == null || entity == EntityType.UNKNOWN) {
            throw new IllegalArgumentException("Invalid entity type for this statistic");
        }
        return typed(statistic) + entity.getKey();
    }

    /** The statistic a stored key stands for plus its material or entity, or null for a key Bukkit has no name for. */
    public static Object[] parse(String key) {
        int slash = key.indexOf('/');
        if (slash < 0) {
            return null;
        }
        String group = key.substring(0, slash);
        String name = key.substring(slash + 1);
        if (group.equals("minecraft:custom")) {
            for (Statistic statistic : Statistic.values()) {
                if (statistic.getType() == Statistic.Type.UNTYPED && untyped(statistic).equals(key)) {
                    return new Object[] {statistic};
                }
            }
            return null;
        }
        Statistic statistic = switch (group) {
            case "minecraft:mined" -> Statistic.MINE_BLOCK;
            case "minecraft:used" -> Statistic.USE_ITEM;
            case "minecraft:broken" -> Statistic.BREAK_ITEM;
            case "minecraft:crafted" -> Statistic.CRAFT_ITEM;
            case "minecraft:picked_up" -> Statistic.PICKUP;
            case "minecraft:dropped" -> Statistic.DROP;
            case "minecraft:killed" -> Statistic.KILL_ENTITY;
            case "minecraft:killed_by" -> Statistic.ENTITY_KILLED_BY;
            default -> null;
        };
        if (statistic == null) {
            return null;
        }
        if (statistic.getType() == Statistic.Type.ENTITY) {
            EntityType type = org.bukkit.Registry.ENTITY_TYPE.get(org.bukkit.NamespacedKey.fromString(name));
            return type == null ? null : new Object[] {statistic, type};
        }
        Material material = Material.matchMaterial(name);
        return material == null ? null : new Object[] {statistic, material};
    }
}
