// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.item;

import java.util.*;
import java.util.function.*;
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

import com.mojang.serialization.DataResult;
import net.minecraft.util.ARGB;
import net.minecraft.util.Prediction;
import net.minecraft.world.entity.SlotAccess;
import net.minecraft.world.entity.item.ItemEntity;
import net.minecraft.world.inventory.ClickAction;
import net.minecraft.world.inventory.Slot;
import net.minecraft.world.inventory.tooltip.BundleTooltip;
import net.minecraft.world.inventory.tooltip.TooltipComponent;
import org.apache.commons.lang3.math.Fraction;
public class BundleItem extends Item {
    public static final int MAX_SHOWN_GRID_ITEMS_X = 4;
    public static final int MAX_SHOWN_GRID_ITEMS_Y = 3;
    public static final int MAX_SHOWN_GRID_ITEMS = 12;
    public static final int OVERFLOWING_MAX_SHOWN_GRID_ITEMS = 11;
    private static final int kFullBarColor = ARGB.colorFromFloat(1.0F, 1.0F, 0.33F, 0.33F);
    private static final int kBarColor = ARGB.colorFromFloat(1.0F, 0.44F, 0.53F, 1.0F);
    private static final int kTicksAfterFirstThrow = 10;
    private static final int kTicksMaxThrowDuration = 200;

    public BundleItem(Item.Properties properties) {
        super(properties);
    }

    private static Fraction weightOf(BundleContents contents) {
        DataResult<Fraction> weight = contents.weight();
        return weight.result().orElse(Fraction.ONE);
    }

    private static BundleContents contentsOf(ItemStack stack) {
        return stack.getOrDefault(DataComponents.BUNDLE_CONTENTS, BundleContents.EMPTY);
    }

    public static float getFullnessDisplay(ItemStack itemStack) {
        return weightOf(contentsOf(itemStack)).floatValue();
    }

    public boolean overrideStackedOnOther(ItemStack self, Slot slot, ClickAction clickAction, Player player) {
        BundleContents stored = self.get(DataComponents.BUNDLE_CONTENTS);
        if (stored == null) {
            return false;
        }
        boolean slotEmpty = slot.getItem().isEmpty();
        BundleContents.Mutable edit = stored.asMutable();
        if (clickAction == ClickAction.PRIMARY && !slotEmpty) {
            if (edit.tryTransfer(slot, player) > 0) {
                playInsertSound(player);
            } else {
                playInsertFailSound(player);
            }
            return commit(self, edit, player);
        }
        if (clickAction != ClickAction.SECONDARY || !slotEmpty) {
            return false;
        }
        ItemStack taken = edit.removeOne();
        if (taken != null) {
            ItemStack rest = slot.safeInsert(taken);
            if (rest.getCount() > 0) {
                edit.tryInsert(rest);
            } else {
                playRemoveOneSound(player);
            }
        }
        return commit(self, edit, player);
    }

    private boolean commit(ItemStack bundle, BundleContents.Mutable edit, Player player) {
        bundle.set(DataComponents.BUNDLE_CONTENTS, edit.toImmutable());
        broadcastChanges(player);
        return true;
    }

    public boolean overrideOtherStackedOnMe(ItemStack self, ItemStack other, Slot slot, ClickAction clickAction, Player player, SlotAccess carriedItem) {
        boolean carriedEmpty = other.isEmpty();
        if (clickAction == ClickAction.PRIMARY && carriedEmpty) {
            toggleSelectedItem(self, -1);
            return false;
        }
        BundleContents stored = self.get(DataComponents.BUNDLE_CONTENTS);
        if (stored == null) {
            return false;
        }
        BundleContents.Mutable edit = stored.asMutable();
        if (clickAction == ClickAction.PRIMARY) {
            boolean added = slot.allowModification(player) && edit.tryInsert(other) > 0;
            if (added) {
                playInsertSound(player);
            } else {
                playInsertFailSound(player);
            }
            return commit(self, edit, player);
        }
        if (clickAction != ClickAction.SECONDARY || !carriedEmpty) {
            toggleSelectedItem(self, -1);
            return false;
        }
        ItemStack taken = slot.allowModification(player) ? edit.removeOne() : null;
        if (taken != null) {
            playRemoveOneSound(player);
            carriedItem.set(taken);
        }
        return commit(self, edit, player);
    }

    @Override
    public InteractionResult use(Level level, Player player, InteractionHand hand) {
        player.startUsingItem(hand);
        return InteractionResult.SUCCESS;
    }

