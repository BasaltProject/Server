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

    default Panda.Gene getCombinedGene() {
        Panda.Gene main = getMainGene();
        Panda.Gene hidden = getHiddenGene();
        if (!main.isRecessive()) {
            return main;
        }
        return main == hidden ? main : Panda.Gene.NORMAL;
    }

    private boolean flag(int bit) {
        return (core().integer("panda_flags") & bit) != 0;
    }

    private void flag(int bit, boolean on) {
        int flags = core().integer("panda_flags");
        core().set("panda_flags", on ? flags | bit : flags & ~bit);
    }

    default boolean isRolling() {
        return flag(4);
    }

    default void setRolling(boolean rolling) {
        flag(4, rolling);
    }

    default boolean isSneezing() {
        return flag(2);
    }

    default void setSneezing(boolean sneezing) {
        flag(2, sneezing);
    }

    default boolean isOnBack() {
        return flag(16);
    }

    default void setOnBack(boolean onBack) {
        flag(16, onBack);
    }

    default boolean isEating() {
        return core().integer("panda_eat") > 0;
    }

    default void setEating(boolean eating) {
        core().set("panda_eat", eating ? 1 : 0);
    }

    default boolean isScared() {
        return getMainGene() == Panda.Gene.WORRIED && getWorld().isThundering();
    }

    default int getUnhappyTicks() {
        return core().integer("panda_unhappy");
    }

    default void setUnhappyTicks(int ticks) {
        core().set("panda_unhappy", ticks);
    }

    default int getSneezeTicks() {
        return core().integer("panda_sneeze");
    }

    default void setSneezeTicks(int ticks) {
        core().set("panda_sneeze", ticks);
    }

    default int getEatingTicks() {
        return core().integer("panda_eat");
    }

    default void setEatingTicks(int ticks) {
        core().set("panda_eat", ticks);
    }
}
