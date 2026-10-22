package net.minecraft.nbt;

import org.basaltmc.bukkit.Unsupported;
public class ByteArrayTag implements CollectionTag {
    public ByteArrayTag(byte[] values) {
    }
    public byte[] getAsByteArray() {
        throw Unsupported.hit("net.minecraft.nbt.ByteArrayTag.getAsByteArray");
    }
}
