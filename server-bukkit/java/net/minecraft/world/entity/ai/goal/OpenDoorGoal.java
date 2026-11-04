// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.ai.goal;

import net.minecraft.world.entity.Mob;

public class OpenDoorGoal extends DoorInteractGoal {
    private final boolean closeDoor;
    private int forgetTime;

    public OpenDoorGoal(Mob mob, boolean closeDoor) {
        super(mob);
        this.mob = mob;
        this.closeDoor = closeDoor;
    }

    @Override
    public boolean canContinueToUse() {
        return closeDoor && forgetTime > 0 && super.canContinueToUse();
    }

    @Override
    public void start() {
        forgetTime = 20;
        setOpen(true);
        super.start();
    }

    @Override
    public void stop() {
        if (closeDoor) {
            setOpen(false);
        }
    }

    @Override
    public void tick() {
        forgetTime--;
        super.tick();
    }
}
