// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.material.condition;
import com.mojang.serialization.MapCodec;
import net.minecraft.world.level.levelgen.material.MaterialRuleContext;
public enum SteepCondition implements MaterialCondition {
    INSTANCE;
    public static final MapCodec<SteepCondition> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.levelgen.material.condition.SteepCondition.class, "CODEC");
    public MapCodec<SteepCondition> codec() {
        return CODEC;
    }
    public ConditionEvaluator compile(MaterialRuleContext context) {
        return context.lazyXz(() -> context.surfaceGradientX() <= -4 || context.surfaceGradientZ() >= 4);
    }
}
