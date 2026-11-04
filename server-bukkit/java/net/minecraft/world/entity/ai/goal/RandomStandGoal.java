// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.ai.goal;

import net.minecraft.world.entity.animal.equine.AbstractHorse;

public class RandomStandGoal extends Goal {
    private final AbstractHorse horse;
    private int nextStand;

    public RandomStandGoal(AbstractHorse horse) {
        this.horse = horse;
        resetStandInterval(horse);
    }

    @Override
    public void start() {
        horse.standIfPossible();
        horse.playSound(horse.getAmbientStandSound(), horse.getSoundVolume(), horse.getVoicePitch());
    }

    @Override
    public boolean canContinueToUse() {
        return false;
    }

    @Override
    public boolean canUse() {
        nextStand--;
        if (nextStand > 0) {
            return false;
        }
        resetStandInterval(horse);
        return true;
    }

    private void resetStandInterval(AbstractHorse horse) {
        nextStand = reducedTickDelay(horse.getAmbientStandInterval());
    }

    @Override
    public boolean requiresUpdateEveryTick() {
        return true;
    }
}
