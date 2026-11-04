// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.ai.goal;

import net.minecraft.server.level.ServerPlayer;
import net.minecraft.world.entity.animal.parrot.ShoulderRidingEntity;

public class LandOnOwnersShoulderGoal extends Goal {
    private final ShoulderRidingEntity entity;
    private boolean isSittingOnShoulder;

    public LandOnOwnersShoulderGoal(ShoulderRidingEntity entity) {
        this.entity = entity;
    }

    @Override
    public boolean canUse() {
        if (!(entity.getOwner() instanceof ServerPlayer owner)) {
            return false;
        }
        boolean fitsOnShoulder = !owner.isSpectator() && !owner.getAbilities().flying && !owner.isInWater()
                && !owner.isInPowderSnow;
        return !entity.isOrderedToSit() && fitsOnShoulder && entity.canSitOnShoulder();
    }

    @Override
    public boolean isInterruptable() {
        return !isSittingOnShoulder;
    }

    @Override
    public void start() {
        isSittingOnShoulder = false;
    }

    @Override
    public void tick() {
        if (!isSittingOnShoulder && !entity.isInSittingPose() && !entity.isLeashed()
                && entity.getBoundingBox().intersects(entity.getOwner().getBoundingBox())) {
            isSittingOnShoulder = entity.setEntityOnShoulder((ServerPlayer) entity.getOwner());
        }
    }
}
