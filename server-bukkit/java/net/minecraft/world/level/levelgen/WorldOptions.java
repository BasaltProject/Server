package net.minecraft.world.level.levelgen;

import java.util.OptionalLong;
import org.basaltmc.bukkit.Unsupported;
public class WorldOptions {
    protected WorldOptions() {
    }
    public WorldOptions withSeed(OptionalLong seed) {
        throw Unsupported.hit("net.minecraft.world.level.levelgen.WorldOptions.withSeed");
    }
}
