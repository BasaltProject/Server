package net.minecraft.world.level.biome;
public class Climate {
    protected Climate() {
    }

    public record Sampler(Object temperature, Object humidity, Object continentalness, Object erosion, Object depth, Object weirdness) {
    }
}
