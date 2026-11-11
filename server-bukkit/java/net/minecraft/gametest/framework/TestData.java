// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.gametest.framework;

import com.mojang.serialization.Codec;
import com.mojang.serialization.MapCodec;
import com.mojang.serialization.codecs.RecordCodecBuilder;
import java.util.function.Function;
import net.minecraft.core.Holder;
import net.minecraft.resources.Identifier;
import net.minecraft.resources.ResourceKey;
import net.minecraft.world.level.Level;
import net.minecraft.world.level.block.Rotation;

public record TestData<EnvironmentType>(EnvironmentType environment, ResourceKey<Level> dimension, Identifier structure, int maxTicks, int setupTicks, boolean required, Rotation rotation, boolean manualOnly, int maxAttempts, int requiredSuccesses, boolean skyAccess, int padding) {
    public static final MapCodec<TestData<Holder<TestEnvironmentDefinition<?>>>> CODEC = RecordCodecBuilder.mapCodec(i -> i.group(
        TestEnvironmentDefinition.CODEC.fieldOf("environment").forGetter(TestData::environment),
        Level.RESOURCE_KEY_CODEC.optionalFieldOf("dimension", Level.OVERWORLD).forGetter(TestData::dimension),
        Identifier.CODEC.fieldOf("structure").forGetter(TestData::structure),
        Codec.INT.fieldOf("max_ticks").forGetter(TestData::maxTicks),
        Codec.INT.optionalFieldOf("setup_ticks", 0).forGetter(TestData::setupTicks),
        Codec.BOOL.optionalFieldOf("required", true).forGetter(TestData::required),
        Rotation.CODEC.optionalFieldOf("rotation", Rotation.NONE).forGetter(TestData::rotation),
        Codec.BOOL.optionalFieldOf("manual_only", false).forGetter(TestData::manualOnly),
        Codec.INT.optionalFieldOf("max_attempts", 1).forGetter(TestData::maxAttempts),
        Codec.INT.optionalFieldOf("required_successes", 1).forGetter(TestData::requiredSuccesses),
        Codec.BOOL.optionalFieldOf("sky_access", false).forGetter(TestData::skyAccess),
        Codec.INT.optionalFieldOf("padding", 0).forGetter(TestData::padding)
    ).apply(i, TestData::new));

    public TestData(EnvironmentType environment, Identifier structure, int maxTicks, int setupTicks, boolean required, Rotation rotation) {
        this(environment, Level.OVERWORLD, structure, maxTicks, setupTicks, required, rotation, false, 1, 1, false, 0);
    }

    public TestData(EnvironmentType environment, Identifier structure, int maxTicks, int setupTicks, boolean required) {
        this(environment, structure, maxTicks, setupTicks, required, Rotation.NONE);
    }

    public <T> TestData<T> map(Function<EnvironmentType, T> mapper) {
        return new TestData<>(mapper.apply(environment), dimension, structure, maxTicks, setupTicks, required, rotation, manualOnly, maxAttempts, requiredSuccesses, skyAccess, padding);
    }
}
