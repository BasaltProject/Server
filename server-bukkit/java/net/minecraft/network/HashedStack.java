// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.network;

import java.util.Optional;
import net.minecraft.core.Holder;
import net.minecraft.core.registries.Registries;
import net.minecraft.network.codec.ByteBufCodecs;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.world.item.Item;
import net.minecraft.world.item.ItemStack;

public interface HashedStack {
    HashedStack EMPTY = new HashedStack() {
        public String toString() {
            return "<empty>";
        }

        public boolean matches(ItemStack stack, HashedPatchMap.HashGenerator hasher) {
            return stack.isEmpty();
        }
    };
    StreamCodec<RegistryFriendlyByteBuf, HashedStack> STREAM_CODEC = ByteBufCodecs.optional(ActualItem.STREAM_CODEC).map(
        actual -> actual.isPresent() ? actual.get() : EMPTY,
        stack -> stack instanceof ActualItem actual ? Optional.of(actual) : Optional.empty());

    boolean matches(ItemStack stack, HashedPatchMap.HashGenerator hasher);

    static HashedStack create(ItemStack itemStack, HashedPatchMap.HashGenerator hasher) {
        return itemStack.isEmpty() ? EMPTY
            : new ActualItem(itemStack.typeHolder(), itemStack.getCount(), HashedPatchMap.create(itemStack.getComponentsPatch(), hasher));
    }

    public static record ActualItem(Holder<Item> item, int count, HashedPatchMap components) implements HashedStack {
        public static final StreamCodec<RegistryFriendlyByteBuf, ActualItem> STREAM_CODEC = StreamCodec.composite(
            ByteBufCodecs.holderRegistry(Registries.ITEM), ActualItem::item, ByteBufCodecs.VAR_INT, ActualItem::count,
            HashedPatchMap.STREAM_CODEC, ActualItem::components, ActualItem::new);

        public boolean matches(ItemStack itemStack, HashedPatchMap.HashGenerator hasher) {
            return count == itemStack.getCount() && item.equals(itemStack.typeHolder())
                && components.matches(itemStack.getComponentsPatch(), hasher);
        }
    }
}
