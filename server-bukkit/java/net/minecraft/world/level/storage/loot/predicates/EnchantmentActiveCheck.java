// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.storage.loot.predicates;
import net.minecraft.world.level.storage.loot.parameters.LootContextParams;
import com.mojang.serialization.MapCodec;
import java.util.Set;
import net.minecraft.util.context.ContextKey;
import net.minecraft.world.level.storage.loot.LootContext;
public record EnchantmentActiveCheck(boolean active) implements LootItemCondition {
    public static final MapCodec<EnchantmentActiveCheck> MAP_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.storage.loot.predicates.EnchantmentActiveCheck.class, "MAP_CODEC");
    public boolean test(LootContext lootContext) {
        Boolean state = lootContext.getOptional(LootContextParams.ENCHANTMENT_ACTIVE);
        return state != null && state == active;
    }
    public MapCodec<EnchantmentActiveCheck> codec() {
        return (com.mojang.serialization.MapCodec) MAP_CODEC;
    }
    public Set<ContextKey<?>> getReferencedContextParams() {
        return Set.of(LootContextParams.ENCHANTMENT_ACTIVE);
    }
    public static LootItemCondition.Builder enchantmentActiveCheck() {
        return () -> new EnchantmentActiveCheck(true);
    }
    public static LootItemCondition.Builder enchantmentInactiveCheck() {
        return () -> new EnchantmentActiveCheck(false);
    }
}
