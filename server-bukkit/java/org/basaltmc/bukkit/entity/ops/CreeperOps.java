package org.basaltmc.bukkit.entity.ops;

import org.basaltmc.bukkit.entity.Cored;
import org.bukkit.entity.Creeper;

public interface CreeperOps extends Creeper, Cored {
    int kMaxFuse = 30;

    default boolean isPowered() {
        return core().flag("powered");
    }

    default void setPowered(boolean powered) {
        core().set("powered", powered);
    }

    default int getFuseTicks() {
        return core().integer("swell");
    }

    default void setFuseTicks(int ticks) {
        core().set("swell", Math.max(0, ticks));
    }

    default int getMaxFuseTicks() {
        return kMaxFuse;
    }

    default boolean isIgnited() {
        return getFuseTicks() > 0;
    }

    default void ignite() {
        setFuseTicks(1);
    }

    default void setIgnited(boolean ignited) {
        setFuseTicks(ignited ? 1 : 0);
    }

    default void explode() {
        setFuseTicks(kMaxFuse);
    }
}
