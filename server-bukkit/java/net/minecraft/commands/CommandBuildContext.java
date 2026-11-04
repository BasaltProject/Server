// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.commands;
import net.minecraft.core.HolderLookup;
import net.minecraft.world.flag.FeatureFlagSet;
public interface CommandBuildContext extends HolderLookup.Provider {
    static CommandBuildContext simple(HolderLookup.Provider access, FeatureFlagSet enabledFeatures) {
        return new CommandBuildContext() {
            @Override
            public FeatureFlagSet enabledFeatures() {
                return enabledFeatures;
            }

            @Override
            public <T> java.util.Optional<? extends HolderLookup.RegistryLookup<T>> lookup(net.minecraft.resources.ResourceKey<? extends net.minecraft.core.Registry<? extends T>> key) {
                return access.lookup(key);
            }

            @Override
            public java.util.stream.Stream<net.minecraft.resources.ResourceKey<? extends net.minecraft.core.Registry<?>>> listRegistryKeys() {
                return access.listRegistryKeys();
            }
        };
    }
    FeatureFlagSet enabledFeatures();
}
