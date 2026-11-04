// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.item.component;

import java.util.*;
import java.util.function.*;
import java.util.stream.*;
import net.minecraft.core.*;
import net.minecraft.core.component.*;
import net.minecraft.network.chat.*;
import net.minecraft.server.level.*;
import net.minecraft.sounds.*;
import net.minecraft.world.*;
import net.minecraft.world.entity.*;
import net.minecraft.world.entity.player.*;
import net.minecraft.world.item.context.*;
import net.minecraft.world.level.*;
import net.minecraft.world.level.block.*;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.gameevent.GameEvent;
import net.minecraft.world.phys.*;
import net.minecraft.world.item.component.*;
import net.minecraft.world.entity.projectile.*;
import net.minecraft.world.entity.projectile.throwableitemprojectile.*;
import net.minecraft.world.entity.projectile.hurtingprojectile.*;
import net.minecraft.world.entity.projectile.arrow.*;
import net.minecraft.world.level.block.state.properties.BlockStateProperties;
import net.minecraft.advancements.triggers.CriteriaTriggers;
import net.minecraft.stats.Stats;
import net.minecraft.util.Mth;
import net.minecraft.util.RandomSource;
import net.minecraft.world.item.enchantment.EnchantmentHelper;
import net.minecraft.world.level.block.entity.SignBlockEntity;
import net.minecraft.world.level.block.entity.SignTextSlot;
import net.minecraft.tags.*;

import net.minecraft.world.item.*;
import com.google.common.collect.ImmutableList;
import com.mojang.serialization.Codec;
import com.mojang.serialization.DataResult;
import net.minecraft.network.RegistryFriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.world.entity.SlotAccess;
import net.minecraft.world.entity.player.Player;
import net.minecraft.world.inventory.Slot;
import net.minecraft.world.inventory.tooltip.TooltipComponent;
import net.minecraft.world.item.slot.SlotSelector;
import org.apache.commons.lang3.math.Fraction;
public final class BundleContents implements ContainerComponent<BundleContents>, TooltipComponent {
    public static final BundleContents EMPTY = new BundleContents(List.of());
    public static final Codec<BundleContents> CODEC = ItemStackTemplate.CODEC.sizeLimitedListOf(1024).xmap(BundleContents::new,
        contents -> contents.items);
    public static final StreamCodec<RegistryFriendlyByteBuf, BundleContents> STREAM_CODEC = ItemStackTemplate.STREAM_CODEC
        .apply(net.minecraft.network.codec.ByteBufCodecs.list()).map(BundleContents::new, each -> each.items());
    private static final Fraction kBundleInBundleWeight = Fraction.getFraction(1, 16);
    public static final int NO_SELECTED_ITEM_INDEX = -1;
    public static final DataResult<Fraction> BEEHIVE_WEIGHT = DataResult.success(Fraction.ONE);

    private final List<ItemStackTemplate> items;
    private final int selectedItem;
    private DataResult<Fraction> weight;

    private BundleContents(List<ItemStackTemplate> items, int selectedItem) {
        this.items = items;
        this.selectedItem = selectedItem;
    }

    public BundleContents(List<ItemStackTemplate> items) {
        this(List.copyOf(items), NO_SELECTED_ITEM_INDEX);
    }

    private static DataResult<Fraction> computeContentWeight(List<? extends ItemInstance> items) {
        try {
            Fraction total = Fraction.ZERO;
            for (ItemInstance stack : items) {
                DataResult<Fraction> each = weightOf(stack);
                if (each.isError()) {
                    return each;
                }
                total = total.add(each.getOrThrow().multiplyBy(Fraction.getFraction(stack.count(), 1)));
            }
            return DataResult.success(total);
        } catch (ArithmeticException overflow) {
            return DataResult.error(() -> "Excessive total bundle weight");
        }
    }

    private static DataResult<Fraction> weightOf(ItemInstance item) {
        BundleContents nested = item.get(DataComponents.BUNDLE_CONTENTS);
        if (nested != null) {
            return nested.weight().map(inner -> inner.add(kBundleInBundleWeight));
        }
        if (!item.getOrDefault(DataComponents.BEES, Bees.EMPTY).bees().isEmpty()) {
            return BEEHIVE_WEIGHT;
        }
        return DataResult.success(Fraction.getFraction(1, item.getMaxStackSize()));
    }

