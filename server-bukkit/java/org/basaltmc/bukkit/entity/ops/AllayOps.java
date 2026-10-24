package org.basaltmc.bukkit.entity.ops;

import org.basaltmc.bukkit.entity.Cored;
import org.bukkit.entity.Allay;

public interface AllayOps extends Allay, Cored {
    default boolean canDuplicate() {
        return core().property("allay_duplication_cooldown") instanceof Number cooldown ? cooldown.longValue() <= 0 : true;
    }

    default void setCanDuplicate(boolean duplicate) {
        setDuplicationCooldown(duplicate ? 0 : 6000);
    }

    default long getDuplicationCooldown() {
        return core().property("allay_duplication_cooldown") instanceof Number cooldown ? cooldown.longValue() : 0L;
    }

    default void setDuplicationCooldown(long cooldown) {
        core().set("allay_duplication_cooldown", cooldown);
    }

    default void resetDuplicationCooldown() {
        setDuplicationCooldown(6000L);
    }

    default boolean isDancing() {
        return core().flag("allay_dancing");
    }

    default void startDancing() {
        core().set("allay_dancing", true);
    }

    default void stopDancing() {
        core().set("allay_dancing", false);
    }

    default org.bukkit.Location getJukebox() {
        return MobPositions.parse(core(), core().text("allay_jukebox"));
    }

    default org.bukkit.entity.Allay duplicateAllay() {
        return core().call("allay_duplicate") instanceof Number id && id.intValue() >= 0
            && core().entity(id.intValue()) instanceof org.bukkit.entity.Allay copy ? copy : null;
    }

    default void startDancing(org.bukkit.Location location) {
        startDancing();
    }

    default org.bukkit.inventory.Inventory getInventory() {
        return new org.basaltmc.bukkit.inv.HeldInventory(core(), this, "allay", 1);
    }
}
