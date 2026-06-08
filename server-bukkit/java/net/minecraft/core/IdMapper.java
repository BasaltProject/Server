package net.minecraft.core;

import org.basaltmc.bukkit.Unsupported;
public class IdMapper<T> implements IdMap {
    protected IdMapper() {
    }
    public int getId(Object thing) {
        throw Unsupported.hit("net.minecraft.core.IdMapper.getId");
    }
}
