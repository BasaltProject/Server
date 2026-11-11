// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.material.condition;
import com.mojang.serialization.MapCodec;
import net.minecraft.world.level.levelgen.material.MaterialRuleContext;
public enum AbovePreliminarySurfaceCondition implements MaterialCondition {
    INSTANCE;
    public static final MapCodec<AbovePreliminarySurfaceCondition> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.levelgen.material.condition.AbovePreliminarySurfaceCondition.class, "CODEC");
    public MapCodec<AbovePreliminarySurfaceCondition> codec() {
        return CODEC;
    }
    public ConditionEvaluator compile(MaterialRuleContext context) {
        return () -> context.blockY() >= context.getMinSurfaceLevel();
    }
}
