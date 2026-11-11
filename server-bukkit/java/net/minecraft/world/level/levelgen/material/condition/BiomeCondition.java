// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.material.condition;
import java.util.Set;
import net.minecraft.core.Holder;
import com.mojang.serialization.MapCodec;
import net.minecraft.core.HolderSet;
import net.minecraft.world.level.biome.Biome;
import net.minecraft.world.level.levelgen.material.MaterialRuleContext;
public record BiomeCondition(HolderSet<Biome> biomes) implements MaterialCondition {
    public static final MapCodec<BiomeCondition> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.levelgen.material.condition.BiomeCondition.class, "CODEC");
    public MapCodec<BiomeCondition> codec() {
        return CODEC;
    }
    public ConditionEvaluator compile(MaterialRuleContext ruleContext) {
        Set<Holder<Biome>> possible = ruleContext.possibleBiomes();
        if (possible != null) {
            boolean overlap = false;
            boolean allInside = true;
            for (Holder<Biome> biome : biomes) {
                overlap |= possible.contains(biome);
            }
            for (Holder<Biome> biome : possible) {
                allInside &= biomes.contains(biome);
            }
            if (!overlap) {
                return () -> false;
            }
            if (allInside) {
                return () -> true;
            }
        }
        return ruleContext.lazyY(() -> biomes.contains(ruleContext.getBiome()));
    }
}
