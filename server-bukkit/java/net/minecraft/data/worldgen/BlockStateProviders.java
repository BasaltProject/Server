// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.data.worldgen;
import net.minecraft.resources.ResourceKey;
import net.minecraft.world.level.levelgen.feature.stateproviders.BlockStateProvider;
public interface BlockStateProviders {
    ResourceKey<BlockStateProvider> CAVE_VINES_BODY = k("cave_vines_body");
    ResourceKey<BlockStateProvider> CAVE_VINES_HEAD = k("cave_vines_head");
    ResourceKey<BlockStateProvider> FLOWER_FLOWER_FOREST = k("flower_flower_forest");
    ResourceKey<BlockStateProvider> FLOWER_MEADOW = k("flower_meadow");
    ResourceKey<BlockStateProvider> FLOWER_PLAIN = k("flower_plain");
    ResourceKey<BlockStateProvider> MANGROVE_PROPAGULE = k("mangrove_propagule");
    ResourceKey<BlockStateProvider> PODZOL_BENEATH_TREE = k("podzol_beneath_tree");
    ResourceKey<BlockStateProvider> SOIL_BENEATH_TREE = k("soil_beneath_tree");
    static void bootstrap(BootstrapContext<BlockStateProvider> context) {
    }

    private static ResourceKey<BlockStateProvider> k(String n) {
        return net.minecraft.resources.ResourceKey.create(net.minecraft.core.registries.Registries.BLOCK_STATE_PROVIDER, net.minecraft.resources.Identifier.withDefaultNamespace(n));
    }
}
