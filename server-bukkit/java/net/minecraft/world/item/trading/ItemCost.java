// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.item.trading;
import com.mojang.serialization.Codec;
import java.util.Optional;
import java.util.function.UnaryOperator;
import net.minecraft.core.Holder;
import net.minecraft.core.component.DataComponentExactPredicate;
import net.minecraft.network.RegistryFriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.world.item.Item;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.level.ItemLike;
public record ItemCost(Holder<Item> item, int count, DataComponentExactPredicate components, ItemStack itemStack) {
    public static final Codec<ItemCost> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.item.trading.ItemCost.class, "CODEC");
    public static final StreamCodec<RegistryFriendlyByteBuf, ItemCost> STREAM_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.streamCodec(net.minecraft.world.item.trading.ItemCost.class, "STREAM_CODEC");
    public static final StreamCodec<RegistryFriendlyByteBuf, Optional<ItemCost>> OPTIONAL_STREAM_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.streamCodec(net.minecraft.world.item.trading.ItemCost.class, "OPTIONAL_STREAM_CODEC");
    public ItemCost(ItemLike item) {
        this(item, 1);
    }
    public ItemCost(ItemLike item, int count) {
        this(net.minecraft.core.registries.BuiltInRegistries.ITEM.wrapAsHolder(item.asItem()), count, DataComponentExactPredicate.EMPTY);
    }
    public ItemCost(Holder<Item> item, int count, DataComponentExactPredicate components) {
        this(item, count, components, new ItemStack(item, count));
    }
    public ItemCost withComponents(UnaryOperator<DataComponentExactPredicate.Builder> components) {
        return this;
    }
    public boolean test(ItemStack itemStack) {
        return itemStack.is(item.value()) && itemStack.getCount() >= count;
    }
}
