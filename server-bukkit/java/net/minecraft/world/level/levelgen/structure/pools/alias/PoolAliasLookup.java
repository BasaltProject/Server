// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.structure.pools.alias;
import net.minecraft.util.RandomSource;
import java.util.List;
import net.minecraft.core.BlockPos;
import net.minecraft.resources.ResourceKey;
import net.minecraft.world.level.levelgen.structure.pools.StructureTemplatePool;
public interface PoolAliasLookup {
    PoolAliasLookup EMPTY = key -> key;
    ResourceKey<StructureTemplatePool> lookup(ResourceKey<StructureTemplatePool> alias);
    static PoolAliasLookup create(List<PoolAliasBinding> poolAliasBindings, BlockPos pos, long seed) {
        if (poolAliasBindings.isEmpty()) {
            return EMPTY;
        }
        RandomSource random = RandomSource.create(seed).forkPositional().at(pos);
        java.util.Map<ResourceKey<StructureTemplatePool>, ResourceKey<StructureTemplatePool>> mapped = new java.util.LinkedHashMap<>();
        for (PoolAliasBinding binding : poolAliasBindings) {
            binding.forEachResolved(random, (from, to) -> {
                if (mapped.putIfAbsent(from, to) != null) {
                    throw new IllegalArgumentException("Multiple entries with same key: " + from);
                }
            });
        }
        return key -> java.util.Objects.requireNonNull(mapped.getOrDefault(key, key), () -> "alias " + key.identifier() + " was mapped to null value");
    }
}
