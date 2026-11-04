// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.ai.behavior;

import java.util.Map;
import net.minecraft.core.component.DataComponents;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.world.entity.LivingEntity;
import net.minecraft.world.entity.Mob;
import net.minecraft.world.entity.ai.memory.MemoryModuleType;
import net.minecraft.world.entity.ai.memory.MemoryStatus;
import net.minecraft.world.entity.monster.CrossbowAttackMob;
import net.minecraft.world.entity.monster.RangedAttackMob;
import net.minecraft.world.entity.projectile.ProjectileUtil;
import net.minecraft.world.item.CrossbowItem;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.item.Items;
import net.minecraft.world.item.component.ChargedProjectiles;

public class CrossbowAttack<E extends Mob & CrossbowAttackMob, T extends LivingEntity> extends Behavior<E> {
    private static final int TIMEOUT = 1200;
    private int attackDelay;
    private CrossbowState crossbowState = CrossbowState.UNCHARGED;

    public CrossbowAttack() {
        super(Map.of(MemoryModuleType.LOOK_TARGET, MemoryStatus.REGISTERED, MemoryModuleType.ATTACK_TARGET, MemoryStatus.VALUE_PRESENT), TIMEOUT);
    }

    protected boolean checkExtraStartConditions(ServerLevel level, E body) {
        LivingEntity attackTarget = getAttackTarget(body);
        return body.isHolding(Items.CROSSBOW) && BehaviorUtils.canSee(body, attackTarget) && BehaviorUtils.isWithinAttackRange(body, attackTarget, 0);
    }

    protected boolean canStillUse(ServerLevel level, E body, long timestamp) {
        return body.getBrain().hasMemoryValue(MemoryModuleType.ATTACK_TARGET) && checkExtraStartConditions(level, body);
    }

    protected void tick(ServerLevel level, E body, long timestamp) {
        LivingEntity target = getAttackTarget(body);
        body.getBrain().setMemory(MemoryModuleType.LOOK_TARGET, new EntityTracker(target, true));
        crossbowAttack(body, target);
    }

    protected void stop(ServerLevel level, E body, long timestamp) {
        if (body.isUsingItem()) {
            body.stopUsingItem();
        }
        if (body.isHolding(Items.CROSSBOW)) {
            body.setChargingCrossbow(false);
            body.getUseItem().set(DataComponents.CHARGED_PROJECTILES, ChargedProjectiles.EMPTY);
        }
    }

    private void crossbowAttack(E body, LivingEntity target) {
        if (crossbowState == CrossbowState.UNCHARGED) {
            body.startUsingItem(ProjectileUtil.getWeaponHoldingHand(body, Items.CROSSBOW));
            crossbowState = CrossbowState.CHARGING;
            body.setChargingCrossbow(true);
        } else if (crossbowState == CrossbowState.CHARGING) {
            if (!body.isUsingItem()) {
                crossbowState = CrossbowState.UNCHARGED;
            }
            ItemStack useItem = body.getUseItem();
            if (body.getTicksUsingItem() >= CrossbowItem.getChargeDuration(useItem, body)) {
                body.releaseUsingItem();
                crossbowState = CrossbowState.CHARGED;
                attackDelay = 20 + body.getRandom().nextInt(20);
                body.setChargingCrossbow(false);
            }
        } else if (crossbowState == CrossbowState.CHARGED) {
            if (--attackDelay == 0) {
                crossbowState = CrossbowState.READY_TO_ATTACK;
            }
        } else if (crossbowState == CrossbowState.READY_TO_ATTACK) {
            ((RangedAttackMob) body).performRangedAttack(target, 1.0F);
            crossbowState = CrossbowState.UNCHARGED;
        }
    }

    private static LivingEntity getAttackTarget(LivingEntity body) {
        return body.getBrain().getMemory(MemoryModuleType.ATTACK_TARGET).get();
    }

    private static enum CrossbowState {
        UNCHARGED, CHARGING, CHARGED, READY_TO_ATTACK;
    }
}
