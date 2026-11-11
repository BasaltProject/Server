// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.structure.pools.alias;
import com.mojang.serialization.MapCodec;
import java.util.List;
import java.util.function.BiConsumer;
import java.util.stream.Stream;
import net.minecraft.resources.ResourceKey;
import net.minecraft.util.RandomSource;
import net.minecraft.util.random.WeightedList;
import net.minecraft.world.level.levelgen.structure.pools.StructureTemplatePool;
public record RandomGroupPoolAlias(WeightedList<List<PoolAliasBinding>> groups) implements PoolAliasBinding {
    public static final MapCodec<RandomGroupPoolAlias> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.levelgen.structure.pools.alias.RandomGroupPoolAlias.class, "CODEC");
    public void forEachResolved(RandomSource random, BiConsumer<ResourceKey<StructureTemplatePool>, ResourceKey<StructureTemplatePool>> aliasAndTargetConsumer) {
        groups.getRandomOrThrow(random).forEach(binding -> binding.forEachResolved(random, aliasAndTargetConsumer));
    }
    public Stream<ResourceKey<StructureTemplatePool>> allTargets() {
        return groups.unwrap().stream().flatMap(entry -> entry.value().stream()).flatMap(PoolAliasBinding::allTargets);
    }
    public MapCodec<RandomGroupPoolAlias> codec() {
        return CODEC;
    }
}
