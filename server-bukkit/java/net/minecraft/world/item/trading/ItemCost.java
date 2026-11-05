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
    public static final Codec<ItemCost> CODEC = com.mojang.serialization.codecs.RecordCodecBuilder.create(i -> i.group(
        net.minecraft.resources.Identifier.CODEC.<Holder<Item>>xmap(id -> org.basaltmc.bukkit.facade.HolderRefs.of(Item.class, id), holder -> holder.unwrapKey().get().identifier())
            .fieldOf("id").forGetter(ItemCost::item),
        Codec.intRange(1, 99).optionalFieldOf("count", 1).forGetter(ItemCost::count),
        DataComponentExactPredicate.CODEC.optionalFieldOf("components", DataComponentExactPredicate.EMPTY).forGetter(ItemCost::components))
        .apply(i, ItemCost::new));
    public static final StreamCodec<RegistryFriendlyByteBuf, ItemCost> STREAM_CODEC = StreamCodec.composite(
        net.minecraft.network.codec.ByteBufCodecs.holderRegistry(net.minecraft.core.registries.Registries.ITEM), ItemCost::item,
        net.minecraft.network.codec.ByteBufCodecs.VAR_INT, ItemCost::count,
        DataComponentExactPredicate.STREAM_CODEC, ItemCost::components, ItemCost::new);
    public static final StreamCodec<RegistryFriendlyByteBuf, Optional<ItemCost>> OPTIONAL_STREAM_CODEC = STREAM_CODEC.apply(net.minecraft.network.codec.ByteBufCodecs::optional);
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
        return new ItemCost(item, count, components.apply(DataComponentExactPredicate.builder()).build());
    }
    public boolean test(ItemStack itemStack) {
        return itemStack.is(item.value()) && itemStack.getCount() >= count && components.test(itemStack.getComponents());
    }
}
