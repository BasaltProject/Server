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

    default boolean inCaravan() {
        return core().integer("llama_caravan_head") >= 0;
    }

    default void joinCaravan(Llama llama) {
        if (!core().test("llama_join_caravan", (long) llama.getEntityId())) {
            throw new IllegalArgumentException("A llama can only follow another llama in its world");
        }
    }

    default void leaveCaravan() {
        core().call("llama_leave_caravan");
    }

    default Llama getCaravanHead() {
        return core().entity(core().integer("llama_caravan_head")) instanceof Llama head ? head : null;
    }

    default boolean hasCaravanTail() {
        return core().integer("llama_caravan_tail") >= 0;
    }

    default Llama getCaravanTail() {
        return core().entity(core().integer("llama_caravan_tail")) instanceof Llama tail ? tail : null;
    }
}
