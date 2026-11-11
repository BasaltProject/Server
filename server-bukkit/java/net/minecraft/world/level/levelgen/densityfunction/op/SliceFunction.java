// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.densityfunction.op;
import net.minecraft.world.level.levelgen.densityfunction.TreeSource;
import net.minecraft.world.level.levelgen.densityfunction.JsonDensityFunction;
import net.minecraft.world.level.levelgen.densityfunction.ScopedDensityBuffer;
import com.mojang.serialization.MapCodec;
import net.minecraft.core.Direction;
import net.minecraft.util.Interval;
import net.minecraft.world.level.levelgen.densityfunction.DensityBuffer;
import net.minecraft.world.level.levelgen.densityfunction.DensityFunction;
import net.minecraft.world.level.levelgen.densityfunction.DensitySampler;
import net.minecraft.world.level.levelgen.densityfunction.DensityVolume;
import net.minecraft.world.level.levelgen.densityfunction.DfRewriteRule;
import net.minecraft.world.level.levelgen.densityfunction.SamplerContext;
public record SliceFunction(Direction.Axis axis, int coordinate, DensityFunction input) implements DensityFunction, TreeSource {
    public String treeJson() {
        return "{\"type\":\"minecraft:slice\",\"axis\":\"" + axis.getSerializedName() + "\",\"coordinate\":" + coordinate + ",\"input\":" + JsonDensityFunction.of(input) + "}";
    }
    public static final MapCodec<SliceFunction> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.levelgen.densityfunction.op.SliceFunction.class, "CODEC");
    public DensitySampler compileSampler(DensityFunction.CompileContext context) {
        if (input instanceof SliceFunction inner && (axis == Direction.Axis.X && inner.axis == Direction.Axis.Z || axis == Direction.Axis.Z && inner.axis == Direction.Axis.X)) {
            int fixedX = axis == Direction.Axis.X ? coordinate : inner.coordinate;
            int fixedZ = axis == Direction.Axis.X ? inner.coordinate : coordinate;
            return new XzSampler(inner.input.compileSampler(context), fixedX, fixedZ);
        }
        DensitySampler in = input.compileSampler(context);
        return switch (axis) {
            case X -> new XSampler(in, coordinate);
            case Y -> new YSampler(in, coordinate);
            case Z -> new ZSampler(in, coordinate);
        };
    }
    public DensityFunction rewriteChildren(DfRewriteRule rule) {
        DensityFunction rewritten = rule.rewrite(input);
        return rewritten == input ? this : new SliceFunction(axis, coordinate, rewritten);
    }
    public Interval range() {
        return input.range();
    }
    public int domainAxes() {
        return input.domainAxes() & ~DensityFunction.axesFrom(axis);
    }
    public MapCodec<SliceFunction> codec() {
        return CODEC;
    }
    public static record XSampler(DensitySampler input, int x) implements DensitySampler {
        public void sampleVolume(SamplerContext context, DensityBuffer outputBuffer, DensityVolume volume) {
        if (volume.sizeX() == 1 && volume.minBlockX() == x) {
            input.sampleVolume(context, outputBuffer, volume);
            return;
        }
        DensityVolume flat = new DensityVolume(1, volume.sizeY(), volume.sizeZ(), x, volume.minBlockY(), volume.minBlockZ(), volume.stepBlockX(), volume.stepBlockY(), volume.stepBlockZ());
        try (ScopedDensityBuffer slab = context.acquireBuffer(flat)) {
            input.sampleVolume(context, slab, flat);
            int index = 0;
            for (int z = 0; z < volume.sizeZ(); z++) {
                for (int column = 0; column < volume.sizeX(); column++) {
                    for (int y = 0; y < volume.sizeY(); y++) {
                        outputBuffer.set(index++, slab.get(flat.indexUnchecked(0, y, z)));
                    }
                }
            }
        }
        }
        public float sampleValue(SamplerContext context, int blockX, int blockY, int blockZ) {
        return input.sampleValue(context, x, blockY, blockZ);
        }
    }
    public static record YSampler(DensitySampler input, int y) implements DensitySampler {
        public void sampleVolume(SamplerContext context, DensityBuffer outputBuffer, DensityVolume volume) {
        if (volume.sizeY() == 1 && volume.minBlockY() == y) {
            input.sampleVolume(context, outputBuffer, volume);
            return;
        }
        DensityVolume flat = new DensityVolume(volume.sizeX(), 1, volume.sizeZ(), volume.minBlockX(), y, volume.minBlockZ(), volume.stepBlockX(), volume.stepBlockY(), volume.stepBlockZ());
        try (ScopedDensityBuffer slab = context.acquireBuffer(flat)) {
            input.sampleVolume(context, slab, flat);
            for (int z = 0; z < volume.sizeZ(); z++) {
                for (int x = 0; x < volume.sizeX(); x++) {
                    outputBuffer.setRange(volume.indexUnchecked(x, 0, z), volume.sizeY(), slab.get(flat.indexUnchecked(x, 0, z)));
                }
            }
        }
        }
        public float sampleValue(SamplerContext context, int blockX, int blockY, int blockZ) {
        return input.sampleValue(context, blockX, y, blockZ);
        }
    }
    public static record ZSampler(DensitySampler input, int z) implements DensitySampler {
        public void sampleVolume(SamplerContext context, DensityBuffer outputBuffer, DensityVolume volume) {
        if (volume.sizeZ() == 1 && volume.minBlockZ() == z) {
            input.sampleVolume(context, outputBuffer, volume);
            return;
        }
        DensityVolume flat = new DensityVolume(volume.sizeX(), volume.sizeY(), 1, volume.minBlockX(), volume.minBlockY(), z, volume.stepBlockX(), volume.stepBlockY(), volume.stepBlockZ());
        try (ScopedDensityBuffer slab = context.acquireBuffer(flat)) {
            input.sampleVolume(context, slab, flat);
            int index = 0;
            for (int row = 0; row < volume.sizeZ(); row++) {
                for (int x = 0; x < volume.sizeX(); x++) {
                    for (int y = 0; y < volume.sizeY(); y++) {
                        outputBuffer.set(index++, slab.get(flat.indexUnchecked(x, y, 0)));
                    }
                }
            }
        }
        }
        public float sampleValue(SamplerContext context, int blockX, int blockY, int blockZ) {
        return input.sampleValue(context, blockX, blockY, z);
        }
    }
    public static record XzSampler(DensitySampler input, int x, int z) implements DensitySampler {
        public void sampleVolume(SamplerContext context, DensityBuffer outputBuffer, DensityVolume volume) {
        if (volume.sizeX() == 1 && volume.sizeZ() == 1 && volume.minBlockX() == x && volume.minBlockZ() == z) {
            input.sampleVolume(context, outputBuffer, volume);
            return;
        }
        DensityVolume flat = new DensityVolume(1, volume.sizeY(), 1, x, volume.minBlockY(), z, volume.stepBlockX(), volume.stepBlockY(), volume.stepBlockZ());
        try (ScopedDensityBuffer slab = context.acquireBuffer(flat)) {
            input.sampleVolume(context, slab, flat);
            for (int y = 0; y < volume.sizeY(); y++) {
                float value = slab.get(flat.indexUnchecked(0, y, 0));
                for (int column = 0; column < volume.sizeX() * volume.sizeZ(); column++) {
                    outputBuffer.set(y + column * volume.sizeY(), value);
                }
            }
        }
        }
        public float sampleValue(SamplerContext context, int blockX, int blockY, int blockZ) {
        return input.sampleValue(context, x, blockY, z);
        }
    }
}
