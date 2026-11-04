// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world;

import net.minecraft.world.entity.Entity;

/** The tick rate of the server, read and written through the Bukkit tick manager of the native server. */
public class TickRateManager {
    public static final float MIN_TICKRATE = 1.0F;
    protected float tickrate;
    protected long nanosecondsPerTick;
    protected int frozenTicksToRun;
    protected boolean runGameElements;
    protected boolean isFrozen;

    public TickRateManager() {
    }

    protected static org.bukkit.ServerTickManager manager() {
        return org.bukkit.Bukkit.getServerTickManager();
    }

    public void setTickRate(float rate) {
        manager().setTickRate(Math.max(rate, MIN_TICKRATE));
    }

    public float tickrate() {
        return manager().getTickRate();
    }

    public float millisecondsPerTick() {
        return 1000.0F / tickrate();
    }

    public long nanosecondsPerTick() {
        return (long) (1.0E9 / tickrate());
    }

    public boolean runsNormally() {
        return !manager().isFrozen() || manager().isStepping();
    }

    public boolean isSteppingForward() {
        return manager().isStepping();
    }

    public void setFrozenTicksToRun(int timeout) {
        manager().stepGameIfFrozen(timeout);
    }

    public int frozenTicksToRun() {
        return manager().isStepping() ? manager().getFrozenTicksToRun() : 0;
    }

    public void setFrozen(boolean state) {
        manager().setFrozen(state);
    }

    public boolean isFrozen() {
        return manager().isFrozen();
    }

    public void tick() {
    }

    public boolean isEntityFrozen(Entity entity) {
        return manager().isFrozen() && !manager().isStepping();
    }
}
