// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.monster;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.entity.projectile.ProjectileUtil;
import net.minecraft.world.item.CrossbowItem;
import net.minecraft.world.item.Items;
import net.minecraft.world.InteractionHand;
import net.minecraft.world.entity.LivingEntity;
public interface CrossbowAttackMob extends RangedAttackMob {
    void setChargingCrossbow(boolean isCharging);
    LivingEntity getTarget();
    void onCrossbowAttackPerformed();
    default void performCrossbowAttack(LivingEntity body, float crossbowPower) {
        InteractionHand hand = ProjectileUtil.getWeaponHoldingHand(body, Items.CROSSBOW);
        ItemStack held = body.getItemInHand(hand);
        if (held.getItem() instanceof CrossbowItem crossbow) {
            crossbow.performShooting(body.level(), body, hand, held, crossbowPower, rangedAttackUncertainty(body.level()), getTarget());
        }
        onCrossbowAttackPerformed();
    }
}
