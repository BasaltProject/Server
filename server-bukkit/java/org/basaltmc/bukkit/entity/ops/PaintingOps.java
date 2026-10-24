package org.basaltmc.bukkit.entity.ops;

import org.basaltmc.bukkit.entity.Cored;
import org.bukkit.entity.Painting;

public interface PaintingOps extends Painting, Cored {
    default org.bukkit.Art getArt() {
        return EntityNbt.variant(core(), "variant", io.papermc.paper.registry.RegistryKey.PAINTING_VARIANT, "kebab");
    }

    default boolean setArt(org.bukkit.Art art) {
        return setArt(art, false);
    }

    default boolean setArt(org.bukkit.Art art, boolean force) {
        EntityNbt.variant(core(), "variant", art);
        return true;
    }
}
