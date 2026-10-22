package net.minecraft.world.level.block;

import org.basaltmc.bukkit.Registries;

public class Blocks {
    public static final Block AIR = named("minecraft:air");
    public static final Block STRUCTURE_BLOCK = named("minecraft:structure_block");

    private Blocks() {}

    public static Block named(String id) {
        return Block.byIndex(Registries.blockIndex(id));
    }
}
