// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.data.worldgen;
import net.minecraft.resources.ResourceKey;
import net.minecraft.world.level.levelgen.carver.WorldCarver;
public interface Carvers {
    ResourceKey<WorldCarver> CAVE = k("cave");
    ResourceKey<WorldCarver> CAVE_EXTRA_UNDERGROUND = k("cave_extra_underground");
    ResourceKey<WorldCarver> CANYON = k("canyon");
    ResourceKey<WorldCarver> NETHER_CAVE = k("nether_cave");
    static void bootstrap(BootstrapContext<WorldCarver> context) {
    }

    private static ResourceKey<WorldCarver> k(String n) {
        return net.minecraft.resources.ResourceKey.create(net.minecraft.core.registries.Registries.CARVER, net.minecraft.resources.Identifier.withDefaultNamespace(n));
    }
}
