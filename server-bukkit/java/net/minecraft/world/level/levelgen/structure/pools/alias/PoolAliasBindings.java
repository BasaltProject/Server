// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.structure.pools.alias;
import net.minecraft.world.level.levelgen.structure.pools.StructurePoolElement;
import com.mojang.serialization.MapCodec;
import java.util.List;
import net.minecraft.core.Holder;
import net.minecraft.core.Registry;
import net.minecraft.data.worldgen.BootstrapContext;
import net.minecraft.world.level.levelgen.structure.pools.StructureTemplatePool;
public class PoolAliasBindings {
    public static MapCodec<? extends PoolAliasBinding> bootstrap(Registry<MapCodec<? extends PoolAliasBinding>> registry) {
        Registry.register(registry, "random", RandomPoolAlias.CODEC);
        Registry.register(registry, "random_group", RandomGroupPoolAlias.CODEC);
        return Registry.register(registry, "direct", DirectPoolAlias.CODEC);
    }
    public static void registerTargetsAsPools(BootstrapContext<StructureTemplatePool> context, Holder<StructureTemplatePool> emptyPool, List<PoolAliasBinding> aliasBindings) {
        aliasBindings.stream().flatMap(PoolAliasBinding::allTargets).map(key -> key.identifier().getPath()).forEach(path ->
            net.minecraft.data.worldgen.Pools.register(context, path, new StructureTemplatePool(emptyPool,
                List.of(com.mojang.datafixers.util.Pair.of(StructurePoolElement.single(path), 1)), StructureTemplatePool.Projection.RIGID)));


    }
}
