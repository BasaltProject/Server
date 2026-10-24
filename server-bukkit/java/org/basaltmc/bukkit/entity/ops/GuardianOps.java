package org.basaltmc.bukkit.entity.ops;

import org.basaltmc.bukkit.entity.Cored;
import org.bukkit.entity.Guardian;

public interface GuardianOps extends Guardian, Cored {
    default boolean isElder() {
        return getType() == org.bukkit.entity.EntityType.ELDER_GUARDIAN;
    }
}
