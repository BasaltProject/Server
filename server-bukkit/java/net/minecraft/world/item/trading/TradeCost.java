// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.item.trading;
import com.mojang.serialization.Codec;
import net.minecraft.core.Holder;
import net.minecraft.core.component.DataComponentExactPredicate;
import net.minecraft.world.item.Item;
import net.minecraft.world.level.ItemLike;
import net.minecraft.world.level.storage.loot.LootContext;
import net.minecraft.world.level.storage.loot.Validatable;
import net.minecraft.world.level.storage.loot.ValidationContext;
import net.minecraft.world.level.storage.loot.providers.number.ints.ContextIntProvider;
public record TradeCost(Holder<Item> item, Holder<ContextIntProvider> count, DataComponentExactPredicate components) implements Validatable {
    public static final Codec<TradeCost> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.item.trading.TradeCost.class, "CODEC");
    public TradeCost(ItemLike item, int count) {
        this(item.asItem().builtInRegistryHolder(), net.minecraft.world.level.storage.loot.providers.number.ints.ContextIntProviders.exactly(count), DataComponentExactPredicate.EMPTY);
    }
    public TradeCost(ItemLike item, Holder<ContextIntProvider> count) {
        this(item.asItem().builtInRegistryHolder(), count, DataComponentExactPredicate.EMPTY);
    }
    public ItemCost toItemCost(LootContext lootContext, int additionalCost) {
        int wanted = count().value().getInt(lootContext) + additionalCost;
        return new ItemCost(item(), net.minecraft.util.Mth.clamp(wanted, 0, item().value().getDefaultMaxStackSize()), components());
    }
    public void validate(ValidationContext context) {
        Validatable.validateHolder(context, "count", count);

    }
}
