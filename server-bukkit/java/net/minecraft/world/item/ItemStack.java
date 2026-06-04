package net.minecraft.world.item;

import net.minecraft.network.codec.StreamCodec;
import net.minecraft.world.level.ItemLike;

/** A stand-in: Basalt items are not vanilla objects, so converting to and from them is not supported. */
public class ItemStack {
    public static final StreamCodec<Object, ItemStack> OPTIONAL_STREAM_CODEC = new StreamCodec<>() {
        @Override
        public ItemStack decode(Object input) {
            throw new UnsupportedOperationException("Basalt items are not vanilla ItemStack objects");
        }

        @Override
        public void encode(Object output, ItemStack value) {
            throw new UnsupportedOperationException("Basalt items are not vanilla ItemStack objects");
        }
    };

    public ItemStack(ItemLike item, int count) {}
}
