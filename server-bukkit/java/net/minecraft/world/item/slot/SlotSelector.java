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
import net.minecraft.world.item.ItemStack;
import org.apache.commons.lang3.mutable.MutableInt;
public interface SlotSelector {
    SlotSelector ANY_SLOT = new FilteringSelector(null);
    SlotSelector EMPTY_SLOTS = selecting(ItemStack::isEmpty);
    SlotSelector NON_EMPTY_SLOTS = selecting(stack -> !stack.isEmpty());

    boolean trySelectSlot(ItemStack itemInSlot);

    SlotSelector filter(Predicate<? super ItemStack> predicate);

    SlotSelector limit(int limit);

    static SlotSelector selecting(Predicate<? super ItemStack> predicate) {
        return new FilteringSelector(predicate);
    }

    static SlotSelector tracking(SlotSelector slotSelector, MutableInt selectedCount) {
        return new TrackingSelector(slotSelector, Integer.MAX_VALUE, selectedCount);
    }

    static SlotSelector tracking(MutableInt selectedCount) {
        return tracking(ANY_SLOT, selectedCount);
    }

    public static record FilteringSelector(Predicate<? super ItemStack> filter) implements SlotSelector {
        @Override
        public boolean trySelectSlot(ItemStack itemInSlot) {
            return filter == null || filter.test(itemInSlot);
        }

        @Override
        public SlotSelector filter(Predicate<? super ItemStack> predicate) {
            return filter == null ? new FilteringSelector(predicate) : new FilteringSelector(stack -> filter.test(stack) && predicate.test(stack));
        }

        @Override
        public SlotSelector limit(int limit) {
            return new TrackingSelector(this, limit, new MutableInt());
        }
    }

    public static record TrackingSelector(SlotSelector slotSelector, int limit, MutableInt selectedCount) implements SlotSelector {
        @Override
        public boolean trySelectSlot(ItemStack itemInSlot) {
            if (selectedCount.intValue() < limit && slotSelector.trySelectSlot(itemInSlot)) {
                selectedCount.increment();
                return true;
            }
            return false;
        }

        @Override
        public SlotSelector filter(Predicate<? super ItemStack> predicate) {
            return new TrackingSelector(slotSelector.filter(predicate), limit, selectedCount);
        }

        @Override
        public SlotSelector limit(int limit) {
            return limit < this.limit ? new TrackingSelector(slotSelector, limit, selectedCount) : this;
        }
    }
}
