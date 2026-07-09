package org.basaltmc.bukkit.entity.ops;

import org.basaltmc.bukkit.entity.Cored;
import org.bukkit.entity.Evoker;

public interface EvokerOps extends Evoker, Cored {
    default Evoker.Spell getCurrentSpell() {
        return Evoker.Spell.values()[Math.max(0, Math.min(5, core().integer("spell")))];
    }

    default void setCurrentSpell(Evoker.Spell spell) {
        core().call("set_spell", (long) (spell == null ? 0 : spell.ordinal()));
    }

    default org.bukkit.entity.Sheep getWololoTarget() {
        return core().integer("evoker_wololo") >= 0
            && core().entity(core().integer("evoker_wololo")) instanceof org.bukkit.entity.Sheep sheep ? sheep : null;
    }

    default void setWololoTarget(org.bukkit.entity.Sheep sheep) {
        core().set("evoker_wololo", sheep == null ? -1L : (long) sheep.getEntityId());
    }
}
