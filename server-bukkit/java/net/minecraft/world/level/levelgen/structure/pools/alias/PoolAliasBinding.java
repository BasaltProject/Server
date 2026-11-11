// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.structure.pools.alias;
import net.minecraft.resources.Identifier;
import net.minecraft.core.registries.Registries;
import com.mojang.serialization.Codec;
import com.mojang.serialization.MapCodec;
import java.util.List;
import java.util.function.BiConsumer;
import java.util.stream.Stream;
import net.minecraft.resources.ResourceKey;
import net.minecraft.util.RandomSource;
import net.minecraft.util.random.WeightedList;
import net.minecraft.world.level.levelgen.structure.pools.StructureTemplatePool;
public interface PoolAliasBinding {
    private static ResourceKey<StructureTemplatePool> poolKey(String name) {
        return ResourceKey.<StructureTemplatePool>create((ResourceKey<? extends net.minecraft.core.Registry<StructureTemplatePool>>) (ResourceKey<?>) Registries.TEMPLATE_POOL, Identifier.withDefaultNamespace(name));
    }
    Codec<PoolAliasBinding> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.level.levelgen.structure.pools.alias.PoolAliasBinding.class, "CODEC");
    void forEachResolved(RandomSource random, BiConsumer<ResourceKey<StructureTemplatePool>, ResourceKey<StructureTemplatePool>> aliasAndTargetConsumer);
    Stream<ResourceKey<StructureTemplatePool>> allTargets();
    static DirectPoolAlias direct(String id, String target) {
        return direct(poolKey(id), poolKey(target));
    }
    static DirectPoolAlias direct(ResourceKey<StructureTemplatePool> alias, ResourceKey<StructureTemplatePool> target) {
        return new DirectPoolAlias(alias, target);
    }
    static RandomPoolAlias random(String id, WeightedList<String> targets) {
        WeightedList.Builder<ResourceKey<StructureTemplatePool>> built = WeightedList.builder();
        targets.unwrap().forEach(entry -> built.add(poolKey(entry.value()), entry.weight()));
        return random(poolKey(id), built.build());
    }
    static RandomPoolAlias random(ResourceKey<StructureTemplatePool> id, WeightedList<ResourceKey<StructureTemplatePool>> targets) {
        return new RandomPoolAlias(id, targets);
    }
    static RandomGroupPoolAlias randomGroup(WeightedList<List<PoolAliasBinding>> combinations) {
        return new RandomGroupPoolAlias(combinations);
    }
    MapCodec<? extends PoolAliasBinding> codec();
}
