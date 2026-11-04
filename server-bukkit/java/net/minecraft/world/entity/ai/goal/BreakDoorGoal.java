// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.ai.goal;

import java.util.function.Predicate;
import net.minecraft.world.Difficulty;
import net.minecraft.world.entity.Mob;

public class BreakDoorGoal extends DoorInteractGoal {
    private static final int DEFAULT_DOOR_BREAK_TIME = 240;
    private final Predicate<Difficulty> validDifficulties;
    protected int breakTime;
    protected int lastBreakProgress = -1;
    protected int doorBreakTime = -1;

    public BreakDoorGoal(Mob mob, Predicate<Difficulty> validDifficulties) {
        super(mob);
        this.validDifficulties = validDifficulties;
    }

    public BreakDoorGoal(Mob mob, int seconds, Predicate<Difficulty> validDifficulties) {
        this(mob, validDifficulties);
        this.doorBreakTime = seconds;
    }

    protected int getDoorBreakTime() {
        return Math.max(DEFAULT_DOOR_BREAK_TIME, doorBreakTime);
    }

    @Override
    public boolean canUse() {
        return super.canUse() && validDifficulties.test(mob.level().getDifficulty())
                && !isOpen() && hasDoor;
    }

    @Override
    public void start() {
        super.start();
        breakTime = 0;
    }

    @Override
    public boolean canContinueToUse() {
        return breakTime <= getDoorBreakTime() && !isOpen() && doorPos.closerToCenterThan(mob.position(), 2.0)
                && validDifficulties.test(mob.level().getDifficulty());
    }

    @Override
    public void stop() {
        super.stop();
        mob.level().destroyBlockProgress(mob.getId(), doorPos, -1);
    }

    @Override
    public void tick() {
        super.tick();
        if (mob.getRandom().nextInt(20) == 0) {
            mob.level().levelEvent(1019, doorPos, 0);
            {
                mob.swingForAttack(net.minecraft.world.InteractionHand.MAIN_HAND);
            }
        }
        breakTime++;
        int progress = (int) (breakTime / (float) getDoorBreakTime() * 10.0F);
        if (progress != lastBreakProgress) {
            mob.level().destroyBlockProgress(mob.getId(), doorPos, progress);
            lastBreakProgress = progress;
        }
        if (breakTime == getDoorBreakTime() && validDifficulties.test(mob.level().getDifficulty())) {
            mob.level().removeBlock(doorPos, false);
            mob.level().levelEvent(1021, doorPos, 0);
            mob.level().levelEvent(2001, doorPos, net.minecraft.world.level.block.Block.getId(mob.level().getBlockState(doorPos)));
        }
    }
}
