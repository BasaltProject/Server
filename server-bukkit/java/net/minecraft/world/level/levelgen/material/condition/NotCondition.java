// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.material.condition;
import com.mojang.serialization.MapCodec;
import net.minecraft.world.level.levelgen.material.MaterialRuleContext;
public record NotCondition(MaterialCondition target) implements MaterialCondition {
    public static final MapCodec<NotCondition> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.levelgen.material.condition.NotCondition.class, "CODEC");
    public MapCodec<NotCondition> codec() {
        return CODEC;
    }
    public ConditionEvaluator compile(MaterialRuleContext context) {
        ConditionEvaluator inner = target.compile(context);
        return () -> !inner.test();
    }
}
