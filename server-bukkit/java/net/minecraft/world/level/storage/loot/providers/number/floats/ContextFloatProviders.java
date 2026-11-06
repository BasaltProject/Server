// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.storage.loot.providers.number.floats;
import com.mojang.serialization.Codec;
import net.minecraft.core.Holder;
import net.minecraft.core.HolderSet;
import net.minecraft.data.worldgen.BootstrapContext;
import net.minecraft.resources.ResourceKey;
import net.minecraft.world.attribute.EnvironmentAttribute;
import net.minecraft.world.level.storage.loot.LootContext;
import net.minecraft.world.item.enchantment.LevelBasedValue;
public class ContextFloatProviders {
    private static ResourceKey<ContextFloatProvider> keyOf(String location) {
        return ResourceKey.create(net.minecraft.core.registries.Registries.CONTEXT_FLOAT_PROVIDER, net.minecraft.resources.Identifier.withDefaultNamespace(location));
    }
    public static final Codec<ContextFloatProvider> DIRECT_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.level.storage.loot.providers.number.floats.ContextFloatProviders.class, "DIRECT_CODEC");
    public static final Codec<Holder<ContextFloatProvider>> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.level.storage.loot.providers.number.floats.ContextFloatProviders.class, "CODEC");
    public static final Codec<HolderSet<ContextFloatProvider>> LIST_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.level.storage.loot.providers.number.floats.ContextFloatProviders.class, "LIST_CODEC");
    public static final ResourceKey<ContextFloatProvider> COOKING_DEFAULT_SPEED_MULTIPLIER = keyOf("cooking/speed_default");
    public static final ResourceKey<ContextFloatProvider> COOKING_NORMAL_SPEED_MULTIPLIER = keyOf("cooking/normal_speed_multiplier");
    public static final ResourceKey<ContextFloatProvider> COOKING_FAST_SPEED_MULTIPLIER = keyOf("cooking/fast_speed_multiplier");
    public static final ResourceKey<ContextFloatProvider> BREWING_DEFAULT_SPEED_MULTIPLIER = keyOf("brewing/speed_default");
    public static void bootstrap(BootstrapContext<ContextFloatProvider> context) {

    }
    public static Holder<ContextFloatProvider> abs(Holder<ContextFloatProvider> input) {
        return Holder.direct(new ComputedFloat(c -> Math.abs(input.value().getFloat(c))));
    }
    public static Holder<ContextFloatProvider> avg(Holder<ContextFloatProvider>... inputs) {
        return Holder.direct(new ComputedFloat(c -> java.util.Arrays.stream(inputs).mapToDouble(h -> h.value().getFloat(c)).average().orElse(0.0)));
    }
    public static Holder<ContextFloatProvider> ceiling(Holder<ContextFloatProvider> input) {
        return Holder.direct(new ComputedFloat(c -> Math.ceil(input.value().getFloat(c))));
    }
    public static Holder<ContextFloatProvider> exactly(float value) {
        return Holder.direct(new ConstantValue(value));
    }
    public static Holder<ContextFloatProvider> cos(Holder<ContextFloatProvider> input) {
        return Holder.direct(new ComputedFloat(c -> Math.cos(input.value().getFloat(c))));
    }
    public static Holder<ContextFloatProvider> sub(Holder<ContextFloatProvider> left, Holder<ContextFloatProvider> right) {
        return Holder.direct(new ComputedFloat(c -> left.value().getFloat(c) - right.value().getFloat(c)));
    }
    public static Holder<ContextFloatProvider> forEnchantmentLevel(LevelBasedValue amount) {
        return Holder.direct(new ComputedFloat(c -> {
            Integer level = c.getOptional(net.minecraft.world.level.storage.loot.parameters.LootContextParams.ENCHANTMENT_LEVEL);
            return level == null ? 0.0F : amount.calculate(level);
        }));
    }
    public static Holder<ContextFloatProvider> forEnvironmentAttribute(EnvironmentAttribute<?> attribute) {
        return Holder.direct(new ComputedFloat(c -> floatOf(c, attribute)));
    }
    private static <Value> float floatOf(LootContext context, EnvironmentAttribute<Value> attribute) {
        return attribute.type().toFloat().applyAsFloat(context.getLevel().environmentAttributes().getValue(context, attribute));
    }
    public static Holder<ContextFloatProvider> length(Holder<ContextFloatProvider>... inputs) {
        return Holder.direct(new ComputedFloat(c -> Math.sqrt(java.util.Arrays.stream(inputs).mapToDouble(h -> h.value().getFloat(c)).map(v -> v * v).sum())));
    }
    public static Holder<ContextFloatProvider> max(Holder<ContextFloatProvider>... inputs) {
        return Holder.direct(new ComputedFloat(c -> java.util.Arrays.stream(inputs).mapToDouble(h -> h.value().getFloat(c)).max().orElse(0.0)));
    }
    public static Holder<ContextFloatProvider> min(Holder<ContextFloatProvider>... inputs) {
        return Holder.direct(new ComputedFloat(c -> java.util.Arrays.stream(inputs).mapToDouble(h -> h.value().getFloat(c)).min().orElse(0.0)));
    }
    public static Holder<ContextFloatProvider> mod(Holder<ContextFloatProvider> left, Holder<ContextFloatProvider> right) {
        return Holder.direct(new ComputedFloat(c -> left.value().getFloat(c) % right.value().getFloat(c)));
    }
    public static Holder<ContextFloatProvider> negate(Holder<ContextFloatProvider> input) {
        return Holder.direct(new ComputedFloat(c -> -input.value().getFloat(c)));
    }
    public static Holder<ContextFloatProvider> pow(Holder<ContextFloatProvider> base, Holder<ContextFloatProvider> exponent) {
        return Holder.direct(new ComputedFloat(c -> Math.pow(base.value().getFloat(c), exponent.value().getFloat(c))));
    }
    public static Holder<ContextFloatProvider> mul(Holder<ContextFloatProvider>... inputs) {
        return Holder.direct(new ComputedFloat(c -> java.util.Arrays.stream(inputs).mapToDouble(h -> h.value().getFloat(c)).reduce(1.0, (a, b) -> a * b)));
    }
    public static Holder<ContextFloatProvider> div(Holder<ContextFloatProvider> left, Holder<ContextFloatProvider> right) {
        return Holder.direct(new ComputedFloat(c -> left.value().getFloat(c) / right.value().getFloat(c)));
    }
    public static Holder<ContextFloatProvider> round(Holder<ContextFloatProvider> input) {
        return Holder.direct(new ComputedFloat(c -> Math.round(input.value().getFloat(c))));
    }
    public static Holder<ContextFloatProvider> sin(Holder<ContextFloatProvider> input) {
        return Holder.direct(new ComputedFloat(c -> Math.sin(input.value().getFloat(c))));
    }
    public static Holder<ContextFloatProvider> sqrt(Holder<ContextFloatProvider> input) {
        return Holder.direct(new ComputedFloat(c -> Math.sqrt(input.value().getFloat(c))));
    }
    public static Holder<ContextFloatProvider> add(Holder<ContextFloatProvider>... inputs) {
        return Holder.direct(new ComputedFloat(c -> java.util.Arrays.stream(inputs).mapToDouble(h -> h.value().getFloat(c)).sum()));
    }
    public static Holder<ContextFloatProvider> trunc(Holder<ContextFloatProvider> input) {
        return Holder.direct(new ComputedFloat(c -> (long) input.value().getFloat(c)));
    }
    public static Holder<ContextFloatProvider> between(float min, float max) {
        return Holder.direct(new UniformGenerator(exactly(min), exactly(max)));
    }
}
