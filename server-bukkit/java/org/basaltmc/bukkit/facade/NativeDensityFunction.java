package org.basaltmc.bukkit.facade;

import com.mojang.serialization.MapCodec;
import net.minecraft.util.Interval;
import net.minecraft.world.level.levelgen.densityfunction.DensityFunction;
import net.minecraft.world.level.levelgen.densityfunction.DensitySampler;
import net.minecraft.world.level.levelgen.densityfunction.DfRewriteRule;

/** A router function of a noise settings and seed; the native density code evaluates it. */
public final class NativeDensityFunction implements DensityFunction {
    private final String settings;
    private final long seed;
    private final String name;

    public NativeDensityFunction(String settings, long seed, String name) {
        this.settings = settings;
        this.seed = seed;
        this.name = name;
    }

    public String settings() {
        return settings;
    }

    public long seed() {
        return seed;
    }

    public String name() {
        return name;
    }

    public NativeDensityFunction withSeed(long seed) {
        return new NativeDensityFunction(settings, seed, name);
    }

    public DensitySampler compileSampler(CompileContext context) {
        return new NativeSampler(this);
    }

    public DensityFunction rewriteChildren(DfRewriteRule rule) {
        return this;
    }

    public Interval range() {
        return Interval.INFINITE;
    }

    public int domainAxes() {
        return ALL_AXES;
    }

    public MapCodec<? extends DensityFunction> codec() {
        return MapCodec.unit(this);
    }
}
