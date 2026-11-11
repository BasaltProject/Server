// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.material.rule;
import com.mojang.serialization.MapCodec;
import net.minecraft.world.level.levelgen.material.MaterialRuleContext;
public enum BandlandsRule implements MaterialRule {
    INSTANCE;
    public static final MapCodec<BandlandsRule> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.levelgen.material.rule.BandlandsRule.class, "CODEC");
    public MapCodec<BandlandsRule> codec() {
        return CODEC;
    }
    public RuleEvaluator compile(MaterialRuleContext context) {
        return context::getBand;
    }
}
