// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.densityfunction;

import org.basaltmc.bukkit.Unsupported;
import net.minecraft.util.context.ContextKey;
import net.minecraft.util.context.ContextMap;
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
            public Builder setUserFields(ContextMap userFields) {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.levelgen.densityfunction.SamplerContext.Builder#setUserFields");
        }
        public Builder useBufferArena(DensityBufferArena bufferArena) {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.levelgen.densityfunction.SamplerContext.Builder#useBufferArena");
        }
}
    private SamplerContext(ContextMap userFields, DensityBufferArena bufferArena, boolean enableCaches) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.levelgen.densityfunction.SamplerContext#<init>");
    }
    public <T> T getField(ContextKey<T> key) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.levelgen.densityfunction.SamplerContext#getField");
    }
    public <T> T getFieldOrDefault(ContextKey<? extends T> key, T defaultValue) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.levelgen.densityfunction.SamplerContext#getFieldOrDefault");
    }
    public ScopedDensityBuffer acquireBuffer(DensityVolume volume) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.levelgen.densityfunction.SamplerContext#acquireBuffer");
    }
    void sampleVolumeCached(int cacheId, DensitySampler input, DensityBuffer outputBuffer, DensityVolume volume) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.levelgen.densityfunction.SamplerContext#sampleVolumeCached");
    }
    float sampleValueCached(int cacheId, DensitySampler input, int blockX, int blockY, int blockZ) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.levelgen.densityfunction.SamplerContext#sampleValueCached");
    }
    static class CacheCell {
    }
}
