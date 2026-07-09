package org.basaltmc.bukkit.entity.ops;

import org.basaltmc.bukkit.entity.Cored;
import org.bukkit.entity.Ocelot;

public interface OcelotOps extends Ocelot, Cored {
    default boolean isTrusting() {
        return core().nbtFlag("Trusting");
    }

    default void setTrusting(boolean trust) {
        core().nbtSet("Trusting", "bool", trust);
    }

    default Ocelot.Type getCatType() {
        return Ocelot.Type.WILD_OCELOT;
    }

    default void setCatType(Ocelot.Type type) {
        if (type == Ocelot.Type.WILD_OCELOT) {
            return;
        }
        core().nbtSet("variant", "string", switch (type) {
            case BLACK_CAT -> "minecraft:black";
            case RED_CAT -> "minecraft:red";
            default -> "minecraft:siamese";
        });
        core().retype("cat");
    }
}
