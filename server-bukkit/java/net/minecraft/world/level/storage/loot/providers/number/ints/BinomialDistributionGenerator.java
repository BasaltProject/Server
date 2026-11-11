// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.storage.loot.providers.number.ints;
import net.minecraft.world.level.storage.loot.Validatable;
import com.mojang.serialization.MapCodec;
import net.minecraft.core.Holder;
import net.minecraft.world.level.storage.loot.LootContext;
import net.minecraft.world.level.storage.loot.ValidationContext;
import net.minecraft.world.level.storage.loot.providers.number.floats.ContextFloatProvider;
public record BinomialDistributionGenerator(Holder<ContextIntProvider> n, Holder<ContextFloatProvider> p) implements ContextIntProvider {
    public static final MapCodec<BinomialDistributionGenerator> MAP_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.storage.loot.providers.number.ints.BinomialDistributionGenerator.class, "MAP_CODEC");
    public MapCodec<BinomialDistributionGenerator> codec() {
        return (com.mojang.serialization.MapCodec) MAP_CODEC;
    }
    public int getIntUnsafe(LootContext context) {
        int trials = n().value().getIntUnsafe(context);
        float chance = p().value().getFloatOrThrow(context);
        int hits = 0;
        for (int i = 0; i < trials; i++) {
            if (context.getRandom().nextFloat() < chance) {
                hits++;
            }
        }
        return hits;


    }
    public void validate(ValidationContext context) {
        Validatable.validateHolder(context, "n", n);
        Validatable.validateHolder(context, "p", p);
    }
}
