// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.material.condition;
import com.mojang.serialization.Codec;
import com.mojang.serialization.MapCodec;
import net.minecraft.core.Holder;
import net.minecraft.world.level.levelgen.material.MaterialRuleContext;
public interface MaterialCondition {
    Codec<MaterialCondition> DIRECT_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.level.levelgen.material.condition.MaterialCondition.class, "DIRECT_CODEC");
    Codec<MaterialCondition> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.level.levelgen.material.condition.MaterialCondition.class, "CODEC");
    ConditionEvaluator compile(MaterialRuleContext context);
    MapCodec<? extends MaterialCondition> codec();
    public static record HolderHolder(Holder<MaterialCondition> holder) implements MaterialCondition {
        public ConditionEvaluator compile(MaterialRuleContext context) {
        return holder.value().compile(context);
        }
        public MapCodec<HolderHolder> codec() {
        throw new UnsupportedOperationException("HolderHolder cannot be serialized");
        }
    }
}
