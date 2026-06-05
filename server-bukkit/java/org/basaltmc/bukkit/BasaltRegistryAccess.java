package org.basaltmc.bukkit;

import io.papermc.paper.registry.RegistryAccess;
import io.papermc.paper.registry.RegistryKey;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Locale;
import java.util.Map;
import org.bukkit.Keyed;
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
    private static synchronized <T extends Keyed> Registry<T> named(String name) {
        return (Registry<T>) registries.computeIfAbsent(name, key -> new BasaltRegistry(() -> entriesOf(key)));
    }

    private static List<Keyed> entriesOf(String name) {
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
            case "entity_type":
                for (EntityType type : EntityType.values()) {
                    if (type != EntityType.UNKNOWN) {
                        out.add(type);
                    }
                }
                break;
            default:
                break;
        }
        return out;
    }

    @Override
    public <T extends Keyed> Registry<T> getRegistry(Class<T> type) {
        return named(type.getSimpleName().toLowerCase(Locale.ROOT));
    }

    @Override
    public <T extends Keyed> Registry<T> getRegistry(RegistryKey<T> key) {
        return named(key.key().value());
    }
}
