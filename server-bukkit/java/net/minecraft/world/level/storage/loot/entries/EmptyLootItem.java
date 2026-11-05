// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.storage.loot.entries;
import com.mojang.serialization.MapCodec;
import java.util.Optional;
import java.util.function.Consumer;
import net.minecraft.core.Holder;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.level.storage.loot.LootContext;
import net.minecraft.world.level.storage.loot.functions.LootItemFunction;
import net.minecraft.world.level.storage.loot.predicates.LootItemCondition;
public class EmptyLootItem extends SingleEntryContainerBase {
    public static final MapCodec<EmptyLootItem> MAP_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.storage.loot.entries.EmptyLootItem.class, "MAP_CODEC");
    private EmptyLootItem(int weight, int quality, Optional<Holder<LootItemCondition>> condition, Optional<Holder<LootItemFunction>> modifier) {
        super(weight, quality, condition, modifier);
    }
    protected void writeEntry(StringBuilder out) {
        out.append("\"type\":\"minecraft:empty\",\"weight\":").append(weight).append(",\"quality\":").append(quality);
    }
    public MapCodec<EmptyLootItem> codec() {
        return MAP_CODEC;
    }
    public void createItemStack(Consumer<ItemStack> output, LootContext context) {
        return;
    }
    public static UniformContainerBase.Builder<?> emptyItem() {
        return simpleBuilder(EmptyLootItem::new);
    }
}
