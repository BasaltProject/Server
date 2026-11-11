// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.gametest.framework;

import java.util.function.BiConsumer;
import java.util.function.Consumer;
import net.minecraft.core.Registry;
import net.minecraft.core.registries.Registries;
import net.minecraft.resources.Identifier;
import net.minecraft.resources.ResourceKey;

public class BuiltinTestFunctions implements TestFunctionLoader {
    public static final ResourceKey<Consumer<GameTestHelper>> ALWAYS_PASS = ResourceKey.create(Registries.TEST_FUNCTION, Identifier.withDefaultNamespace("always_pass"));
    public static final Consumer<GameTestHelper> ALWAYS_PASS_INSTANCE = GameTestHelper::succeed;

    public static Consumer<GameTestHelper> bootstrap(Registry<Consumer<GameTestHelper>> registry) {
        return Registry.register(registry, ALWAYS_PASS, ALWAYS_PASS_INSTANCE);
    }

    @Override
    public void load(BiConsumer<ResourceKey<Consumer<GameTestHelper>>, Consumer<GameTestHelper>> register) {
        register.accept(ALWAYS_PASS, ALWAYS_PASS_INSTANCE);
    }
}
