// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.gametest.framework;

import com.mojang.serialization.Codec;
import com.mojang.serialization.MapCodec;
import net.minecraft.core.Holder;
import net.minecraft.core.Registry;
import net.minecraft.core.registries.BuiltInRegistries;
import net.minecraft.network.chat.Component;
import net.minecraft.network.chat.MutableComponent;
import net.minecraft.resources.Identifier;
import net.minecraft.resources.ResourceKey;
import net.minecraft.world.level.Level;
import net.minecraft.world.level.block.Rotation;

public abstract class GameTestInstance {
    public static final Codec<GameTestInstance> DIRECT_CODEC = BuiltInRegistries.TEST_INSTANCE_TYPE.byNameCodec().dispatch(GameTestInstance::codec, codec -> codec);
    private final TestData<Holder<TestEnvironmentDefinition<?>>> info;

    public static MapCodec<? extends GameTestInstance> bootstrap(Registry<MapCodec<? extends GameTestInstance>> registry) {
        Registry.register(registry, Identifier.withDefaultNamespace("block_based"), BlockBasedTestInstance.CODEC);
        return Registry.register(registry, Identifier.withDefaultNamespace("function"), FunctionGameTestInstance.CODEC);
    }

    protected GameTestInstance(TestData<Holder<TestEnvironmentDefinition<?>>> info) {
        this.info = info;
    }

    public abstract void run(GameTestHelper helper);

    public abstract MapCodec<? extends GameTestInstance> codec();

    public Holder<TestEnvironmentDefinition<?>> batch() {
        return info.environment();
    }

    public ResourceKey<Level> dimension() {
        return info.dimension();
    }

    public Identifier structure() {
        return info.structure();
    }

    public int maxTicks() {
        return info.maxTicks();
    }

    public int setupTicks() {
        return info.setupTicks();
    }

    public boolean required() {
        return info.required();
    }

    public boolean manualOnly() {
        return info.manualOnly();
    }

    public int maxAttempts() {
        return info.maxAttempts();
    }

    public int requiredSuccesses() {
        return info.requiredSuccesses();
    }

    public boolean skyAccess() {
        return info.skyAccess();
    }

    public Rotation rotation() {
        return info.rotation();
    }

    public int padding() {
        return info.padding();
    }

    protected TestData<Holder<TestEnvironmentDefinition<?>>> info() {
        return info;
    }

    protected abstract MutableComponent typeDescription();

    public Component describe() {
        return describeType().append(describeInfo());
    }

    protected MutableComponent describeType() {
        return descriptionRow("test_instance.description.type", typeDescription());
    }

    protected Component describeInfo() {
        MutableComponent out = descriptionRow("test_instance.description.structure", structure().toString());
        out.append(descriptionRow("test_instance.description.batch", batch().getRegisteredName()));
        out.append(descriptionRow("test_instance.description.max_ticks", String.valueOf(maxTicks())));
        out.append(descriptionRow("test_instance.description.required", String.valueOf(required())));
        return out;
    }

    protected MutableComponent descriptionRow(String translationKey, String value) {
        return descriptionRow(translationKey, Component.literal(value));
    }

    protected MutableComponent descriptionRow(String translationKey, MutableComponent value) {
        return Component.translatable(translationKey, value).append("\n");
    }
}
