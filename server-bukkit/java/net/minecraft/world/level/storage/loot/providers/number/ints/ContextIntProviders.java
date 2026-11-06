// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.storage.loot.providers.number.ints;
import com.mojang.serialization.Codec;
import net.minecraft.core.Holder;
import net.minecraft.core.HolderSet;
import net.minecraft.data.worldgen.BootstrapContext;
import net.minecraft.resources.ResourceKey;
import net.minecraft.util.random.WeightedList;
import net.minecraft.world.level.storage.loot.LootContext;
import net.minecraft.world.level.storage.loot.providers.number.floats.ContextFloatProvider;
public class ContextIntProviders {
    private static ResourceKey<ContextIntProvider> keyOf(String location) {
        return ResourceKey.create(net.minecraft.core.registries.Registries.CONTEXT_INT_PROVIDER, net.minecraft.resources.Identifier.withDefaultNamespace(location));
    }
    public static final Codec<ContextIntProvider> DIRECT_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.level.storage.loot.providers.number.ints.ContextIntProviders.class, "DIRECT_CODEC");
    public static final Codec<Holder<ContextIntProvider>> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.level.storage.loot.providers.number.ints.ContextIntProviders.class, "CODEC");
    public static final Codec<HolderSet<ContextIntProvider>> LIST_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.level.storage.loot.providers.number.ints.ContextIntProviders.class, "LIST_CODEC");
    public static final ResourceKey<ContextIntProvider> COMPOSTABLE_LOW = keyOf("compostable/low");
    public static final ResourceKey<ContextIntProvider> COMPOSTABLE_LOW_MEDIUM = keyOf("compostable/low_medium");
    public static final ResourceKey<ContextIntProvider> COMPOSTABLE_MEDIUM = keyOf("compostable/medium");
    public static final ResourceKey<ContextIntProvider> COMPOSTABLE_MEDIUM_HIGH = keyOf("compostable/medium_high");
    public static final ResourceKey<ContextIntProvider> COMPOSTABLE_ALWAYS_ADD_ONE = keyOf("compostable/always_add_one");
    public static final ResourceKey<ContextIntProvider> COOKING_TIME_BAMBOO = keyOf("cooking/time_bamboo");
    public static final ResourceKey<ContextIntProvider> COOKING_TIME_WOOL_SLABS = keyOf("cooking/time_wool_slabs");
    public static final ResourceKey<ContextIntProvider> COOKING_TIME_WOOL_CARPETS = keyOf("cooking/time_wool_carpets");
    public static final ResourceKey<ContextIntProvider> COOKING_TIME_DRY_PLANTS = keyOf("cooking/time_dry_plants");
    public static final ResourceKey<ContextIntProvider> COOKING_TIME_WOOD_ITEMS_EXTRA_SMALL = keyOf("cooking/time_wood_items_extra_small");
    public static final ResourceKey<ContextIntProvider> COOKING_TIME_WOOL = keyOf("cooking/time_wool");
    public static final ResourceKey<ContextIntProvider> COOKING_TIME_WOOD_SLABS = keyOf("cooking/time_wood_slabs");
    public static final ResourceKey<ContextIntProvider> COOKING_TIME_WOOD_ITEMS_LARGE = keyOf("cooking/time_wood_items_large");
    public static final ResourceKey<ContextIntProvider> COOKING_TIME_WOOD_ITEMS_SMALL = keyOf("cooking/time_wood_items_small");
    public static final ResourceKey<ContextIntProvider> COOKING_TIME_ROOTS = keyOf("cooking/time_roots");
    public static final ResourceKey<ContextIntProvider> COOKING_TIME_WOOD_BLOCKS = keyOf("cooking/time_wood_blocks");
    public static final ResourceKey<ContextIntProvider> COOKING_TIME_HANGING_SIGNS = keyOf("cooking/time_hanging_signs");
    public static final ResourceKey<ContextIntProvider> COOKING_TIME_BOATS = keyOf("cooking/time_boats");
    public static final ResourceKey<ContextIntProvider> COOKING_TIME_COAL = keyOf("cooking/time_coal");
    public static final ResourceKey<ContextIntProvider> COOKING_TIME_BLAZE_ROD = keyOf("cooking/time_blaze_rod");
    public static final ResourceKey<ContextIntProvider> COOKING_TIME_DRIED_KELP_BLOCK = keyOf("cooking/time_dried_kelp_block");
    public static final ResourceKey<ContextIntProvider> COOKING_TIME_COAL_BLOCK = keyOf("cooking/time_coal_block");
    public static final ResourceKey<ContextIntProvider> COOKING_TIME_LAVA_BUCKET = keyOf("cooking/time_lava_bucket");
    public static final ResourceKey<ContextIntProvider> COOKING_NORMAL_BURN_TIME_REDUCTION_FACTOR = keyOf("cooking/normal_burn_time_reduction_factor");
    public static final ResourceKey<ContextIntProvider> COOKING_FAST_BURN_TIME_REDUCTION_FACTOR = keyOf("cooking/fast_burn_time_reduction_factor");
    public static final ResourceKey<ContextIntProvider> BREWING_DEFAULT_USES = keyOf("brewing/uses_default");
    public static void bootstrap(BootstrapContext<ContextIntProvider> context) {

    }
    public static Holder<ContextIntProvider> fromFloat(Holder<ContextFloatProvider> input) {
        return Holder.direct(new ComputedInt(c -> (int) input.value().getFloat(c)));
    }
    public static Holder<ContextIntProvider> binomial(int n, float p) {
        return Holder.direct(new ComputedInt(c -> {
            int hits = 0;
            for (int i = 0; i < n; i++) {
                hits += c.getRandom().nextFloat() < p ? 1 : 0;
            }
            return hits;
        }));
    }
    public static Holder<ContextIntProvider> exactly(int value) {
        return Holder.direct(new ConstantValue(value));
    }
    public static Holder<ContextIntProvider> fromScoreboard(LootContext.EntityTarget entityTarget, String score) {
        return Holder.direct(new ComputedInt(c -> {
            net.minecraft.world.entity.Entity entity = c.getOptional(entityTarget.contextParam());
            net.minecraft.world.scores.Scoreboard scoreboard = c.getLevel().getScoreboard();
            net.minecraft.world.scores.Objective objective = entity == null ? null : scoreboard.getObjective(score);
            net.minecraft.world.scores.ReadOnlyScoreInfo info = objective == null ? null : scoreboard.getPlayerScoreInfo(entity, objective);
            return info == null ? 0 : info.value();
        }));
    }
    public static Holder<ContextIntProvider> avg(Holder<ContextIntProvider>... inputs) {
        return Holder.direct(new ComputedInt(c -> (int) java.util.Arrays.stream(inputs).mapToInt(h -> h.value().getInt(c)).average().orElse(0.0)));
    }
    public static Holder<ContextIntProvider> sub(Holder<ContextIntProvider> left, Holder<ContextIntProvider> right) {
        return Holder.direct(new ComputedInt(c -> left.value().getInt(c) - right.value().getInt(c)));
    }
    public static Holder<ContextIntProvider> max(Holder<ContextIntProvider>... inputs) {
        return Holder.direct(new ComputedInt(c -> java.util.Arrays.stream(inputs).mapToInt(h -> h.value().getInt(c)).max().orElse(0)));
    }
    public static Holder<ContextIntProvider> min(Holder<ContextIntProvider>... inputs) {
        return Holder.direct(new ComputedInt(c -> java.util.Arrays.stream(inputs).mapToInt(h -> h.value().getInt(c)).min().orElse(0)));
    }
    public static Holder<ContextIntProvider> mod(Holder<ContextIntProvider> left, Holder<ContextIntProvider> right) {
        return Holder.direct(new ComputedInt(c -> left.value().getInt(c) % right.value().getInt(c)));
    }
    public static Holder<ContextIntProvider> floorMod(Holder<ContextIntProvider> left, Holder<ContextIntProvider> right) {
        return Holder.direct(new ComputedInt(c -> Math.floorMod(left.value().getInt(c), right.value().getInt(c))));
    }
    public static Holder<ContextIntProvider> negate(Holder<ContextIntProvider> input) {
        return Holder.direct(new ComputedInt(c -> -input.value().getInt(c)));
    }
    public static Holder<ContextIntProvider> mul(Holder<ContextIntProvider>... inputs) {
        return Holder.direct(new ComputedInt(c -> java.util.Arrays.stream(inputs).mapToInt(h -> h.value().getInt(c)).reduce(1, (a, b) -> a * b)));
    }
    public static Holder<ContextIntProvider> div(Holder<ContextIntProvider> left, Holder<ContextIntProvider> right) {
        return Holder.direct(new ComputedInt(c -> left.value().getInt(c) / right.value().getInt(c)));
    }
    public static Holder<ContextIntProvider> floorDiv(Holder<ContextIntProvider> left, Holder<ContextIntProvider> right) {
        return Holder.direct(new ComputedInt(c -> Math.floorDiv(left.value().getInt(c), right.value().getInt(c))));
    }
    public static Holder<ContextIntProvider> add(Holder<ContextIntProvider>... inputs) {
        return Holder.direct(new ComputedInt(c -> java.util.Arrays.stream(inputs).mapToInt(h -> h.value().getInt(c)).sum()));
    }
    public static Holder<ContextIntProvider> between(int min, int max) {
        return Holder.direct(new UniformGenerator(exactly(min), exactly(max)));
    }
    public static Holder<ContextIntProvider> weighted(WeightedList<Holder<ContextIntProvider>> cases) {
        return Holder.direct(new ComputedInt(c -> cases.getRandom(c.getRandom()).map(h -> h.value().getInt(c)).orElse(0)));
    }
}
