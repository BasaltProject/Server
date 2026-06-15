package org.basaltmc.bukkit.entity.ops;

import io.papermc.paper.entity.Leashable;
import org.basaltmc.bukkit.entity.Cored;
import org.bukkit.entity.Entity;

public interface LeashableOps extends Leashable, Cored {
    default boolean isLeashed() {
        return core().integer("leash_holder") >= 0;
    }

    default Entity getLeashHolder() {
        Entity holder = core().entity(core().integer("leash_holder"));
        if (holder == null) {
            throw new IllegalStateException("Entity not leashed");
        }
        return holder;
    }

    default boolean setLeashHolder(Entity holder) {
        return core().set("leash_holder", holder == null ? -1 : holder.getEntityId());
    }
}
