package net.minecraft.util;

import org.basaltmc.bukkit.Unsupported;
public interface StringRepresentable {
    public default String getSerializedName() {
        throw Unsupported.hit("net.minecraft.util.StringRepresentable.getSerializedName");
    }
}
