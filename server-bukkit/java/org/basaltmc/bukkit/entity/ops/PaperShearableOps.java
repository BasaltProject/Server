package org.basaltmc.bukkit.entity.ops;

import org.basaltmc.bukkit.entity.Cored;

public interface PaperShearableOps extends io.papermc.paper.entity.Shearable, Cored {
    default boolean readyToBeSheared() {
        return core().test("shear_ready");
    }

    default void shear(net.kyori.adventure.sound.Sound.Source source) {
        core().call("shear", (long) source.ordinal());
    }
}
