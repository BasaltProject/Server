package org.basaltmc.bukkit;

import io.papermc.paper.registry.RegistryAccess;
import io.papermc.paper.registry.RegistryKey;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Locale;
import java.util.Map;
import java.util.function.Supplier;
import org.basaltmc.bukkit.gen.RegistryTypes;
import org.bukkit.Keyed;
import org.bukkit.NamespacedKey;
import org.bukkit.Registry;
import org.bukkit.block.BlockType;
import org.bukkit.entity.EntityType;

public final class BasaltRegistryAccess implements RegistryAccess {
    private static final Map<Integer, BasaltBlockType> blockTypes = new HashMap<>();
    private static final Map<String, Registry<?>> registries = new HashMap<>();

    public BasaltRegistryAccess() {}

    static synchronized BlockType blockType(int block) {
        BasaltBlockType type = blockTypes.get(block);
        if (type == null) {
            type = new BasaltBlockType(block);
            blockTypes.putIfAbsent(block, type);
            type = blockTypes.get(block);
        }
        return type;
    }

    @SuppressWarnings({"unchecked", "rawtypes"})
    private static synchronized <T extends Keyed> Registry<T> named(String name, Class<?> type) {
        return (Registry<T>) org.basaltmc.bukkit.facade.Memo.get(registries, name, key -> genericRegistry(key, type));
    }

    /** Entries come from the server's own registry when it has one; an interface type also answers for any key named. */
    @SuppressWarnings({"unchecked", "rawtypes"})
    private static <T extends Keyed> Registry<T> genericRegistry(String name, Class<?> type) {
        return ((BasaltRegistry<T>) (BasaltRegistry) plainRegistry(name, type)).named(name);
    }

    @SuppressWarnings({"unchecked", "rawtypes"})
    private static <T extends Keyed> Registry<T> plainRegistry(String name, Class<?> type) {
        Supplier<List<T>> source = () -> (List<T>) (List) entriesOf(name, type);
        switch (name) {
            case "game_event":
                return new BasaltRegistry<T>(source, key -> (T) new BasaltGameEvent(key), kGameEvents);
            case "instrument":
                return new BasaltRegistry<T>(source, key -> (T) new BasaltMusicInstrument(key),
                    BasaltMusicInstrument.kNames);
            case "worldgen/structure":
                return new BasaltRegistry<T>(source, key -> (T) new BasaltStructure(key), kStructures);
            case "worldgen/structure_type":
                return new BasaltRegistry<T>(source, key -> (T) new BasaltStructureType(key), kStructureTypes);
            default:
                break;
        }
        if (type != null && type.isInterface() && !BUILT_IN.contains(name)) {
            Class<T> keyed = (Class<T>) type;
            BasaltRegistry<T> opened = new BasaltRegistry<T>(source, key -> KeyedProxy.of(keyed, key, -1));
            return isSynced(name) ? opened.complete() : opened;
        }
        return new BasaltRegistry<T>(source);
    }

    /** parity: `GameEvent`'s registrations, in registry order. */
    private static final java.util.List<String> kGameEvents = java.util.List.of(
        "block_activate", "block_attach", "block_change", "block_close", "block_deactivate", "block_destroy",
        "block_detach", "block_open", "block_place", "bounce", "container_close", "container_open", "drink", "eat",
        "elytra_glide", "entity_damage", "entity_die", "entity_dismount", "entity_interact", "entity_mount",
        "entity_place", "entity_action", "equip", "explode", "flap", "fluid_pickup", "fluid_place", "hit_ground",
        "instrument_play", "item_interact_finish", "item_interact_start", "jukebox_play", "jukebox_stop_play",
        "lightning_strike", "note_block_play", "prime_fuse", "projectile_land", "projectile_shoot",
        "sculk_sensor_tendrils_clicking", "shear", "shriek", "splash", "step", "swim", "teleport", "unequip",
        "resonate_1", "resonate_2", "resonate_3", "resonate_4", "resonate_5", "resonate_6", "resonate_7",
        "resonate_8", "resonate_9", "resonate_10", "resonate_11", "resonate_12", "resonate_13", "resonate_14",
        "resonate_15");

