package net.minecraft.world.level.chunk;

import net.minecraft.core.IdMap;
import org.basaltmc.bukkit.Unsupported;
public abstract class Strategy<T> {
    protected Strategy() {
    }
    public static Strategy createForBiomes(IdMap registry) {
        throw Unsupported.hit("net.minecraft.world.level.chunk.Strategy.createForBiomes");
    }
}
