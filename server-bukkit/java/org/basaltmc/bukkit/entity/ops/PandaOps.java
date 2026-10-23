package org.basaltmc.bukkit.entity.ops;

import java.util.Locale;
import org.basaltmc.bukkit.entity.Cored;
import org.bukkit.entity.Panda;

public interface PandaOps extends Panda, Cored {
    private Panda.Gene gene(String key) {
        String name = core().nbtText(key);
        try {
            return name == null ? Panda.Gene.NORMAL : Panda.Gene.valueOf(name.toUpperCase(Locale.ROOT));
        } catch (IllegalArgumentException unknown) {
            return Panda.Gene.NORMAL;
        }
    }

    default Panda.Gene getMainGene() {
        return gene("MainGene");
    }

    default void setMainGene(Panda.Gene gene) {
        core().nbtSet("MainGene", "string", gene.name().toLowerCase(Locale.ROOT));
    }

    default Panda.Gene getHiddenGene() {
        return gene("HiddenGene");
    }

    default void setHiddenGene(Panda.Gene gene) {
        core().nbtSet("HiddenGene", "string", gene.name().toLowerCase(Locale.ROOT));
    }
}
