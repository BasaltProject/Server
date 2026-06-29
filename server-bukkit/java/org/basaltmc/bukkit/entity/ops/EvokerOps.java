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
}
