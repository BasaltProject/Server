package org.basaltmc.bukkit.entity.ops;

import org.basaltmc.bukkit.entity.Cored;
import org.bukkit.entity.Entity;
import org.bukkit.entity.FishHook;
import org.bukkit.inventory.EquipmentSlot;

public interface FishHookOps extends FishHook, Cored {
    default int getMinWaitTime() {
        return core().integer("hook_min_wait");
    }

    default void setMinWaitTime(int minWaitTime) {
        if (minWaitTime < 0 || minWaitTime > getMaxWaitTime()) {
            throw new IllegalArgumentException("The minimum wait time should be between 0 and " + getMaxWaitTime());
        }
        core().set("hook_min_wait", minWaitTime);
    }

    default int getMaxWaitTime() {
        return core().integer("hook_max_wait");
    }

    default void setMaxWaitTime(int maxWaitTime) {
        if (maxWaitTime < 0 || maxWaitTime < getMinWaitTime()) {
            throw new IllegalArgumentException("The maximum wait time should be at least " + getMinWaitTime());
        }
        core().set("hook_max_wait", maxWaitTime);
    }

    default void setWaitTime(int min, int max) {
        if (min < 0 || max < 0 || min > max) {
            throw new IllegalArgumentException("The wait times must be positive and ordered");
        }
        core().set("hook_min_wait", min);
        core().set("hook_max_wait", max);
    }

    default int getMinLureTime() {
        return core().integer("hook_min_lure");
    }

    default void setMinLureTime(int minLureTime) {
        if (minLureTime < 0 || minLureTime > getMaxLureTime()) {
            throw new IllegalArgumentException("The minimum lure time should be between 0 and " + getMaxLureTime());
        }
        core().set("hook_min_lure", minLureTime);
    }

    default int getMaxLureTime() {
        return core().integer("hook_max_lure");
    }

    default void setMaxLureTime(int maxLureTime) {
        if (maxLureTime < 0 || maxLureTime < getMinLureTime()) {
            throw new IllegalArgumentException("The maximum lure time should be at least " + getMinLureTime());
        }
        core().set("hook_max_lure", maxLureTime);
    }

    default void setLureTime(int min, int max) {
        if (min < 0 || max < 0 || min > max) {
            throw new IllegalArgumentException("The lure times must be positive and ordered");
        }
        core().set("hook_min_lure", min);
        core().set("hook_max_lure", max);
    }

    default float getMinLureAngle() {
        return (float) core().real("hook_min_angle");
    }

    default void setMinLureAngle(float minLureAngle) {
        if (minLureAngle > getMaxLureAngle()) {
            throw new IllegalArgumentException("The minimum lure angle should not pass the maximum");
        }
        core().set("hook_min_angle", (double) minLureAngle);
    }

    default float getMaxLureAngle() {
        return (float) core().real("hook_max_angle");
    }

    default void setMaxLureAngle(float maxLureAngle) {
        if (maxLureAngle < getMinLureAngle()) {
            throw new IllegalArgumentException("The maximum lure angle should not be under the minimum");
        }
        core().set("hook_max_angle", (double) maxLureAngle);
    }

    default void setLureAngle(float min, float max) {
        if (min > max) {
            throw new IllegalArgumentException("The minimum lure angle should not pass the maximum");
        }
        core().set("hook_min_angle", (double) min);
        core().set("hook_max_angle", (double) max);
    }

    default boolean getApplyLure() {
        return core().flag("hook_apply_lure");
    }

    default void setApplyLure(boolean applyLure) {
        core().set("hook_apply_lure", applyLure);
    }

    default double getBiteChance() {
        double chance = core().real("hook_bite_chance");
        if (chance >= 0.0) {
            return chance;
        }
        return getWorld().hasStorm() ? 0.0033333333333333335 : 0.002;
    }

    default void setBiteChance(double chance) {
        if (chance < 0.0 || chance > 1.0) {
            throw new IllegalArgumentException("The bite chance must be between 0 and 1");
        }
        core().set("hook_bite_chance", chance);
    }

    default boolean isInOpenWater() {
        return core().flag("hook_open_water");
    }

    default Entity getHookedEntity() {
        return core().entity(core().integer("hook_hooked"));
    }

    default void setHookedEntity(Entity entity) {
        core().set("hook_hooked", entity == null ? -1 : entity.getEntityId());
    }

    default boolean pullHookedEntity() {
        return core().test("hook_pull");
    }

    default boolean isSkyInfluenced() {
        return core().flag("hook_sky_influenced");
    }

    default void setSkyInfluenced(boolean skyInfluenced) {
        core().set("hook_sky_influenced", skyInfluenced);
    }

    default boolean isRainInfluenced() {
        return core().flag("hook_rain_influenced");
    }

    default void setRainInfluenced(boolean rainInfluenced) {
        core().set("hook_rain_influenced", rainInfluenced);
    }

    default FishHook.HookState getState() {
        return FishHook.HookState.values()[Math.max(0, Math.min(2, core().integer("hook_phase")))];
    }

    default int getWaitTime() {
        return core().integer("hook_wait");
    }

    default void setWaitTime(int ticks) {
        core().set("hook_wait", ticks);
    }

    default int getTimeUntilBite() {
        return core().integer("hook_bite_in");
    }

    default void setTimeUntilBite(int ticks) {
        if (ticks < 1) {
            throw new IllegalArgumentException("Cannot set time until bite to less than 1 (" + ticks + "<1)");
        }
        core().set("hook_bite_in", ticks);
    }

    default void resetFishingState() {
        core().call("hook_reset");
    }

    default int retrieve(EquipmentSlot slot) {
        if (slot != EquipmentSlot.HAND && slot != EquipmentSlot.OFF_HAND) {
            throw new IllegalArgumentException("Equipment slot must be HAND or OFF_HAND");
        }
        throw org.basaltmc.bukkit.Unsupported.hit("FishHook#retrieve");
    }
}
