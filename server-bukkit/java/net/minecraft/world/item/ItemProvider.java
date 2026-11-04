// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.item;

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

import java.util.Collection;
import java.util.NoSuchElementException;
public interface ItemProvider {
    boolean hasNext();

    void restart();

    ItemStack next() throws NoSuchElementException;

    ItemStack peek() throws NoSuchElementException;

    boolean findNext(Predicate<ItemStack> predicate);

    default boolean findNextNonEmpty() {
        return findNext(stack -> !stack.isEmpty());
    }

    default ItemProvider orElseProvide(ItemStack fallbackItem) {
        return new FallbackItemProvider(this, fallbackItem);
    }

    static ItemProvider of(ItemStack... items) {
        return new ArrayItemProvider(items);
    }

    static ItemProvider of(Collection<ItemStack> items) {
        return of(items.toArray(new ItemStack[0]));
    }

    static ItemProvider cycle(ItemStack... items) {
        return new ArrayItemProvider.Cycled(items);
    }

    static ItemProvider cycle(Collection<ItemStack> items) {
        return cycle(items.toArray(new ItemStack[0]));
    }

    public static class FallbackItemProvider implements ItemProvider {
        private final ItemProvider itemProvider;
        private final ItemStack fallbackItem;
        private boolean usingFallback;

        public FallbackItemProvider(ItemProvider itemProvider, ItemStack fallbackItem) {
            this.itemProvider = itemProvider;
            this.fallbackItem = fallbackItem;
        }

        @Override
        public boolean hasNext() {
            return true;
        }

        @Override
        public void restart() {
            itemProvider.restart();
            usingFallback = false;
        }

        @Override
        public ItemProvider orElseProvide(ItemStack fallbackItem) {
            return this;
        }

        private boolean fallBack() {
            if (!usingFallback && !itemProvider.hasNext()) {
                usingFallback = true;
            }
            return usingFallback;
        }

        @Override
        public ItemStack peek() {
            return fallBack() ? fallbackItem.copy() : itemProvider.peek();
        }

        @Override
        public ItemStack next() {
            return fallBack() ? fallbackItem.copy() : itemProvider.next();
        }

        @Override
        public boolean findNext(Predicate<ItemStack> predicate) {
            if (usingFallback) {
                return predicate.test(fallbackItem);
            }
            if (itemProvider.findNext(predicate)) {
                return true;
            }
            if (predicate.test(fallbackItem)) {
                usingFallback = true;
                return true;
            }
            return false;
        }
    }
}
