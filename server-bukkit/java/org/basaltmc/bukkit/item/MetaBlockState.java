package org.basaltmc.bukkit.item;

import java.util.List;
import java.util.Map;
import org.basaltmc.bukkit.BasaltBlockState;
import org.basaltmc.bukkit.inv.ContainerStates;
import org.basaltmc.bukkit.tile.TileStates;
import org.bukkit.Material;
import org.bukkit.block.BlockState;
import org.bukkit.configuration.serialization.DelegateDeserialization;
import org.bukkit.inventory.meta.BlockStateMeta;

@DelegateDeserialization(ItemMetas.Serial.class)
public final class MetaBlockState extends MetaItem implements BlockStateMeta {
    private static final String kEntityData = "minecraft:block_entity_data";
    private static final String kContainer = "minecraft:container";
    private static final String kBees = "minecraft:bees";
    private static final String kLoot = "minecraft:container_loot";

    private BlockState cached;

    public MetaBlockState(Material type, Map<String, Object> components) {
        super(type, components);
    }

    @Override
    public MetaBlockState clone() {
        MetaBlockState copy = (MetaBlockState) super.clone();
        copy.cached = null;
        return copy;
    }

    @Override
    public boolean hasBlockState() {
        return c.containsKey(kEntityData) || c.containsKey(kContainer) || c.containsKey(kBees)
            || c.containsKey(kLoot);
    }

    @Override
    public void clearBlockState() {
        c.remove(kEntityData);
        c.remove(kContainer);
        c.remove(kBees);
        c.remove(kLoot);
        cached = null;
    }

    @Override
    public BlockState getBlockState() {
        if (cached == null) {
            Map<String, Object> data = Nbt.compound();
            Map<String, Object> held = Nbt.map(c.get(kEntityData));
            if (held != null) {
                data.putAll(Nbt.deepCopy(held));
                data.remove("id");
            }
            if (c.get(kBees) != null) {
                data.put("bees", Nbt.copy(c.get(kBees)));
            }
            Map<String, Object> loot = Nbt.map(c.get(kLoot));
            if (loot != null) {
                data.put("LootTable", Nbt.string(loot, "loot_table", ""));
                data.put("LootTableSeed", Nbt.longValue(loot, "seed", 0L));
            }
            BlockState base = BasaltBlockState.detached(materialType().createBlockData());
            List<Object> items = Nbt.list(c.get(kContainer));
            cached = TileStates.detached(base, data, items);
        }
        return cached;
    }

    @Override
    public void setBlockState(BlockState state) {
        if (state == null || state.getType() != materialType()) {
            throw new IllegalArgumentException("Invalid blockState, expected " + materialType() + " got "
                + (state == null ? null : state.getType()));
        }
        Map<String, Object> data = TileStates.dataOf(state);
        BlockState inner = TileStates.innerOf(state);
        List<Object> items = ContainerStates.detachedItems(inner);
        Map<String, Object> rest = data == null ? Nbt.compound() : Nbt.deepCopy(data);
        if (data == null) {
            Map<String, Object> plain = ContainerStates.detachedData(inner);
            if (plain != null) {
                rest = Nbt.deepCopy(plain);
            }
        }
        if (items.isEmpty()) {
            c.remove(kContainer);
        } else {
            c.put(kContainer, items);
        }
        rest.remove("Items");
        Object bees = rest.remove("bees");
        if (bees != null) {
            c.put(kBees, bees);
        } else {
            c.remove(kBees);
        }
        String table = Nbt.string(rest, "LootTable", "");
        rest.remove("LootTable");
        long seed = Nbt.longValue(rest, "LootTableSeed", 0L);
        rest.remove("LootTableSeed");
        if (table.isEmpty()) {
            c.remove(kLoot);
        } else {
            Map<String, Object> loot = Nbt.compound();
            loot.put("loot_table", table);
            if (seed != 0L) {
                loot.put("seed", seed);
            }
            c.put(kLoot, loot);
        }
        if (rest.isEmpty()) {
            c.remove(kEntityData);
        } else {
            rest.put("id", materialType().getKey().toString());
            c.put(kEntityData, rest);
        }
        cached = null;
    }
}
