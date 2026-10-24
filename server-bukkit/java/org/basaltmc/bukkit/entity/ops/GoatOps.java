package org.basaltmc.bukkit.entity.ops;

import org.basaltmc.bukkit.entity.Cored;
import org.bukkit.entity.Goat;

public interface GoatOps extends Goat, Cored {
    default boolean hasLeftHorn() {
        return core().nbtValue("HasLeftHorn") == null || core().nbtFlag("HasLeftHorn");
    }

    default void setLeftHorn(boolean horn) {
        core().nbtSet("HasLeftHorn", "bool", horn);
    }

    default boolean hasRightHorn() {
        return core().nbtValue("HasRightHorn") == null || core().nbtFlag("HasRightHorn");
    }

    default void setRightHorn(boolean horn) {
        core().nbtSet("HasRightHorn", "bool", horn);
    }

    default boolean isScreaming() {
        return core().nbtFlag("IsScreamingGoat");
    }

    default void setScreaming(boolean screaming) {
        core().nbtSet("IsScreamingGoat", "bool", screaming);
    }

    default void ram(org.bukkit.entity.LivingEntity target) {
        core().call("goat_ram", (long) target.getEntityId());
    }
}
