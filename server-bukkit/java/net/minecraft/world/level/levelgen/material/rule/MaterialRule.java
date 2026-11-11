// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.material.rule;
import com.mojang.serialization.Codec;
import com.mojang.serialization.MapCodec;
import net.minecraft.core.Holder;
import net.minecraft.world.level.levelgen.material.MaterialRuleContext;
public interface MaterialRule {
    Codec<MaterialRule> DIRECT_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.level.levelgen.material.rule.MaterialRule.class, "DIRECT_CODEC");
    Codec<Holder<MaterialRule>> HOLDER_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.level.levelgen.material.rule.MaterialRule.class, "HOLDER_CODEC");
    Codec<MaterialRule> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.level.levelgen.material.rule.MaterialRule.class, "CODEC");
    RuleEvaluator compile(MaterialRuleContext context);
    MapCodec<? extends MaterialRule> codec();
    public static record HolderHolder(Holder<MaterialRule> holder) implements MaterialRule {
        public RuleEvaluator compile(MaterialRuleContext context) {
        return holder.value().compile(context);
        }
        public MapCodec<HolderHolder> codec() {
        throw new UnsupportedOperationException("HolderHolder cannot be serialized");
        }
    }
}
