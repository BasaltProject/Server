// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.ai.goal;

import java.util.EnumSet;
import java.util.List;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.entity.EntityType;
import net.minecraft.world.entity.animal.equine.Llama;
import net.minecraft.world.phys.Vec3;

public class LlamaFollowCaravanGoal extends Goal {
    public final Llama llama;
    private double speedModifier;
    private int distCheckCounter;

    public LlamaFollowCaravanGoal(Llama llama, double speedModifier) {
        this.llama = llama;
        this.speedModifier = speedModifier;
        setFlags(EnumSet.of(Goal.Flag.MOVE));
    }

    private Llama closestWithoutTail(List<Entity> llamas, boolean leashedOnly) {
        Llama closest = null;
        double best = Double.MAX_VALUE;
        for (Entity entity : llamas) {
            Llama candidate = (Llama) entity;
            boolean eligible = leashedOnly ? candidate.isLeashed() : candidate.inCaravan();
            if (eligible && !candidate.hasCaravanTail()) {
                double distance = llama.distanceToSqr(candidate);
                if (distance <= best) {
                    best = distance;
                    closest = candidate;
                }
            }
        }
        return closest;
    }

    @Override
    public boolean canUse() {
        if (llama.isLeashed() || llama.inCaravan()) {
            return false;
        }
        List<Entity> llamas = llama.level().getEntities(llama, llama.getBoundingBox().inflate(9.0, 4.0, 9.0),
            other -> other instanceof Llama);
        Llama closest = closestWithoutTail(llamas, false);
        if (closest == null) {
            closest = closestWithoutTail(llamas, true);
        }
        if (closest == null || llama.distanceToSqr(closest) < 4.0) {
            return false;
        }
        if (!closest.isLeashed() && !headIsLeashed(closest, 1)) {
            return false;
        }
        llama.joinCaravan(closest);
        return true;
    }

    @Override
    public boolean canContinueToUse() {
        if (!llama.inCaravan() || !llama.getCaravanHead().isAlive() || !headIsLeashed(llama, 0)) {
            return false;
        }
        if (llama.distanceToSqr(llama.getCaravanHead()) > 676.0) {
            if (speedModifier <= 3.0) {
                speedModifier *= 1.2;
                distCheckCounter = reducedTickDelay(40);
                return true;
            }
            if (distCheckCounter == 0) {
                return false;
            }
        }
        if (distCheckCounter > 0) {
            distCheckCounter--;
        }
        return true;
    }

    @Override
    public void stop() {
        llama.leaveCaravan();
        speedModifier = 2.1;
    }

    @Override
    public void tick() {
        if (!llama.inCaravan()) {
            return;
        }
        Llama head = llama.getCaravanHead();
        double distance = llama.distanceTo(head);
        Vec3 delta = new Vec3(head.getX() - llama.getX(), head.getY() - llama.getY(), head.getZ() - llama.getZ())
            .normalize().scale(Math.max(distance - 2.0, 0.0));
        llama.getNavigation().moveTo(llama.getX() + delta.x, llama.getY() + delta.y, llama.getZ() + delta.z, speedModifier);
    }

    private boolean headIsLeashed(Llama current, int depth) {
        if (depth > 8 || !current.inCaravan()) {
            return false;
        }
        Llama head = current.getCaravanHead();
        return head.isLeashed() || headIsLeashed(head, depth + 1);
    }
}
