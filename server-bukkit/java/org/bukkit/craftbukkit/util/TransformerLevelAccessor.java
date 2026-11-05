package org.bukkit.craftbukkit.util;

import net.minecraft.world.level.WorldGenLevel;
import org.basaltmc.bukkit.Unsupported;
public abstract class TransformerLevelAccessor extends DelegatedLevelAccessor {
    public TransformerLevelAccessor() {
    }
    public void setDelegate(WorldGenLevel worldGenLevel) {
        throw Unsupported.hit("org.bukkit.craftbukkit.util.TransformerLevelAccessor.setDelegate");
    }
}
