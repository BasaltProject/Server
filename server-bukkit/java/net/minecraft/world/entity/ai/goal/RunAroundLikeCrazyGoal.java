// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.ai.goal;

import java.util.EnumSet;
import net.minecraft.world.entity.animal.equine.AbstractHorse;
import net.minecraft.world.entity.ai.util.DefaultRandomPos;
import net.minecraft.world.entity.player.Player;
import net.minecraft.world.phys.Vec3;

public class RunAroundLikeCrazyGoal extends Goal {
    private final AbstractHorse horse;
    private final double speedModifier;
    private double posX;
    private double posY;
    private double posZ;

    public RunAroundLikeCrazyGoal(AbstractHorse mob, double speedModifier) {
        this.horse = mob;
        this.speedModifier = speedModifier;
        setFlags(EnumSet.of(Goal.Flag.MOVE));
    }

    @Override
    public boolean canUse() {
        if (horse.isTamed() || !horse.isVehicle()) {
            return false;
        }
        Vec3 pos = DefaultRandomPos.getPos(horse, 5, 4);
        if (pos == null) {
            return false;
        }
        posX = pos.x;
        posY = pos.y;
        posZ = pos.z;
        return true;
    }

    @Override
    public void start() {
        horse.getNavigation().moveTo(posX, posY, posZ, speedModifier);
    }

    @Override
    public boolean canContinueToUse() {
        return !horse.isTamed() && !horse.getNavigation().isDone() && horse.isVehicle();
    }

    @Override
    public void tick() {
        if (horse.isTamed() || horse.getRandom().nextInt(adjustedTickDelay(50)) != 0) {
            return;
        }
        if (horse.getFirstPassenger() instanceof Player rider) {
            int max = horse.getMaxTemper();
            int temper = horse.getTemper();
            if (max > 0 && horse.getRandom().nextInt(max) < temper) {
                horse.tameWithName(rider);
                return;
            }
            horse.modifyTemper(5);
        }
        horse.ejectPassengers();
        horse.makeMad();
    }
}
