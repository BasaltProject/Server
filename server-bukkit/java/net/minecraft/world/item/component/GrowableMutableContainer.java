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
import net.minecraft.world.item.ItemProvider;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.item.slot.SlotSelector;
public abstract class GrowableMutableContainer<T> extends SimpleMutableContainer<T> {
    public GrowableMutableContainer(List<ItemStack> items) {
        super(items);
    }

    public abstract boolean canInsertNewSlots();

    @Override
    public int replaceSlotItems(ItemProvider newItems, SlotSelector slotSelector) {
        int replaced = super.replaceSlotItems(newItems, slotSelector);
        while (newItems.hasNext() && canInsertNewSlots() && slotSelector.trySelectSlot(ItemStack.EMPTY) && addSlotWithItem(newItems)) {
            replaced++;
        }
        return replaced;
    }

    protected boolean addSlotWithItem(ItemProvider newItems) {
        items.add(newItems.next());
        return true;
    }
}
