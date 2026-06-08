package net.minecraft.tags;

import net.minecraft.core.registries.Registries;
import net.minecraft.resources.Identifier;
import net.minecraft.world.level.material.Fluid;

public class FluidTags {
    public static final TagKey<Fluid> WATER = TagKey.create(Registries.FLUID, Identifier.withDefaultNamespace("water"));
    public static final TagKey<Fluid> LAVA = TagKey.create(Registries.FLUID, Identifier.withDefaultNamespace("lava"));

    private FluidTags() {}
}
