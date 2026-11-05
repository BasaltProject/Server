// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.storage.loot.entries;
import com.mojang.serialization.MapCodec;
import java.util.Optional;
import java.util.function.Consumer;
import net.minecraft.core.Holder;
import net.minecraft.core.HolderSet;
import net.minecraft.world.level.storage.loot.LootTable;
import net.minecraft.world.level.storage.loot.ValidationContext;
import net.minecraft.world.level.storage.loot.functions.LootItemFunction;
import net.minecraft.world.level.storage.loot.predicates.LootItemCondition;
public class NestedLootTable extends ExpandableContainerBase {
    public static final MapCodec<NestedLootTable> MAP_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.storage.loot.entries.NestedLootTable.class, "MAP_CODEC");
    private NestedLootTable(HolderSet<LootTable> value, boolean expand, int weight, int quality, Optional<Holder<LootItemCondition>> condition, Optional<Holder<LootItemFunction>> modifier) {
        super(expand, weight, quality, condition, modifier);
        this.value = value;
    }
    private final HolderSet<LootTable> value;
    protected void writeEntry(StringBuilder out) {
        out.append("\"type\":\"minecraft:loot_table\",\"value\":");
        org.basaltmc.bukkit.loot.LootJson.value(out, value.get(0));
        out.append(",\"weight\":").append(weight).append(",\"quality\":").append(quality);
    }
    public MapCodec<NestedLootTable> codec() {
        return MAP_CODEC;
    }
    protected boolean addExpandedEntries(Consumer<LootPoolEntry> output) {
        return false;
    }
    protected boolean addUnexpandedEntry(Consumer<LootPoolEntry> output) {
        return false;
    }
    public void validate(ValidationContext context) {
        return;
    }
    public static UniformContainerBase.Builder<?> lootTableReference(Holder<LootTable> table) {
        return simpleBuilder((weight, quality, condition, modifier) -> new NestedLootTable(HolderSet.direct(table), false, weight, quality, condition, modifier));
    }
    public static UniformContainerBase.Builder<?> inlineLootTable(LootTable table) {
        return simpleBuilder((weight, quality, condition, modifier) -> new NestedLootTable(HolderSet.direct(Holder.direct(table)), false, weight, quality, condition, modifier));
    }
}
