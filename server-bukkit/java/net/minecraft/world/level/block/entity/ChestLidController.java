// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.block.entity;

import net.minecraft.util.Mth;

public class ChestLidController {
    private static final float kStep = 0.1F;

    private boolean open;
    private float current;
    private float previous;

    public void tickLid() {
        previous = current;
        if (open) {
            current = Math.min(current + kStep, 1.0F);
        } else {
            current = Math.max(current - kStep, 0.0F);
        }
    }

    public float getOpenness(float a) {
        return Mth.lerp(a, previous, current);
    }

    public void shouldBeOpen(boolean shouldBeOpen) {
        this.open = shouldBeOpen;
    }
}
