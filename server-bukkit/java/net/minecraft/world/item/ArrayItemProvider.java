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

import java.util.NoSuchElementException;
public class ArrayItemProvider implements ItemProvider {
    protected final ItemStack[] items;
    protected int position;
    private ItemStack peeked;

    public ArrayItemProvider(ItemStack[] items) {
        this.items = items;
    }

    @Override
    public boolean hasNext() {
        return position < items.length;
    }

    @Override
    public void restart() {
        setPosition(0);
    }

    @Override
    public final ItemStack next() {
        if (!hasNext()) {
            throw new NoSuchElementException();
        }
        ItemStack result = peeked != null ? peeked : computeNextItem();
        peeked = null;
        position++;
        return result;
    }

    @Override
    public final ItemStack peek() {
        if (!hasNext()) {
            throw new NoSuchElementException();
        }
        if (peeked == null) {
            peeked = computeNextItem();
        }
        return peeked;
    }

    protected ItemStack computeNextItem() {
        return items[position].copy();
    }

    @Override
    public boolean findNext(Predicate<ItemStack> predicate) {
        for (int i = position; i < items.length; i++) {
            if (predicate.test(items[i])) {
                setPosition(i);
                return true;
            }
        }
        return false;
    }

    protected void setPosition(int newPosition) {
        if (position != newPosition) {
            position = newPosition;
            peeked = null;
        }
    }

    public static class Cycled extends ArrayItemProvider {
        public Cycled(ItemStack[] items) {
            super(items);
        }

        @Override
        public boolean hasNext() {
            return items.length > 0;
        }

        @Override
        protected ItemStack computeNextItem() {
            return items[position % items.length].copy();
        }

        @Override
        public boolean findNext(Predicate<ItemStack> predicate) {
            for (int offset = 0; offset < items.length; offset++) {
                if (predicate.test(items[(position + offset) % items.length])) {
                    setPosition(position + offset);
                    return true;
                }
            }
            return false;
        }
    }
}