    public static boolean canItemBeInBundle(ItemStack itemToAdd) {
        return !itemToAdd.isEmpty() && itemToAdd.getItem().canFitInsideContainerItems();
    }

    public int getNumberOfItemsToShow() {
        int stacks = size();
        int room = stacks > 12 ? 11 : 12;
        int onLastRow = stacks % 4;
        int gap = onLastRow == 0 ? 0 : 4 - onLastRow;
        return Math.min(stacks, room - gap);
    }

    @Override
    public Stream<ItemStack> itemCopies() {
        return items.stream().map(ItemStackTemplate::create);
    }

    public List<ItemStackTemplate> items() {
        return items;
    }

    @Override
    public int size() {
        return items.size();
    }

    public DataResult<Fraction> weight() {
        if (weight == null) {
            weight = computeContentWeight(items);
        }
        return weight;
    }

    public boolean isEmpty() {
        return items.isEmpty();
    }

    public int getSelectedItemIndex() {
        return selectedItem;
    }

    public ItemStackTemplate getSelectedItem() {
        return selectedItem == NO_SELECTED_ITEM_INDEX ? null : items.get(selectedItem);
    }

    @Override
    public BundleContents copyWithContents(Stream<ItemStack> newContents) {
        Mutable builder = new Mutable();
        newContents.forEach(builder::tryInsert);
        return builder.toImmutable();
    }

    @Override
    public Mutable asMutable() {
        DataResult<Fraction> current = weight();
        if (current.isError()) {
            return new Mutable();
        }
        List<ItemStack> copies = new ArrayList<>(items.size());
        for (ItemStackTemplate template : items) {
            copies.add(template.create());
        }
        return new Mutable(copies, current.getOrThrow(), selectedItem);
    }

    @Override
    public boolean equals(Object obj) {
        return this == obj || obj instanceof BundleContents other && items.equals(other.items);
    }

    @Override
    public int hashCode() {
        return items.hashCode();
    }

    @Override
    public String toString() {
        return "BundleContents" + items;
    }

    public static class Mutable extends GrowableMutableContainer<BundleContents> {
        private Fraction weight;
        private int selectedItem;
        private boolean needsFlattening;

        private Mutable(List<ItemStack> items, Fraction weight, int selectedItem) {
            super(items);
            this.weight = weight;
            this.selectedItem = selectedItem;
        }

        public Mutable() {
            this(new ArrayList<>(), Fraction.ZERO, NO_SELECTED_ITEM_INDEX);
        }

        public Mutable clearItems() {
            items.clear();
            weight = Fraction.ZERO;
            selectedItem = NO_SELECTED_ITEM_INDEX;
            return this;
        }

        private int findStackIndexWithinRange(ItemStack toAdd, int minInclusive, int maxExclusive) {
            if (!toAdd.isStackable()) {
                return -1;
            }
            int end = Math.min(maxExclusive, items.size());
            for (int i = Math.max(minInclusive, 0); i < end; i++) {
                if (ItemStack.isSameItemSameComponents(items.get(i), toAdd)) {
                    return i;
                }
            }
            return -1;
        }

        private int findStackIndex(ItemStack toAdd) {
            return findStackIndexWithinRange(toAdd, 0, items.size());
        }

        private int getMaxAmountToAdd(Fraction itemWeight) {
            return Math.max(Fraction.ONE.subtract(weight).divideBy(itemWeight).intValue(), 0);
        }

        public int tryInsert(ItemStack itemsToAdd) {
            if (!canItemBeInBundle(itemsToAdd)) {
                return 0;
            }
            DataResult<Fraction> each = weightOf(itemsToAdd);
            if (each.isError()) {
                return 0;
            }
            Fraction itemWeight = each.getOrThrow();
            int amount = Math.min(itemsToAdd.getCount(), getMaxAmountToAdd(itemWeight));
            if (amount == 0) {
                return 0;
            }
            weight = weight.add(stackedWeight(itemWeight, amount));
            int existing = findStackIndex(itemsToAdd);
            if (existing != -1) {
                ItemStack removed = items.remove(existing);
                ItemStack merged = removed.copyWithCount(removed.getCount() + amount);
                itemsToAdd.shrink(amount);
                items.addFirst(merged);
            } else {
                items.addFirst(itemsToAdd.split(amount));
            }
            return amount;
        }

