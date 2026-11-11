// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.block.entity.vault;

import net.minecraft.util.Mth;

public class VaultClientData {
    public static final float ROTATION_SPEED = 10.0F;

    private float spin;
    private float lastSpin;

    public float currentSpin() {
        return spin;
    }

    public float previousSpin() {
        return lastSpin;
    }

    public void updateDisplayItemSpin() {
        lastSpin = spin;
        spin = Mth.wrapDegrees(spin + ROTATION_SPEED);
    }
}
