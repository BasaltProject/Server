package net.minecraft.world.level.levelgen.densityfunction;

import org.basaltmc.bukkit.Unsupported;
public class SamplerContext {
    protected SamplerContext() {
    }
    public static SamplerContext EMPTY_UNCACHED;
    public static SamplerContext.Builder builder() {
        throw Unsupported.hit("net.minecraft.world.level.levelgen.densityfunction.SamplerContext.builder");
    }

    public static class Builder {
        protected Builder() {
        }
        public SamplerContext build() {
            throw Unsupported.hit("net.minecraft.world.level.levelgen.densityfunction.SamplerContext.Builder.build");
        }
        public SamplerContext.Builder enableCaches() {
            throw Unsupported.hit("net.minecraft.world.level.levelgen.densityfunction.SamplerContext.Builder.enableCaches");
        }
    }
}
