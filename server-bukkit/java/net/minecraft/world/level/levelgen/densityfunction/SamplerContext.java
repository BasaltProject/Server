// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.densityfunction;

import org.basaltmc.bukkit.Unsupported;
import net.minecraft.util.context.ContextKey;
import net.minecraft.util.context.ContextMap;
public class SamplerContext {
    private ContextMap userFields = ContextMap.EMPTY;
    private DensityBufferArena bufferArena = DensityBufferArena.GLOBAL;
    private CacheCell[] cacheCells;
    protected SamplerContext() {
    }
    public static SamplerContext EMPTY_UNCACHED = new SamplerContext();
    public static SamplerContext.Builder builder() {
        return new Builder();
    }

    public static class Builder {
        private ContextMap userFields = ContextMap.EMPTY;
        private DensityBufferArena bufferArena = DensityBufferArena.GLOBAL;
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
        this.cacheCells = !enableCaches ? null : new CacheCell[0];
    }
    public <T> T getField(ContextKey<T> key) {
        return userFields.get(key);
    }
    public <T> T getFieldOrDefault(ContextKey<? extends T> key, T defaultValue) {
        T found = userFields.get(key);
        return found != null ? found : defaultValue;
    }
    public ScopedDensityBuffer acquireBuffer(DensityVolume volume) {
        return bufferArena.acquire(volume.size());
    }
    private CacheCell cell(int cacheId) {
        if (cacheCells == null) {
            return null;
        }
        if (cacheId >= cacheCells.length) {
            int from = cacheCells.length;
            cacheCells = java.util.Arrays.copyOf(cacheCells, net.minecraft.util.Mth.roundToward(cacheId + 1, 16));
            for (int i = from; i < cacheCells.length; i++) {
                cacheCells[i] = new CacheCell();
            }
        }
        return cacheCells[cacheId];
    }
    void sampleVolumeCached(int cacheId, DensitySampler input, DensityBuffer outputBuffer, DensityVolume volume) {
        CacheCell cell = cell(cacheId);
        if (cell == null) {
            input.sampleVolume(this, outputBuffer, volume);
            return;
        }
        if (cell.buffer == null || !volume.equals(cell.volume)) {
            if (cell.buffer != null) {
                cell.buffer.close();
            }
            cell.volume = volume;
            cell.buffer = acquireBuffer(volume);
            input.sampleVolume(this, cell.buffer, volume);
        }
        outputBuffer.copyFrom(cell.buffer);
    }
    float sampleValueCached(int cacheId, DensitySampler input, int blockX, int blockY, int blockZ) {
        CacheCell cell = cell(cacheId);
        if (cell == null) {
            return input.sampleValue(this, blockX, blockY, blockZ);
        }
        long key = net.minecraft.core.BlockPos.asLong(blockX, blockY, blockZ);
        if (cell.valueKey == key && !Float.isNaN(cell.value)) {
            return cell.value;
        }
        if (cell.buffer != null && cell.volume != null) {
            int index = cell.volume.indexOfBlock(blockX, blockY, blockZ);
            if (index != -1) {
                return cell.buffer.get(index);
            }
        }
        float computed = input.sampleValue(this, blockX, blockY, blockZ);
        float value = computed;
        cell.valueKey = key;
        cell.value = value;
        return value;
    }
    static class CacheCell {
        float value = Float.NaN;
        long valueKey;
        ScopedDensityBuffer buffer;
        DensityVolume volume;
    }
}
