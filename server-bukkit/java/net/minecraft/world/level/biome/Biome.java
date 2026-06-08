package net.minecraft.world.level.biome;

import net.minecraft.resources.Identifier;

public class Biome {
    private final Identifier key;

    public Biome(Identifier key) {
        this.key = key;
    }

    public Identifier biomeKey() {
        return key;
    }

    @Override
    public String toString() {
        return key.toString();
    }
}
