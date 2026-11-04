// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.item;

import com.mojang.serialization.Codec;
import com.mojang.serialization.MapCodec;
import com.mojang.serialization.codecs.RecordCodecBuilder;
import net.minecraft.core.Holder;
import net.minecraft.core.component.DataComponentPatch;
import net.minecraft.core.component.DataComponentType;
import net.minecraft.core.registries.BuiltInRegistries;
import net.minecraft.network.RegistryFriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.resources.Identifier;
import org.basaltmc.bukkit.facade.HolderRefs;

public record ItemStackTemplate(Holder<Item> item, int count, DataComponentPatch components) implements ItemInstance {
    public static final MapCodec<ItemStackTemplate> MAP_CODEC = RecordCodecBuilder.mapCodec(instance -> instance.group(
        Identifier.CODEC.<Holder<Item>>xmap(id -> HolderRefs.of(Item.class, id), holder -> holder.unwrapKey().get().identifier())
            .fieldOf("id").forGetter(ItemStackTemplate::item),
        Codec.intRange(1, 99).optionalFieldOf("count", 1).forGetter(ItemStackTemplate::count),
        DataComponentPatch.CODEC.optionalFieldOf("components", DataComponentPatch.EMPTY).forGetter(ItemStackTemplate::components))
        .apply(instance, ItemStackTemplate::new));
    public static final Codec<ItemStackTemplate> CODEC = MAP_CODEC.codec();
    public static final StreamCodec<RegistryFriendlyByteBuf, ItemStackTemplate> STREAM_CODEC = net.minecraft.network.codec.StreamCodec.composite(net.minecraft.network.codec.ByteBufCodecs.holderRegistry(net.minecraft.core.registries.Registries.ITEM), ItemStackTemplate::item, net.minecraft.network.codec.ByteBufCodecs.VAR_INT, ItemStackTemplate::count, DataComponentPatch.STREAM_CODEC, ItemStackTemplate::components, ItemStackTemplate::new);

    public ItemStackTemplate(Item item) {
        this(item, 1, DataComponentPatch.EMPTY);
    }

    public ItemStackTemplate(Item item, int count) {
        this(item, count, DataComponentPatch.EMPTY);
    }

    public ItemStackTemplate(Item item, DataComponentPatch patch) {
        this(item, 1, patch);
    }

    private ItemStackTemplate(Item item, int count, DataComponentPatch patch) {
        this(BuiltInRegistries.ITEM.wrapAsHolder(item), count, patch);
    }

    public static ItemStackTemplate fromNonEmptyStack(ItemStack itemStack) {
        return fromNonEmptyStack(itemStack, itemStack.getCount());
    }

    public static ItemStackTemplate fromStack(ItemStack itemStack) {
        return itemStack.isEmpty() ? null : fromNonEmptyStack(itemStack);
    }

    public static ItemStackTemplate fromNonEmptyStack(ItemStack itemStack, int newCount) {
        if (itemStack.isEmpty()) {
            throw new IllegalStateException("Stack is empty");
        }
        return new ItemStackTemplate(itemStack.typeHolder(), newCount, itemStack.getComponentsPatch());
    }

    public ItemStackTemplate withCount(int count) {
        return count == this.count ? this : new ItemStackTemplate(item, count, components);
    }

    public ItemStack create() {
        return new ItemStack(item, count, components);
    }

    public ItemStack apply(DataComponentPatch additionalPatch) {
        return apply(count, additionalPatch);
    }

    public ItemStack apply(int count, DataComponentPatch additionalPatch) {
        ItemStack stack = new ItemStack(item, count, components);
        if (!additionalPatch.isEmpty()) {
            net.minecraft.core.component.PatchedDataComponentMap map =
                net.minecraft.core.component.PatchedDataComponentMap.fromPatch(net.minecraft.core.component.DataComponentMap.EMPTY, components);
            map.applyPatch(additionalPatch);
            stack.applyComponents(map.asPatch());
        }
        return stack;
    }

    public Holder<Item> typeHolder() {
        return item;
    }

    @Override
    public <T> T get(DataComponentType<? extends T> type) {
        return components.get(net.minecraft.core.component.DataComponentMap.EMPTY, type);
    }
}