    private static final java.util.List<String> kStructures = java.util.List.of("pillager_outpost", "mineshaft",
        "mineshaft_mesa", "mansion", "jungle_pyramid", "desert_pyramid", "igloo", "shipwreck", "shipwreck_beached",
        "swamp_hut", "stronghold", "monument", "ocean_ruin_cold", "ocean_ruin_warm", "fortress", "nether_fossil",
        "end_city", "buried_treasure", "bastion_remnant", "village_plains", "village_desert", "village_savanna",
        "village_snowy", "village_taiga", "ruined_portal", "ruined_portal_desert", "ruined_portal_jungle",
        "ruined_portal_swamp", "ruined_portal_mountain", "ruined_portal_ocean", "ruined_portal_nether", "ancient_city",
        "trail_ruins", "trial_chambers", "abandoned_camp_bamboo_jungle", "abandoned_camp_birch_forest",
        "abandoned_camp_cherry_grove", "abandoned_camp_dappled_forest", "abandoned_camp_flower_forest",
        "abandoned_camp_forest", "abandoned_camp_meadow", "abandoned_camp_old_growth_birch_forest",
        "abandoned_camp_old_growth_pine_taiga", "abandoned_camp_old_growth_spruce_taiga", "abandoned_camp_pale_garden",
        "abandoned_camp_savanna", "abandoned_camp_snowy_taiga", "abandoned_camp_sparse_jungle", "abandoned_camp_swamp",
        "abandoned_camp_taiga", "abandoned_camp_windswept_forest", "abandoned_camp_wooded_badlands");

    private static final java.util.List<String> kStructureTypes = java.util.List.of("buried_treasure", "desert_pyramid",
        "end_city", "fortress", "igloo", "jigsaw", "jungle_temple", "mineshaft", "nether_fossil", "ocean_monument",
        "ocean_ruin", "ruined_portal", "shipwreck", "stronghold", "swamp_hut", "woodland_mansion");

    /** The registries the server sends to clients are listed in full by the server itself. */
    private static boolean isSynced(String name) {
        return WorldNatives.registryIds(name).length > 0 && !kCustomLists.contains(name);
    }

    private static final java.util.Set<String> kCustomLists = java.util.Set.of("sound_event", "menu",
        "data_component_type", "game_rule", "mob_effect", "block", "item", "entity_type", "worldgen/biome", "biome");

    private static final java.util.Set<String> BUILT_IN =
        java.util.Set.of("block", "item", "worldgen/biome", "entity_type");

    private static List<Keyed> entriesOf(String name, Class<?> type) {
        List<Keyed> out = new ArrayList<>();
        switch (name) {
            case "block":
                for (int block = 0; block < Registries.blockCount(); block++) {
                    out.add(blockType(block));
                }
                break;
            case "item":
                for (String id : Registries.itemIds()) {
                    out.add(new BasaltItemType(id));
                }
                break;
            case "biome", "worldgen/biome":
                String[] ids = Registries.biomeIds();
                for (int i = 0; i < ids.length; i++) {
                    out.add(new BasaltBiome(ids[i], i));
                }
                break;
            case "data_component_type":
                out.addAll(org.basaltmc.bukkit.datacomponent.ComponentType.list().stream()
                    .map(entry -> (Keyed) entry).toList());
                break;
            case "game_rule":
                out.addAll(BasaltGameRule.all());
                break;
            case "menu":
                out.addAll(org.basaltmc.bukkit.inv.MenuTypes.list());
                break;
            case "mob_effect":
                out.addAll(org.basaltmc.bukkit.item.BasaltPotionEffectType.all());
                break;
            case "enchantment":
                for (String enchantmentId : InventoryNatives.enchantmentIds()) {
                    out.add(new org.basaltmc.bukkit.item.BasaltEnchantment(NamespacedKey.fromString(enchantmentId)));
                }
                break;
            case "entity_type":
                for (EntityType entity : EntityType.values()) {
                    if (entity != EntityType.UNKNOWN) {
                        out.add(entity);
                    }
                }
                break;
            case "sound_event":
                int ordinal = 0;
                for (String id : Registries.soundIds()) {
                    out.add(KeyedProxy.of((Class<? extends Keyed>) type, NamespacedKey.minecraft(id), ordinal++));
                }
                break;
            default:
                if (type != null && type.isInterface()) {
                    String[] entryIds = WorldNatives.registryIds(name);
                    for (int i = 0; i < entryIds.length; i++) {
                        out.add(KeyedProxy.of((Class<? extends Keyed>) type, NamespacedKey.fromString(entryIds[i]), i));
                    }
                } else if (type != null && type.isEnum()) {
                    for (Object constant : type.getEnumConstants()) {
                        out.add((Keyed) constant);
                    }
                }
                break;
        }
        return out;
    }

    @Override
    public <T extends Keyed> Registry<T> getRegistry(Class<T> type) {
        for (Map.Entry<String, Class<?>> entry : RegistryTypes.kByName.entrySet()) {
            if (entry.getValue() == type) {
                return named(entry.getKey(), type);
            }
        }
        return named(type.getSimpleName().toLowerCase(Locale.ROOT), type);
    }

    @Override
    public <T extends Keyed> Registry<T> getRegistry(RegistryKey<T> key) {
        String name = key.key().value();
        Class<?> type = RegistryTypes.kByName.get(name);
        return named(name, type);
    }
}
