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
        return blockTypes.computeIfAbsent(block, BasaltBlockType::new);
    }

    @SuppressWarnings({"unchecked", "rawtypes"})
    private static synchronized <T extends Keyed> Registry<T> named(String name, Class<?> type) {
        return (Registry<T>) registries.computeIfAbsent(name, key -> genericRegistry(key, type));
    }

    /** Entries come from the server's own registry when it has one; an interface type also answers for any key named. */
    @SuppressWarnings({"unchecked", "rawtypes"})
    private static <T extends Keyed> Registry<T> genericRegistry(String name, Class<?> type) {
        Supplier<List<T>> source = () -> (List<T>) (List) entriesOf(name, type);
        switch (name) {
            case "game_event":
                return new BasaltRegistry<T>(source, key -> (T) new BasaltGameEvent(key));
            case "worldgen/structure":
                return new BasaltRegistry<T>(source, key -> (T) new BasaltStructure(key));
            case "worldgen/structure_type":
                return new BasaltRegistry<T>(source, key -> (T) new BasaltStructureType(key));
            default:
                break;
        }
        if (type != null && type.isInterface() && !BUILT_IN.contains(name)) {
            Class<T> keyed = (Class<T>) type;
            return new BasaltRegistry<T>(source, key -> KeyedProxy.of(keyed, key, -1));
        }
        return new BasaltRegistry<T>(source);
    }

    private static final java.util.Set<String> BUILT_IN = java.util.Set.of("block", "item", "worldgen/biome", "entity_type");

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
