// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.item.component;

import com.mojang.serialization.Codec;
import com.mojang.serialization.codecs.RecordCodecBuilder;
import java.util.ArrayList;
import java.util.List;
import java.util.Optional;
import java.util.function.Consumer;
import java.util.stream.Stream;
import net.minecraft.core.NonNullList;
import net.minecraft.core.component.DataComponentGetter;
import net.minecraft.network.RegistryFriendlyByteBuf;
import net.minecraft.network.chat.Component;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.world.item.Item;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.item.ItemStackTemplate;
import net.minecraft.world.item.TooltipFlag;

public final class ItemContainerContents implements ContainerComponent<ItemContainerContents>, TooltipProvider {
    public static final int MAX_SIZE = 256;
    public static final ItemContainerContents EMPTY = new ItemContainerContents(List.of());
    public static final Codec<ItemContainerContents> CODEC = Slot.CODEC.sizeLimitedListOf(MAX_SIZE).xmap(ItemContainerContents::fromSlots,
        ItemContainerContents::asSlots);
    public static final StreamCodec<RegistryFriendlyByteBuf, ItemContainerContents> STREAM_CODEC = ItemStackTemplate.STREAM_CODEC.apply(net.minecraft.network.codec.ByteBufCodecs::optional).apply(net.minecraft.network.codec.ByteBufCodecs.list(256)).map(ItemContainerContents::new, each -> each.items);

    private final List<Optional<ItemStackTemplate>> items;

    private ItemContainerContents(List<Optional<ItemStackTemplate>> items) {
        this.items = items;
    }

    private static ItemContainerContents fromSlots(List<Slot> slots) {
        int highest = slots.stream().mapToInt(Slot::index).max().orElse(-1);
        List<Optional<ItemStackTemplate>> out = new ArrayList<>();
        for (int i = 0; i <= highest; i++) {
            out.add(Optional.empty());
        }
        for (Slot slot : slots) {
            out.set(slot.index(), Optional.of(slot.item()));
        }
        return new ItemContainerContents(List.copyOf(out));
    }

    private List<Slot> asSlots() {
        List<Slot> out = new ArrayList<>();
        for (int i = 0; i < items.size(); i++) {
            int index = i;
            items.get(i).ifPresent(item -> out.add(new Slot(index, item)));
        }
        return out;
    }

    public static ItemContainerContents fromItems(List<ItemStack> itemStacks) {
        int last = -1;
        for (int i = 0; i < itemStacks.size(); i++) {
            if (!itemStacks.get(i).isEmpty()) {
                last = i;
            }
        }
        if (last == -1) {
            return EMPTY;
        }
        List<Optional<ItemStackTemplate>> out = new ArrayList<>(last + 1);
        for (int i = 0; i <= last; i++) {
            ItemStack stack = itemStacks.get(i);
            out.add(stack.isEmpty() ? Optional.empty() : Optional.of(ItemStackTemplate.fromNonEmptyStack(stack)));
        }
        return new ItemContainerContents(List.copyOf(out));
    }

    public void copyInto(NonNullList<ItemStack> destination) {
        for (int i = 0; i < destination.size(); i++) {
            destination.set(i, i < items.size() ? items.get(i).map(ItemStackTemplate::create).orElse(ItemStack.EMPTY) : ItemStack.EMPTY);
        }
    }

    public ItemStack copyOne() {
        return items.stream().flatMap(Optional::stream).findFirst().map(ItemStackTemplate::create).orElse(ItemStack.EMPTY);
    }

    @Override
    public Stream<ItemStack> itemCopies() {
        return items.stream().map(item -> item.map(ItemStackTemplate::create).orElse(ItemStack.EMPTY));
    }

    public Stream<ItemStack> nonEmptyItemCopyStream() {
        return items.stream().flatMap(Optional::stream).map(ItemStackTemplate::create);
    }

    public Iterable<ItemStackTemplate> nonEmptyItems() {
        return items.stream().flatMap(Optional::stream).toList();
    }

    @Override
    public int size() {
        return items.size();
    }

    @Override
    public ItemContainerContents copyWithContents(Stream<ItemStack> newContents) {
        return fromItems(newContents.toList());
    }

    @Override
    public Mutable asMutable() {
        return new Mutable(itemCopies().toList());
    }

    @Override
    public boolean equals(Object obj) {
        return obj instanceof ItemContainerContents other && items.equals(other.items);
    }

    @Override
    public int hashCode() {
        return items.hashCode();
    }

    @Override
    public void addToTooltip(Item.TooltipContext context, Consumer<Component> consumer, TooltipFlag flag, DataComponentGetter components) {
        nonEmptyItemCopyStream().forEach(stack -> consumer.accept(Component.translatable("item.container.item_count", stack.getHoverName(), stack.getCount())));
    }

    static record Slot(int index, ItemStackTemplate item) {
        public static final Codec<Slot> CODEC = RecordCodecBuilder.create(instance -> instance.group(
            Codec.intRange(0, MAX_SIZE - 1).fieldOf("slot").forGetter(Slot::index),
            ItemStackTemplate.CODEC.fieldOf("item").forGetter(Slot::item)).apply(instance, Slot::new));
    }

    public static class Mutable extends GrowableMutableContainer<ItemContainerContents> {
        private final List<ItemStack> stacks;

        private Mutable(List<ItemStack> items) {
            super(items);
            this.stacks = new ArrayList<>(items);
        }

        @Override
        public boolean canInsertNewSlots() {
            return stacks.size() < MAX_SIZE;
        }

        @Override
        public ItemContainerContents toImmutable() {
            return fromItems(stacks);
        }

        @Override
        public Stream<ItemStack> itemCopies() {
            return stacks.stream().map(ItemStack::copy);
        }

        @Override
        public int size() {
            return stacks.size();
        }
    }
}
