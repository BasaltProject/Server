// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.item.slot;

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
import net.minecraft.world.entity.SlotAccess;
import net.minecraft.world.item.ItemProvider;
import net.minecraft.world.item.ItemStack;
import org.apache.commons.lang3.mutable.MutableInt;
public interface SlotCollection {
    SlotCollection EMPTY = new SlotCollection() {
        @Override
        public Stream<ItemStack> itemCopies() {
            return Stream.empty();
        }

        @Override
        public int size() {
            return 0;
        }

        @Override
        public int replaceSlotItems(ItemProvider items, SlotSelector slotSelector) {
            return 0;
        }

        @Override
        public void modifySlots(Consumer<? super SlotAccess> consumer, SlotSelector slotSelector) {
        }
    };

    Stream<ItemStack> itemCopies();

    int size();

    int replaceSlotItems(ItemProvider items, SlotSelector slotSelector);

    default int replaceSlotItems(ItemProvider items) {
        return replaceSlotItems(items, SlotSelector.ANY_SLOT);
    }

    void modifySlots(Consumer<? super SlotAccess> consumer, SlotSelector slotSelector);

    default void modifySlots(Consumer<? super SlotAccess> consumer) {
        modifySlots(consumer, SlotSelector.ANY_SLOT);
    }

    default SlotCollection filter(Predicate<? super ItemStack> predicate) {
        return new Filtered(this, predicate);
    }

    default SlotCollection flatMap(Function<ItemStack, ? extends SlotCollection> mapper) {
        return new FlatMapped(this, mapper);
    }

    default SlotCollection limit(int limit) {
        return new Limited(this, limit);
    }

    static SlotCollection of(SlotAccess slotAccess) {
        return new SlotCollection() {
            @Override
            public Stream<ItemStack> itemCopies() {
                return Stream.of(slotAccess.get().copy());
            }

            @Override
            public int size() {
                return 1;
            }

            @Override
            public int replaceSlotItems(ItemProvider items, SlotSelector slotSelector) {
                if (items.hasNext() && slotSelector.trySelectSlot(slotAccess.get())) {
                    return slotAccess.set(items.next()) ? 1 : 0;
                }
                return 0;
            }

            @Override
            public void modifySlots(Consumer<? super SlotAccess> consumer, SlotSelector slotSelector) {
                if (slotSelector.trySelectSlot(slotAccess.get())) {
                    consumer.accept(slotAccess);
                }
            }
        };
    }

    static SlotCollection of(Collection<? extends SlotAccess> slots) {
        if (slots.isEmpty()) {
            return EMPTY;
        }
        if (slots.size() == 1) {
            return of(slots.iterator().next());
        }
        return new SlotCollection() {
            @Override
            public Stream<ItemStack> itemCopies() {
                return slots.stream().map(SlotAccess::get).map(ItemStack::copy);
            }

            @Override
            public int size() {
                return slots.size();
            }

            @Override
            public int replaceSlotItems(ItemProvider items, SlotSelector slotSelector) {
                int replaced = 0;
                Iterator<? extends SlotAccess> iterator = slots.iterator();
                while (iterator.hasNext() && items.hasNext()) {
                    SlotAccess slot = iterator.next();
                    if (slotSelector.trySelectSlot(slot.get()) && slot.set(items.next())) {
                        replaced++;
                    }
                }
                return replaced;
            }

            @Override
            public void modifySlots(Consumer<? super SlotAccess> consumer, SlotSelector slotSelector) {
                for (SlotAccess slot : slots) {
                    if (slotSelector.trySelectSlot(slot.get())) {
                        consumer.accept(slot);
                    }
                }
            }
        };
    }

    static SlotCollection concat(SlotCollection first, SlotCollection second) {
        return concat(List.of(first, second));
    }

    static SlotCollection concat(List<? extends SlotCollection> terms) {
        if (terms.isEmpty()) {
            return EMPTY;
        }
        if (terms.size() == 1) {
            return terms.getFirst();
        }
        return new SlotCollection() {
            @Override
            public Stream<ItemStack> itemCopies() {
                return terms.stream().flatMap(SlotCollection::itemCopies);
            }

            @Override
            public int size() {
                return terms.stream().mapToInt(SlotCollection::size).sum();
            }

            @Override
            public int replaceSlotItems(ItemProvider items, SlotSelector slotSelector) {
                int replaced = 0;
                for (SlotCollection term : terms) {
                    replaced += term.replaceSlotItems(items, slotSelector);
                }
                return replaced;
            }

            @Override
            public void modifySlots(Consumer<? super SlotAccess> consumer, SlotSelector slotSelector) {
                for (SlotCollection term : terms) {
                    term.modifySlots(consumer, slotSelector);
                }
            }
        };
    }

    public static record Filtered(SlotCollection slots, Predicate<? super ItemStack> filter) implements SlotCollection {
        @Override
        public Stream<ItemStack> itemCopies() {
            return slots.itemCopies().filter(filter);
        }

        @Override
        public int size() {
            return (int) itemCopies().count();
        }

        @Override
        public int replaceSlotItems(ItemProvider items, SlotSelector slotSelector) {
            return slots.replaceSlotItems(items, slotSelector.filter(filter));
        }

        @Override
        public void modifySlots(Consumer<? super SlotAccess> consumer, SlotSelector slotSelector) {
            slots.modifySlots(consumer, slotSelector.filter(filter));
        }

        @Override
        public SlotCollection filter(Predicate<? super ItemStack> predicate) {
            Objects.requireNonNull(predicate);
            return new Filtered(slots, stack -> filter.test(stack) && predicate.test(stack));
        }
    }

    public static record FlatMapped(SlotCollection slots, Function<ItemStack, ? extends SlotCollection> mapper) implements SlotCollection {
        @Override
        public Stream<ItemStack> itemCopies() {
            return slots.itemCopies().map(mapper).flatMap(SlotCollection::itemCopies);
        }

        @Override
        public int size() {
            return slots.itemCopies().map(mapper).mapToInt(SlotCollection::size).sum();
        }

        @Override
        public int replaceSlotItems(ItemProvider items, SlotSelector slotSelector) {
            MutableInt replaced = new MutableInt();
            slots.modifySlots(slot -> replaced.add(mapper.apply(slot.get()).replaceSlotItems(items, slotSelector)));
            return replaced.intValue();
        }

        @Override
        public void modifySlots(Consumer<? super SlotAccess> consumer, SlotSelector slotSelector) {
            slots.modifySlots(slot -> mapper.apply(slot.get()).modifySlots(consumer, slotSelector));
        }
    }

    public static record Limited(SlotCollection slots, int limit) implements SlotCollection {
        @Override
        public Stream<ItemStack> itemCopies() {
            return slots.itemCopies().limit(limit);
        }

        @Override
        public int size() {
            return Math.min(slots.size(), limit);
        }

        @Override
        public int replaceSlotItems(ItemProvider items, SlotSelector slotSelector) {
            return slots.replaceSlotItems(items, slotSelector.limit(limit));
        }

        @Override
        public void modifySlots(Consumer<? super SlotAccess> consumer, SlotSelector slotSelector) {
            slots.modifySlots(consumer, slotSelector.limit(limit));
        }

        @Override
        public SlotCollection limit(int limit) {
            return new Limited(slots, Math.min(this.limit, limit));
        }
    }
}
