package net.minecraft.world.item;

import net.minecraft.core.Holder;
import net.minecraft.core.component.DataComponentPatch;
import net.minecraft.core.registries.BuiltInRegistries;
import net.minecraft.network.chat.Component;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.world.InteractionResult;
import net.minecraft.world.item.context.UseOnContext;
import net.minecraft.world.level.ItemLike;

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
    public static final ItemStack EMPTY = new ItemStack((Item) null, 0);

    private Item item;
    private int count;
    private DataComponentPatch patch = DataComponentPatch.EMPTY;

    public ItemStack(ItemLike item, int count) {
        this.item = item instanceof Item found ? found : null;
        this.count = count;
    }

    public ItemStack(Holder<Item> item, int count) {
        this.item = item == null ? null : item.value();
        this.count = count;
    }

    public Item getItem() {
        return item == null ? BuiltInRegistries.ITEM.getValue(BuiltInRegistries.ITEM.getDefaultKey()) : item;
    }

    public int getCount() {
        return count;
    }

    public void setCount(int count) {
        this.count = count;
    }

    public boolean isEmpty() {
        return item == null || count <= 0;
    }

    public DataComponentPatch getComponentsPatch() {
        return patch;
    }

    public void applyComponents(DataComponentPatch changes) {
        patch = changes;
    }

    public Component getItemName() {
        return Component.translatable(getItem().getDescriptionId());
    }

    public InteractionResult useOn(UseOnContext context) {
        throw Unsupported.hit("net.minecraft.world.item.ItemStack.useOn");
    }
}
