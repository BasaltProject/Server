// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.structure.pools.alias;
import com.mojang.serialization.MapCodec;
import java.util.function.BiConsumer;
import java.util.stream.Stream;
import net.minecraft.resources.ResourceKey;
import net.minecraft.util.RandomSource;
import net.minecraft.util.random.WeightedList;
import net.minecraft.world.level.levelgen.structure.pools.StructureTemplatePool;
public record RandomPoolAlias(ResourceKey<StructureTemplatePool> alias, WeightedList<ResourceKey<StructureTemplatePool>> targets) implements PoolAliasBinding {
    public static final MapCodec<RandomPoolAlias> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.levelgen.structure.pools.alias.RandomPoolAlias.class, "CODEC");
    public void forEachResolved(RandomSource random, BiConsumer<ResourceKey<StructureTemplatePool>, ResourceKey<StructureTemplatePool>> aliasAndTargetConsumer) {
        aliasAndTargetConsumer.accept(alias, targets.getRandomOrThrow(random));
    }
    public Stream<ResourceKey<StructureTemplatePool>> allTargets() {
        return targets.unwrap().stream().map(net.minecraft.util.random.Weighted::value);
    }
    public MapCodec<RandomPoolAlias> codec() {
        return CODEC;
    }
}