    private void dropContent(Level level, Player player, ItemStack itemStack) {
        if (dropContent(itemStack, player)) {
            level.playSound(null, player.blockPosition(), SoundEvents.BUNDLE_DROP_CONTENTS, SoundSource.PLAYERS, 0.8F,
                0.8F + level.getRandom().nextFloat() * 0.4F);
            player.awardStat(Stats.ITEM_USED.get(this));
        }
    }

    @Override
    public boolean isBarVisible(ItemStack stack) {
        return weightOf(contentsOf(stack)).compareTo(Fraction.ZERO) > 0;
    }

    @Override
    public int getBarWidth(ItemStack stack) {
        return Math.min(1 + Mth.mulAndTruncate(weightOf(contentsOf(stack)), 12), 13);
    }

    @Override
    public int getBarColor(ItemStack stack) {
        return weightOf(contentsOf(stack)).compareTo(Fraction.ONE) >= 0 ? kFullBarColor : kBarColor;
    }

    public static void toggleSelectedItem(ItemStack stack, int selectedItem) {
        BundleContents stored = stack.get(DataComponents.BUNDLE_CONTENTS);
        if (stored == null) {
            return;
        }
        BundleContents.Mutable edit = stored.asMutable();
        edit.toggleSelectedItem(selectedItem);
        stack.set(DataComponents.BUNDLE_CONTENTS, edit.toImmutable());
    }

    public static int getSelectedItemIndex(ItemStack stack) {
        return contentsOf(stack).getSelectedItemIndex();
    }

    public static ItemStackTemplate getSelectedItem(ItemStack stack) {
        return contentsOf(stack).getSelectedItem();
    }

    public static int getNumberOfItemsToShow(ItemStack stack) {
        return contentsOf(stack).getNumberOfItemsToShow();
    }

    private boolean dropContent(ItemStack bundle, Player player) {
        BundleContents stored = bundle.get(DataComponents.BUNDLE_CONTENTS);
        if (stored == null || stored.isEmpty()) {
            return false;
        }
        BundleContents.Mutable edit = stored.asMutable();
        ItemStack thrown = edit.removeOne();
        if (thrown == null) {
            return false;
        }
        playRemoveOneSound(player);
        bundle.set(DataComponents.BUNDLE_CONTENTS, edit.toImmutable());
        player.drop(thrown, true, Prediction.PREDICTED);
        return true;
    }

    @Override
    public void onUseTick(Level level, LivingEntity livingEntity, ItemStack itemStack, int ticksRemaining) {
        if (livingEntity instanceof Player player) {
            int duration = getUseDuration(itemStack, livingEntity);
            if (ticksRemaining == duration || ticksRemaining < duration - kTicksAfterFirstThrow && ticksRemaining % 2 == 0) {
                dropContent(level, player, itemStack);
            }
        }
    }

    @Override
    public int getUseDuration(ItemStack itemStack, LivingEntity entity) {
        return kTicksMaxThrowDuration;
    }

    @Override
    public ItemUseAnimation getUseAnimation(ItemStack itemStack) {
        return ItemUseAnimation.BUNDLE;
    }

    @Override
    public Optional<TooltipComponent> getTooltipImage(ItemStack bundle) {
        TooltipDisplay display = bundle.getOrDefault(DataComponents.TOOLTIP_DISPLAY, TooltipDisplay.DEFAULT);
        if (!display.shows(DataComponents.BUNDLE_CONTENTS)) {
            return Optional.empty();
        }
        return Optional.ofNullable(bundle.get(DataComponents.BUNDLE_CONTENTS)).map(BundleTooltip::new);
    }

    @Override
    public void onDestroyed(ItemEntity entity) {
        BundleContents contents = entity.getItem().get(DataComponents.BUNDLE_CONTENTS);
        if (contents != null) {
            entity.getItem().set(DataComponents.BUNDLE_CONTENTS, BundleContents.EMPTY);
            ItemUtils.onContainerDestroyed(entity, contents.itemCopies());
        }
    }

    private static void playRemoveOneSound(Entity entity) {
        entity.playSound(SoundEvents.BUNDLE_REMOVE_ONE, 0.8F, 0.8F + entity.level().getRandom().nextFloat() * 0.4F);
    }

    private static void playInsertSound(Entity entity) {
        entity.playSound(SoundEvents.BUNDLE_INSERT, 0.8F, 0.8F + entity.level().getRandom().nextFloat() * 0.4F);
    }

    private static void playInsertFailSound(Entity entity) {
        entity.playSound(SoundEvents.BUNDLE_INSERT_FAIL, 1.0F, 1.0F);
    }

    private void broadcastChanges(Player player) {
        if (player.containerMenu != null) {
            player.containerMenu.slotsChanged(player.getInventory());
        }
    }
}
