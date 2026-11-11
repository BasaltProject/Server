// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.storage.loot.predicates;
import net.minecraft.world.level.storage.loot.parameters.LootContextParams;
import com.mojang.serialization.MapCodec;
import java.util.Set;
import net.minecraft.util.context.ContextKey;
import net.minecraft.world.level.storage.loot.LootContext;
public class LootItemKilledByPlayerCondition implements LootItemCondition {
    public static final MapCodec<LootItemKilledByPlayerCondition> MAP_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.storage.loot.predicates.LootItemKilledByPlayerCondition.class, "MAP_CODEC");
    private LootItemKilledByPlayerCondition() {
        
    }
    public MapCodec<LootItemKilledByPlayerCondition> codec() {
        return (com.mojang.serialization.MapCodec) MAP_CODEC;
    }
    public Set<ContextKey<?>> getReferencedContextParams() {
        return Set.of(LootContextParams.LAST_DAMAGE_PLAYER);
    }
    public boolean test(LootContext context) {
        return context.hasParameter(LootContextParams.LAST_DAMAGE_PLAYER);
    }
    public static LootItemCondition.Builder killedByPlayer() {
        return () -> new LootItemKilledByPlayerCondition();
    }
}
