// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.storage.loot.predicates;
import net.minecraft.world.level.storage.loot.parameters.LootContextParams;
import com.mojang.serialization.MapCodec;
import java.util.Set;
import net.minecraft.util.context.ContextKey;
import net.minecraft.world.level.storage.loot.LootContext;
public class ExplosionCondition implements LootItemCondition {
    public static final MapCodec<ExplosionCondition> MAP_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.storage.loot.predicates.ExplosionCondition.class, "MAP_CODEC");
    private ExplosionCondition() {
        
    }
    public MapCodec<ExplosionCondition> codec() {
        return (com.mojang.serialization.MapCodec) MAP_CODEC;
    }
    public Set<ContextKey<?>> getReferencedContextParams() {
        return Set.of(LootContextParams.EXPLOSION_RADIUS);
    }
    public boolean test(LootContext context) {
        Float radius = context.getOptional(LootContextParams.EXPLOSION_RADIUS);
        return radius == null || context.getRandom().nextFloat() <= 1.0F / radius;
    }
    public static LootItemCondition.Builder survivesExplosion() {
        return () -> new ExplosionCondition();
    }
}
