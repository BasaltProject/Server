package org.basaltmc.bukkit.entity.ops;

import org.basaltmc.bukkit.entity.Cored;
import org.bukkit.entity.Llama;

public interface LlamaOps extends Llama, Cored {
    default Llama.Color getColor() {
        return Llama.Color.values()[Math.floorMod(core().nbtInt("Variant"), Llama.Color.values().length)];
    }

    default void setColor(Llama.Color color) {
        core().nbtSet("Variant", "int", color.ordinal());
    }

    default int getStrength() {
        return Math.max(1, core().nbtInt("Strength", 1));
    }

    default void setStrength(int strength) {
        if (strength < 1 || strength > 5) {
            throw new IllegalArgumentException("Strength must be between 1 and 5");
        }
        core().nbtSet("Strength", "int", strength);
    }
}
