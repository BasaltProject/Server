package org.basaltmc.bukkit.entity.ops;

import org.basaltmc.bukkit.entity.Cored;
import org.bukkit.entity.Spellcaster;

public interface SpellcasterOps extends Spellcaster, Cored {
    default Spellcaster.Spell getSpell() {
        return Spellcaster.Spell.values()[Math.max(0, Math.min(5, core().integer("spell")))];
    }

    default void setSpell(Spellcaster.Spell spell) {
        if (!core().test("set_spell", (long) spell.ordinal())) {
            throw org.basaltmc.bukkit.Unsupported.hit("Spellcaster#setSpell");
        }
    }
}
