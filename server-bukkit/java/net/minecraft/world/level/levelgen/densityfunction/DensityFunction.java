// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.densityfunction;
import com.mojang.serialization.Codec;
import com.mojang.serialization.MapCodec;
import net.minecraft.core.Direction;
import net.minecraft.core.Holder;
import net.minecraft.resources.Identifier;
import net.minecraft.util.Interval;
import net.minecraft.util.RandomSource;
import net.minecraft.world.level.levelgen.synth.Noise;
import net.minecraft.world.level.levelgen.synth.NormalNoise;
public interface DensityFunction {
    Codec<Holder<DensityFunction>> REFERENCE_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.level.levelgen.densityfunction.DensityFunction.class, "REFERENCE_CODEC");
    Codec<DensityFunction> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.level.levelgen.densityfunction.DensityFunction.class, "CODEC");
    int AXIS_X = 1;
    int AXIS_Y = 2;
    int AXIS_Z = 4;
    int NO_AXES = 0;
    int ALL_AXES = 7;
    static int axesFrom(Direction.Axis axis) {
        return switch (axis) {
            case X -> AXIS_X;
            case Y -> AXIS_Y;
            case Z -> AXIS_Z;
        };
    }
    DensitySampler compileSampler(CompileContext context);
    DensityFunction rewriteChildren(DfRewriteRule rule);
    Interval range();
    int domainAxes();
    MapCodec<? extends DensityFunction> codec();
    default DensityFunction clamp(float min, float max) {
        return JsonDensityFunction.raw("minecraft:clamp", this, "\"min\":" + min + ",\"max\":" + max);
    }
    default DensityFunction abs() {
        return JsonDensityFunction.node("minecraft:abs", "input", this);
    }
    default DensityFunction square() {
        return JsonDensityFunction.node("minecraft:square", "input", this);
    }
    default DensityFunction cube() {
        return JsonDensityFunction.node("minecraft:cube", "input", this);
    }
    default DensityFunction sqrt() {
        return JsonDensityFunction.node("minecraft:sqrt", "input", this);
    }
    default DensityFunction halfNegative() {
        return JsonDensityFunction.node("minecraft:half_negative", "input", this);
    }
    default DensityFunction quarterNegative() {
        return JsonDensityFunction.node("minecraft:quarter_negative", "input", this);
    }
    default DensityFunction reciprocal() {
        return JsonDensityFunction.node("minecraft:reciprocal", "input", this);
    }
    default DensityFunction negate() {
        return JsonDensityFunction.node("minecraft:negate", "input", this);
    }
    default DensityFunction squeeze() {
        return JsonDensityFunction.node("minecraft:squeeze", "input", this);
    }
    default DensityFunction log() {
        return JsonDensityFunction.node("minecraft:log", "input", this);
    }
    default DensityFunction sign() {
        return JsonDensityFunction.node("minecraft:sign", "input", this);
    }
    default DensityFunction add(DensityFunction right) {
        return JsonDensityFunction.binary("minecraft:add", this, right);
    }
    default DensityFunction add(float right) {
        return JsonDensityFunction.binary("minecraft:add", this, JsonDensityFunction.constant(right));
    }
    default DensityFunction sub(DensityFunction right) {
        return JsonDensityFunction.binary("minecraft:sub", this, right);
    }
    default DensityFunction sub(float right) {
        return JsonDensityFunction.binary("minecraft:sub", this, JsonDensityFunction.constant(right));
    }
    default DensityFunction mul(DensityFunction right) {
        return JsonDensityFunction.binary("minecraft:mul", this, right);
    }
    default DensityFunction mul(float right) {
        return JsonDensityFunction.binary("minecraft:mul", this, JsonDensityFunction.constant(right));
    }
    default DensityFunction div(DensityFunction right) {
        return JsonDensityFunction.binary("minecraft:div", this, right);
    }
    default DensityFunction div(float right) {
        return JsonDensityFunction.binary("minecraft:div", this, JsonDensityFunction.constant(right));
    }
    default DensityFunction pow(DensityFunction exponent) {
        return JsonDensityFunction.binary("minecraft:pow", this, exponent);
    }
    default DensityFunction pow(float exponent) {
        return JsonDensityFunction.binary("minecraft:pow", this, JsonDensityFunction.constant(exponent));
    }
    public @interface Axes {
    }
    public interface CompileContext {
        Noise createNoiseSampler(Holder<NormalNoise> parameters);
        RandomSource createRandom(Identifier seed);
        RandomSource createEndIslandRandom();
    }
}
