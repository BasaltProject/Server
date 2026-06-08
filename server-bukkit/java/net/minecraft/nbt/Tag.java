package net.minecraft.nbt;

import org.basaltmc.bukkit.Unsupported;
public interface Tag {
    public default byte getId() {
        throw Unsupported.hit("net.minecraft.nbt.Tag.getId");
    }
}
