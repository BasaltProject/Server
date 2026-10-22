package net.minecraft;

import org.basaltmc.bukkit.Unsupported;
public class SharedConstants {
    protected SharedConstants() {
    }
    public static WorldVersion getCurrentVersion() {
        throw Unsupported.hit("net.minecraft.SharedConstants.getCurrentVersion");
    }
}
