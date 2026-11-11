// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.storage.loot.providers.number.floats;
import net.minecraft.world.level.storage.loot.parameters.LootContextParams;
import com.mojang.serialization.MapCodec;
import java.util.Set;
import net.minecraft.util.context.ContextKey;
import net.minecraft.world.item.enchantment.LevelBasedValue;
import net.minecraft.world.level.storage.loot.LootContext;
import net.minecraft.world.level.storage.loot.LootContextUser;
public record EnchantmentLevelProvider(LevelBasedValue amount) implements ContextFloatProvider, LootContextUser {
    public static final MapCodec<EnchantmentLevelProvider> MAP_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.storage.loot.providers.number.floats.EnchantmentLevelProvider.class, "MAP_CODEC");
    public MapCodec<EnchantmentLevelProvider> codec() {
        return (com.mojang.serialization.MapCodec) MAP_CODEC;
    }
    public Set<ContextKey<?>> getReferencedContextParams() {
        return Set.of(LootContextParams.ENCHANTMENT_LEVEL);
    }
    public float getFloatUnsafe(LootContext context) {
        Integer level = context.getOptional(net.minecraft.world.level.storage.loot.parameters.LootContextParams.ENCHANTMENT_LEVEL);
        return amount.calculate(level != null ? level : 0);

    }
}
