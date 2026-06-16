package org.basaltmc.bukkit.misc;

import org.basaltmc.bukkit.MiscNatives;
import org.bukkit.ServerTickManager;
import org.bukkit.entity.Entity;

public final class BasaltTickManager implements ServerTickManager {
    @Override
    public boolean isRunningNormally() {
        return !MiscNatives.tmFrozen() && !MiscNatives.tmSprinting() || MiscNatives.tmStepsOwed() > 0;
    }

    @Override
    public boolean isStepping() {
        return MiscNatives.tmStepsOwed() > 0;
    }

    @Override
    public boolean isSprinting() {
        return MiscNatives.tmSprinting();
    }

    @Override
    public boolean isFrozen() {
        return MiscNatives.tmFrozen();
    }

    @Override
    public float getTickRate() {
        return MiscNatives.tmRate();
    }

    @Override
    public void setTickRate(float tick) {
        if (tick < 1.0F || tick > 10000.0F) {
            throw new IllegalArgumentException("Tick rate must be between 1.0 and 10000.0, got " + tick);
        }
        MiscNatives.tmSetRate(tick);
    }

    @Override
    public void setFrozen(boolean frozen) {
        MiscNatives.tmSetFrozen(frozen);
    }

    @Override
    public boolean stepGameIfFrozen(int ticks) {
        return MiscNatives.tmStep(ticks);
    }

    @Override
    public boolean stopStepping() {
        return MiscNatives.tmStopStepping();
    }

    @Override
    public boolean requestGameToSprint(int ticks) {
        return MiscNatives.tmSprint(ticks);
    }

    @Override
    public boolean stopSprinting() {
        return MiscNatives.tmStopSprinting();
    }

    @Override
    public boolean isFrozen(Entity entity) {
        return MiscNatives.tmFrozen() && MiscNatives.tmStepsOwed() == 0 && !(entity instanceof org.bukkit.entity.Player);
    }

    @Override
    public int getFrozenTicksToRun() {
        return MiscNatives.tmStepsOwed();
    }
}
