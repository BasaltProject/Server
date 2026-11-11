// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.storage.loot.functions;
import net.minecraft.core.component.DataComponents;
import net.minecraft.world.item.component.SeededContainerLoot;
import com.mojang.serialization.Codec;
import com.mojang.serialization.MapCodec;
import java.util.Optional;
import net.minecraft.core.Holder;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.level.storage.loot.LootContext;
import net.minecraft.world.level.storage.loot.LootTable;
import net.minecraft.world.level.storage.loot.predicates.LootItemCondition;
public class SetContainerLootTable extends LootItemConditionalFunction {
    private final Holder.Reference<LootTable> lootTableId;
    private final long seed;
    public static final Codec<Holder.Reference<LootTable>> ID_ONLY_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.level.storage.loot.functions.SetContainerLootTable.class, "ID_ONLY_CODEC");
    public static final MapCodec<SetContainerLootTable> MAP_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.storage.loot.functions.SetContainerLootTable.class, "MAP_CODEC");
    private SetContainerLootTable(Optional<Holder<LootItemCondition>> condition, Holder.Reference<LootTable> lootTableId, long seed) {
        super(condition);
        this.lootTableId = lootTableId;
        this.seed = seed;
    }
    public MapCodec<SetContainerLootTable> codec() {
        return (com.mojang.serialization.MapCodec) MAP_CODEC;
    }
    public ItemStack run(ItemStack itemStack, LootContext context) {
        if (!itemStack.isEmpty()) {
            itemStack.set(DataComponents.CONTAINER_LOOT, new SeededContainerLoot(lootTableId.key(), seed));
        }
        return itemStack;
    }
    public static LootItemConditionalFunction.Builder<?> withLootTable(Holder.Reference<LootTable> value) {
        return withLootTable(value, 0L);
    }
    public static LootItemConditionalFunction.Builder<?> withLootTable(Holder.Reference<LootTable> value, long seed) {
        return simpleBuilder(c -> new SetContainerLootTable(c, value, seed));
    }
}
