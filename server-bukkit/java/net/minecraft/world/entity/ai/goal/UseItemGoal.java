// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.ai.goal;

import java.util.function.Predicate;
import net.minecraft.sounds.SoundEvent;
import net.minecraft.world.InteractionHand;
import net.minecraft.world.entity.EquipmentSlot;
import net.minecraft.world.entity.Mob;
import net.minecraft.world.item.ItemStack;

public class UseItemGoal<T extends Mob> extends Goal {
    private final T mob;
    private final ItemStack item;
    private final Predicate<? super T> canUseSelector;
    private final SoundEvent finishUsingSound;

    public UseItemGoal(T mob, ItemStack item, SoundEvent finishUsingSound, Predicate<? super T> canUseSelector) {
        this.mob = mob;
        this.item = item;
        this.finishUsingSound = finishUsingSound;
        this.canUseSelector = canUseSelector;
    }

    @Override
    public boolean canUse() {
        return canUseSelector.test(mob);
    }

    @Override
    public boolean canContinueToUse() {
        return mob.isUsingItem();
    }

    @Override
    public void start() {
        mob.setItemSlot(EquipmentSlot.MAINHAND, item.copy());
        mob.startUsingItem(InteractionHand.MAIN_HAND);
    }

    @Override
    public void stop() {
        mob.setItemSlot(EquipmentSlot.MAINHAND, ItemStack.EMPTY);
        if (finishUsingSound != null) {
            mob.playSound(finishUsingSound, 1.0F, mob.getRandom().nextFloat() * 0.2F + 0.9F);
        }
    }
}
