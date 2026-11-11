// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.item.component;
import net.minecraft.core.component.BlockTransformer;
import net.minecraft.core.registries.Registries;
import net.minecraft.data.worldgen.BootstrapContext;
import net.minecraft.resources.Identifier;
import net.minecraft.resources.ResourceKey;
public class BlockTransformers {
    public static final ResourceKey<BlockTransformer> SHOVEL = named("shovel");
    public static final ResourceKey<BlockTransformer> AXE = named("axe");
    public static final ResourceKey<BlockTransformer> HOE = named("hoe");
    private static ResourceKey<BlockTransformer> named(String name) {
        return ResourceKey.create(Registries.BLOCK_TRANSFORMER, Identifier.withDefaultNamespace(name));
    }
    public static void bootstrap(BootstrapContext<BlockTransformer> context) {
        // datapack generation only: the native tool-use service owns shovel, axe and hoe transforms
    }
}
