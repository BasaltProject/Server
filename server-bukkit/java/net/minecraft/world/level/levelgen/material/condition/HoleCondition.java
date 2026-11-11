// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.material.condition;
import com.mojang.serialization.MapCodec;
import net.minecraft.world.level.levelgen.material.MaterialRuleContext;
public enum HoleCondition implements MaterialCondition {
    INSTANCE;
    public static final MapCodec<HoleCondition> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.levelgen.material.condition.HoleCondition.class, "CODEC");
    public MapCodec<HoleCondition> codec() {
        return CODEC;
    }
    public ConditionEvaluator compile(MaterialRuleContext context) {
        return context.lazyXz(() -> context.surfaceDepth() <= 0);
    }
}
