// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.densityfunction;

public interface DensitySampler {
    void sampleVolume(SamplerContext context, DensityBuffer outputBuffer, DensityVolume volume);

    static void sampleVolumeNaive(SamplerContext context, DensityBuffer outputBuffer, DensityVolume volume, DensitySampler sampler) {
        int index = 0;
        for (int z = 0; z < volume.sizeZ(); z++) {
            for (int x = 0; x < volume.sizeX(); x++) {
                for (int y = 0; y < volume.sizeY(); y++) {
                    outputBuffer.set(index++, sampler.sampleValue(context, volume.blockX(x), volume.blockY(y), volume.blockZ(z)));
                }
            }
        }
    }

    float sampleValue(SamplerContext context, int blockX, int blockY, int blockZ);

    default Bound bind(SamplerContext context) {
        return new Bound(this, context);
    }

    public static record Bound(DensitySampler sampler, SamplerContext context) {
        public float sampleValue(int blockX, int blockY, int blockZ) {
            return sampler.sampleValue(context, blockX, blockY, blockZ);
        }

        public void sampleVolume(DensityBuffer outputBuffer, DensityVolume volume) {
            sampler.sampleVolume(context, outputBuffer, volume);
        }

        public ScopedDensityBuffer sampleVolume(DensityVolume volume) {
            ScopedDensityBuffer buffer = context.acquireBuffer(volume);
            sampleVolume(buffer, volume);
            return buffer;
        }
    }
}
