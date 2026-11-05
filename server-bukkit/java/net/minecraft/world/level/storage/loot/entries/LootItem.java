// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.storage.loot.entries;
import com.mojang.serialization.MapCodec;
import java.util.Optional;
import java.util.function.Consumer;
import net.minecraft.core.Holder;
import net.minecraft.world.item.Item;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.level.ItemLike;
import net.minecraft.world.level.storage.loot.LootContext;
import net.minecraft.world.level.storage.loot.functions.LootItemFunction;
import net.minecraft.world.level.storage.loot.predicates.LootItemCondition;
public class LootItem extends SingleEntryContainerBase {
    public static final MapCodec<LootItem> MAP_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.storage.loot.entries.LootItem.class, "MAP_CODEC");
    private LootItem(Holder<Item> item, int weight, int quality, Optional<Holder<LootItemCondition>> condition, Optional<Holder<LootItemFunction>> modifier) {
        super(weight, quality, condition, modifier);
        this.item = item;
    }
    private final Holder<Item> item;
    protected void writeEntry(StringBuilder out) {
        out.append("\"type\":\"minecraft:item\",\"name\":");
        org.basaltmc.bukkit.loot.LootJson.string(out, item.unwrapKey().map(key -> key.identifier().toString())
            .orElseGet(() -> net.minecraft.core.registries.BuiltInRegistries.ITEM.getKey(item.value()).toString()));
        out.append(",\"weight\":").append(weight).append(",\"quality\":").append(quality);
    }
    public MapCodec<LootItem> codec() {
        return MAP_CODEC;
    }
    public void createItemStack(Consumer<ItemStack> output, LootContext context) {
        output.accept(new ItemStack(item));
    }
    public static UniformContainerBase.Builder<?> lootTableItem(ItemLike item) {
        return simpleBuilder((weight, quality, condition, modifier) -> new LootItem(item.asItem().builtInRegistryHolder(), weight, quality, condition, modifier));
    }
}
