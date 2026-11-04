// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.ai.control;

import net.minecraft.world.entity.Mob;

public class JumpControl implements Control {
    private final Mob mob;
    protected boolean jump;

    public JumpControl(Mob mob) {
        this.mob = mob;
    }

    public void jump() {
        jump = true;
    }

    public void tick() {
        mob.setJumping(jump);
        jump = false;
    }
}
