package net.minecraft;

import net.minecraft.world.level.storage.DataVersion;
import org.basaltmc.bukkit.Unsupported;
public interface WorldVersion {
    public default DataVersion dataVersion() {
        throw Unsupported.hit("net.minecraft.WorldVersion.dataVersion");
    }
}
