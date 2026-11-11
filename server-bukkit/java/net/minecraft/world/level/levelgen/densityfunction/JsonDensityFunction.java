// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.densityfunction;

import com.mojang.serialization.MapCodec;
import it.unimi.dsi.fastutil.floats.FloatList;
import java.util.List;
import net.minecraft.core.Holder;
import net.minecraft.util.Interval;
import net.minecraft.world.level.levelgen.synth.NormalNoise;
import org.basaltmc.bukkit.LeftoverNatives;

/** A density function tree held as game data JSON; the native density code evaluates it. */
public final class JsonDensityFunction implements DensityFunction {
    private final String json;
    private final String settings;
    private final long seed;

    private JsonDensityFunction(String json, String settings, long seed) {
        this.json = json;
        this.settings = settings;
        this.seed = seed;
    }

    public String json() {
        return json;
    }

    public JsonDensityFunction withSeed(long seed) {
        return new JsonDensityFunction(json, settings, seed);
    }

    public JsonDensityFunction withSettings(String settings) {
        return new JsonDensityFunction(json, settings, seed);
    }

    public double sample(int x, int y, int z) {
        return LeftoverNatives.terrainDensityTree(settings, seed, json, x, y, z);
    }

    public static JsonDensityFunction wrap(String json) {
        return new JsonDensityFunction(json, "minecraft:overworld", 0L);
    }

    static JsonDensityFunction constant(float value) {
        return raw("minecraft:constant", null, "\"value\":" + value);
    }

    static JsonDensityFunction node(String type, String field, DensityFunction child) {
        return raw(type, null, "\"" + field + "\":" + of(child));
    }

    static JsonDensityFunction binary(String type, DensityFunction left, DensityFunction right) {
        return raw(type, null, "\"left\":" + of(left) + ",\"right\":" + of(right));
    }

    static JsonDensityFunction raw(String type, DensityFunction input, String fields) {
        StringBuilder out = new StringBuilder("{\"type\":\"").append(type).append('"');
        if (input != null) {
            out.append(",\"input\":").append(of(input));
        }
        if (fields != null) {
            out.append(',').append(fields);
        }
        return new JsonDensityFunction(out.append('}').toString(), "minecraft:overworld", 0L);
    }

    static JsonDensityFunction intervalSelect(DensityFunction input, FloatList thresholds, List<DensityFunction> functions) {
        StringBuilder out = new StringBuilder("\"thresholds\":[");
        for (int i = 0; i < thresholds.size(); i++) {
            out.append(i == 0 ? "" : ",").append(thresholds.getFloat(i));
        }
        out.append("],\"functions\":[");
        for (int i = 0; i < functions.size(); i++) {
            out.append(i == 0 ? "" : ",").append(of(functions.get(i)));
        }
        return raw("minecraft:interval_select", input, out.append(']').toString());
    }

    public static String of(DensityFunction function) {
        if (function instanceof JsonDensityFunction tree) {
            return tree.json;
        }
        if (function instanceof TreeSource source) {
            return source.treeJson();
        }
        if (function instanceof DensityFunctions.HolderHolder holder) {
            return reference(holder.function());
        }
        throw new IllegalArgumentException("No game data form for " + function.getClass().getName());
    }

    public static String key(Holder<NormalNoise> noise) {
        return "\"" + noise.getRegisteredName() + "\"";
    }

    private static String reference(Holder<DensityFunction> holder) {
        return holder.unwrapKey().map(key -> "\"" + key.identifier() + "\"")
            .orElseGet(() -> of(holder.value()));
    }

    @Override
    public DensitySampler compileSampler(CompileContext context) {
        JsonDensityFunction self = this;
        return new DensitySampler() {
            public float sampleValue(SamplerContext sampler, int blockX, int blockY, int blockZ) {
                return (float) self.sample(blockX, blockY, blockZ);
            }

            public void sampleVolume(SamplerContext sampler, DensityBuffer outputBuffer, DensityVolume volume) {
                DensitySampler.sampleVolumeNaive(sampler, outputBuffer, volume, this);
            }
        };
    }

    @Override
    public DensityFunction rewriteChildren(DfRewriteRule rule) {
        return this;
    }

    @Override
    public Interval range() {
        return Interval.INFINITE;
    }

    @Override
    public int domainAxes() {
        return ALL_AXES;
    }

    @Override
    public MapCodec<? extends DensityFunction> codec() {
        return MapCodec.unit(this);
    }
}
