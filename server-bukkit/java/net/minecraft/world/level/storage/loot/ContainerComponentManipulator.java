// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.storage.loot;
import java.util.function.Consumer;
import java.util.function.UnaryOperator;
import java.util.stream.Stream;
import net.minecraft.core.component.DataComponentType;
import net.minecraft.world.entity.SlotAccess;
import net.minecraft.world.item.ItemProvider;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.item.component.ContainerComponent;
import net.minecraft.world.item.slot.SlotCollection;
import net.minecraft.world.item.slot.SlotSelector;
public record ContainerComponentManipulator<T extends ContainerComponent<T>>(DataComponentType<T> type, T empty) {
    public void setContents(ItemStack itemStack, T defaultValue, Stream<ItemStack> newContents) {
        T current = itemStack.getOrDefault(type(), defaultValue);
        itemStack.set(type(), current.copyWithContents(newContents));
    }
    public void setContents(ItemStack itemStack, Stream<ItemStack> newContents) {
        setContents(itemStack, empty(), newContents);
    }
    public void modifyItems(ItemStack itemStack, UnaryOperator<ItemStack> modifier) {
        T contents = itemStack.get(type());
        if (contents == null) {
            return;
        }
        UnaryOperator<ItemStack> guarded = stack -> {
            if (stack.isEmpty()) {
                return stack;
            }
            ItemStack changed = modifier.apply(stack);
            changed.limitSize(changed.getMaxStackSize());
            return changed;
        };
        setContents(itemStack, contents.itemCopies().map(guarded));
    }
    public SlotCollection getSlots(ItemStack itemStack) {
        return new ComponentSlotCollection<>(itemStack, this);
    }
    static record ComponentSlotCollection<T extends ContainerComponent<T>>(ItemStack itemStack, ContainerComponentManipulator<T> component) implements SlotCollection {
        public Stream<ItemStack> itemCopies() {
            T contents = itemStack.get(component.type());
            return contents != null ? contents.itemCopies() : Stream.empty();
        }
        public int size() {
            T contents = itemStack.get(component.type());
            return contents != null ? contents.size() : 0;
        }
        public int replaceSlotItems(ItemProvider items, SlotSelector slotSelector) {
            ContainerComponent.Mutable<T> mutable = itemStack.getOrDefault(component.type(), component.empty()).asMutable();
            int replaced = mutable.replaceSlotItems(items, slotSelector);
            itemStack.set(component.type(), mutable.toImmutable());
            return replaced;
        }
        public void modifySlots(Consumer<? super SlotAccess> consumer, SlotSelector slotSelector) {
            ContainerComponent.Mutable<T> mutable = itemStack.getOrDefault(component.type(), component.empty()).asMutable();
            mutable.modifySlots(consumer, slotSelector);
            itemStack.set(component.type(), mutable.toImmutable());
        }
    }
}
