// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.storage.loot.predicates;
import net.minecraft.world.level.storage.loot.parameters.LootContextParams;
import net.minecraft.world.item.enchantment.EnchantmentHelper;
import com.mojang.serialization.MapCodec;
import java.util.List;
import java.util.Set;
import net.minecraft.core.Holder;
import net.minecraft.util.context.ContextKey;
import net.minecraft.world.item.enchantment.Enchantment;
import net.minecraft.world.level.storage.loot.LootContext;
public record BonusLevelTableCondition(Holder<Enchantment> enchantment, List<Float> values) implements LootItemCondition {
    public static final MapCodec<BonusLevelTableCondition> MAP_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.storage.loot.predicates.BonusLevelTableCondition.class, "MAP_CODEC");
    public MapCodec<BonusLevelTableCondition> codec() {
        return (com.mojang.serialization.MapCodec) MAP_CODEC;
    }
    public Set<ContextKey<?>> getReferencedContextParams() {
        return Set.of(LootContextParams.TOOL);
    }
    public boolean test(LootContext context) {
        var tool = context.getOptional(LootContextParams.TOOL);
        int level = tool != null ? EnchantmentHelper.getItemEnchantmentLevel(enchantment, tool) : 0;
        return context.getRandom().nextFloat() < values.get(Math.min(level, values.size() - 1));
    }
    public static LootItemCondition.Builder bonusLevelFlatChance(Holder<Enchantment> enchantment, float... chances) {
        List<Float> table = new java.util.ArrayList<>(chances.length);
        for (float each : chances) {
            table.add(each);
        }
        return () -> new BonusLevelTableCondition(enchantment, table);
    }
}
