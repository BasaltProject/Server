// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.ai.goal;

import java.util.EnumSet;
import net.minecraft.tags.EntityTypeTags;
import net.minecraft.world.entity.LivingEntity;
import net.minecraft.world.entity.Mob;
import net.minecraft.world.entity.animal.golem.CopperGolem;
import net.minecraft.world.item.Items;
import net.minecraft.world.entity.animal.golem.IronGolem;
import net.minecraft.world.entity.ai.targeting.TargetingConditions;
import net.minecraft.world.phys.AABB;

public class OfferFlowerGoal extends Goal {
    private static final TargetingConditions kOfferTargeting = TargetingConditions.forNonCombat().range(6.0);
    public static final int OFFER_TICKS = 400;
    private final IronGolem golem;
    private LivingEntity entity;
    private int tick;

    public OfferFlowerGoal(IronGolem golem) {
        this.golem = golem;
        setFlags(EnumSet.of(Goal.Flag.MOVE, Goal.Flag.LOOK));
    }

    @Override
    public boolean canUse() {
        if (!golem.level().isBrightOutside() || golem.getRandom().nextInt(8000) != 0) {
            return false;
        }
        AABB area = golem.getBoundingBox().inflate(6.0, 2.0, 6.0);
        entity = null;
        double best = Double.MAX_VALUE;
        for (LivingEntity candidate : golem.level().getEntitiesOfClass(LivingEntity.class, area)) {
            double distance = golem.distanceToSqr(candidate);
            if (candidate.is(EntityTypeTags.CANDIDATE_FOR_IRON_GOLEM_GIFT) && distance < best
                    && kOfferTargeting.test(getServerLevel(golem), golem, candidate)) {
                best = distance;
                entity = candidate;
            }
        }
        return entity != null;
    }

    @Override
    public boolean canContinueToUse() {
        return tick > 0;
    }

    @Override
    public void start() {
        tick = adjustedTickDelay(OFFER_TICKS);
        golem.offerFlower(true);
    }

    @Override
    public void stop() {
        golem.offerFlower(false);
        if (tick == 0 && entity instanceof Mob mob && mob.is(EntityTypeTags.ACCEPTS_IRON_GOLEM_GIFT)
                && mob.getItemBySlot(CopperGolem.EQUIPMENT_SLOT_ANTENNA).isEmpty()
                && golem.getBoundingBox().inflate(6.0, 2.0, 6.0).intersects(mob.getBoundingBox())) {
            mob.setItemSlot(CopperGolem.EQUIPMENT_SLOT_ANTENNA, Items.POPPY.getDefaultInstance());
            mob.setGuaranteedDrop(CopperGolem.EQUIPMENT_SLOT_ANTENNA);
        }
        entity = null;
    }

    @Override
    public void tick() {
        if (entity != null) {
            golem.getLookControl().setLookAt(entity, 30.0F, 30.0F);
        }
        tick--;
    }
}
