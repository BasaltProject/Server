// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.data.worldgen;
import java.util.List;
import net.minecraft.util.random.WeightedList;
import net.minecraft.resources.ResourceKey;
import net.minecraft.world.level.levelgen.structure.pools.StructureTemplatePool;
import net.minecraft.world.level.levelgen.structure.pools.alias.PoolAliasBinding;
public class TrialChambersStructurePools {
    public static final ResourceKey<StructureTemplatePool> START = k("trial_chambers/chamber/end");
    public static final ResourceKey<StructureTemplatePool> HALLWAY_FALLBACK = k("trial_chambers/hallway/fallback");
    public static final List<PoolAliasBinding> ALIAS_BINDINGS = List.of(
        PoolAliasBinding.randomGroup(WeightedList.<List<PoolAliasBinding>>builder()
            .add(List.of(PoolAliasBinding.direct(spawner("contents/ranged"), spawner("ranged/skeleton")),
                PoolAliasBinding.direct(spawner("contents/slow_ranged"), spawner("slow_ranged/skeleton"))))
            .add(List.of(PoolAliasBinding.direct(spawner("contents/ranged"), spawner("ranged/stray")),
                PoolAliasBinding.direct(spawner("contents/slow_ranged"), spawner("slow_ranged/stray"))))
            .add(List.of(PoolAliasBinding.direct(spawner("contents/ranged"), spawner("ranged/poison_skeleton")),
                PoolAliasBinding.direct(spawner("contents/slow_ranged"), spawner("slow_ranged/poison_skeleton"))))
            .build()),
        PoolAliasBinding.random(spawner("contents/melee"), WeightedList.<String>builder()
            .add(spawner("melee/zombie")).add(spawner("melee/husk")).add(spawner("melee/spider")).build()),
        PoolAliasBinding.random(spawner("contents/small_melee"), WeightedList.<String>builder()
            .add(spawner("small_melee/slime")).add(spawner("small_melee/cave_spider"))
            .add(spawner("small_melee/silverfish")).add(spawner("small_melee/baby_zombie")).build()));
    public static String spawner(String alias) {
        return "trial_chambers/spawner/" + alias;
    }
    public static void bootstrap(BootstrapContext<StructureTemplatePool> context) {
    }

    private static ResourceKey<StructureTemplatePool> k(String n) {
        return Pools.createKey(n);
    }
}