        public int tryTransfer(Slot slot, Player player) {
            ItemStack other = slot.getItem();
            DataResult<Fraction> each = weightOf(other);
            if (each.isError()) {
                return 0;
            }
            int max = getMaxAmountToAdd(each.getOrThrow());
            return canItemBeInBundle(other) ? tryInsert(slot.safeTake(other.getCount(), max, player)) : 0;
        }

        public void toggleSelectedItem(int selected) {
            selectedItem = selectedItem != selected && !outsideBounds(selected) ? selected : NO_SELECTED_ITEM_INDEX;
        }

        private boolean outsideBounds(int index) {
            return index < 0 || index >= items.size();
        }

        public ItemStack removeOne() {
            if (items.isEmpty()) {
                return null;
            }
            ItemStack stack = items.remove(outsideBounds(selectedItem) ? 0 : selectedItem).copy();
            weight = weight.subtract(stackedWeight(stack));
            toggleSelectedItem(NO_SELECTED_ITEM_INDEX);
            return stack;
        }

        private static Fraction stackedWeight(Fraction each, int count) {
            return each.multiplyBy(Fraction.getFraction(count, 1));
        }

        private static Fraction stackedWeight(ItemStack stack) {
            return stackedWeight(weightOf(stack).getOrThrow(), stack.getCount());
        }

        public Fraction weight() {
            return weight;
        }

        @Override
        public int replaceSlotItems(ItemProvider newItems, SlotSelector slotSelector) {
            mergeIdenticalStacks();
            return super.replaceSlotItems(newItems, slotSelector);
        }

        @Override
        public void modifySlots(Consumer<? super SlotAccess> consumer, SlotSelector slotSelector) {
            mergeIdenticalStacks();
            super.modifySlots(consumer, slotSelector);
        }

        @Override
        protected boolean setItem(int slot, ItemStack itemStack) {
            ItemStack current = items.get(slot);
            Fraction without = current.isEmpty() ? weight : weight.subtract(stackedWeight(current));
            Fraction updated = itemStack.isEmpty() ? without : weightWithAdded(without, itemStack);
            if (updated != null && super.setItem(slot, itemStack)) {
                weight = updated;
                needsFlattening = true;
                return true;
            }
            return false;
        }

        @Override
        protected boolean addSlotWithItem(ItemProvider newItems) {
            if (!newItems.findNextNonEmpty()) {
                return false;
            }
            Fraction updated = weightWithAdded(weight, newItems.peek());
            if (updated != null && super.addSlotWithItem(newItems)) {
                weight = updated;
                needsFlattening = true;
                return true;
            }
            return false;
        }

        private static Fraction weightWithAdded(Fraction base, ItemStack toAdd) {
            if (!canItemBeInBundle(toAdd)) {
                return null;
            }
            DataResult<Fraction> each = weightOf(toAdd);
            if (each.isError()) {
                return null;
            }
            Fraction total = base.add(stackedWeight(each.getOrThrow(), toAdd.getCount()));
            return total.compareTo(Fraction.ONE) <= 0 ? total : null;
        }

        @Override
        public boolean canInsertNewSlots() {
            return weight.compareTo(Fraction.ONE) < 0;
        }

        private void mergeIdenticalStacks() {
            if (!needsFlattening) {
                return;
            }
            for (int index = 0; index < items.size(); index++) {
                ItemStack stack = items.get(index);
                if (stack.isEmpty()) {
                    items.remove(index--);
                    continue;
                }
                int twin = findStackIndexWithinRange(stack, index + 1, items.size());
                if (twin != -1) {
                    items.set(twin, stack.copyWithCount(stack.getCount() + items.get(twin).getCount()));
                    items.remove(index--);
                }
            }
            needsFlattening = false;
        }

        @Override
        public BundleContents toImmutable() {
            mergeIdenticalStacks();
            ImmutableList.Builder<ItemStackTemplate> builder = ImmutableList.builder();
            for (ItemStack stack : items) {
                builder.add(ItemStackTemplate.fromNonEmptyStack(stack));
            }
            return new BundleContents(builder.build(), selectedItem);
        }
    }
}
