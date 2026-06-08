package net.minecraft.world.level.levelgen;

import net.minecraft.world.level.biome.Climate;
import net.minecraft.world.level.levelgen.densityfunction.SamplerContext;
import org.basaltmc.bukkit.Unsupported;
public class RandomState {
    protected RandomState() {
    }
    public Climate.Sampler createClimateSampler(SamplerContext context) {
        throw Unsupported.hit("net.minecraft.world.level.levelgen.RandomState.createClimateSampler");
    }
}
