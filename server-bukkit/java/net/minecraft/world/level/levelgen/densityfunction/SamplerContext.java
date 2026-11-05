// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.densityfunction;

import org.basaltmc.bukkit.Unsupported;
import net.minecraft.util.context.ContextKey;
import net.minecraft.util.context.ContextMap;
public class SamplerContext {
    private ContextMap userFields = ContextMap.EMPTY;
    private DensityBufferArena bufferArena;
    private boolean enableCaches;
    protected SamplerContext() {
    }
    public static SamplerContext EMPTY_UNCACHED = new SamplerContext();
    public static SamplerContext.Builder builder() {
        return new Builder();
    }

    public static class Builder {
        private ContextMap userFields = ContextMap.EMPTY;
        private DensityBufferArena bufferArena;
        private boolean enableCaches;
        protected Builder() {
        }
        public SamplerContext build() {
            return new SamplerContext(userFields, bufferArena, enableCaches);
        }
        public SamplerContext.Builder enableCaches() {
            this.enableCaches = true;
            return this;
        }
            public Builder setUserFields(ContextMap userFields) {
            this.userFields = userFields;
            return this;
        }
        public Builder useBufferArena(DensityBufferArena bufferArena) {
            this.bufferArena = bufferArena;
            return this;
        }
}
    private SamplerContext(ContextMap userFields, DensityBufferArena bufferArena, boolean enableCaches) {
        this.userFields = userFields;
        this.bufferArena = bufferArena;
        this.enableCaches = enableCaches;
    }
    public <T> T getField(ContextKey<T> key) {
        return userFields.getOrThrow(key);
    }
    public <T> T getFieldOrDefault(ContextKey<? extends T> key, T defaultValue) {
        T found = userFields.get(key);
        return found != null ? found : defaultValue;
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
