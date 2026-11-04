// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.ai.goal;

import net.minecraft.world.entity.monster.zombie.Zombie;

public class ZombieAttackGoal extends MeleeAttackGoal {
    private final Zombie zombie;
    private int raiseArmTicks;

    public ZombieAttackGoal(Zombie zombie, double speedModifier, boolean trackTarget) {
        super(zombie, speedModifier, trackTarget);
        this.zombie = zombie;
    }

    @Override
    public void start() {
        super.start();
        raiseArmTicks = 0;
    }

    @Override
    public void stop() {
        super.stop();
        zombie.setAggressive(false);
    }

    @Override
    public void tick() {
        super.tick();
        raiseArmTicks++;
        zombie.setAggressive(raiseArmTicks >= 5 && getTicksUntilNextAttack() < getAttackInterval() / 2);
    }
}
