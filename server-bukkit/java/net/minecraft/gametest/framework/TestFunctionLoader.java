// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.gametest.framework;

import java.util.ArrayList;
import java.util.List;
import java.util.function.BiConsumer;
import java.util.function.Consumer;
import net.minecraft.core.Registry;
import net.minecraft.resources.ResourceKey;

public interface TestFunctionLoader {
    List<TestFunctionLoader> ALL_LOADERS = new ArrayList<>();

    static void registerLoader(TestFunctionLoader loader) {
        ALL_LOADERS.add(loader);
    }

    static void runLoaders(Registry<Consumer<GameTestHelper>> registry) {
        for (TestFunctionLoader loader : ALL_LOADERS) {
            loader.load((key, function) -> Registry.register(registry, key, function));
        }
    }

    void load(BiConsumer<ResourceKey<Consumer<GameTestHelper>>, Consumer<GameTestHelper>> register);
}
