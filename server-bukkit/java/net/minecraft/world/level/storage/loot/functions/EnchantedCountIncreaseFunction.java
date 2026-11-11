// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.storage.loot.functions;
import net.minecraft.world.level.storage.loot.parameters.LootContextParams;
import net.minecraft.world.entity.LivingEntity;
import net.minecraft.world.item.enchantment.EnchantmentHelper;
import net.minecraft.world.item.enchantment.Enchantments;
import com.mojang.serialization.MapCodec;
import java.util.Optional;
import java.util.Set;
import net.minecraft.core.Holder;
import net.minecraft.core.HolderGetter;
import net.minecraft.util.context.ContextKey;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.item.enchantment.Enchantment;
import net.minecraft.world.level.storage.loot.LootContext;
import net.minecraft.world.level.storage.loot.ValidationContext;
import net.minecraft.world.level.storage.loot.predicates.LootItemCondition;
import net.minecraft.world.level.storage.loot.providers.number.floats.ContextFloatProvider;
public class EnchantedCountIncreaseFunction extends LootItemConditionalFunction {
    private final Holder<Enchantment> enchantment;
    private final Holder<ContextFloatProvider> count;
    private final int limit;
    public static final int NO_LIMIT = 0;
    public static final MapCodec<EnchantedCountIncreaseFunction> MAP_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.storage.loot.functions.EnchantedCountIncreaseFunction.class, "MAP_CODEC");
    private EnchantedCountIncreaseFunction(Optional<Holder<LootItemCondition>> condition, Holder<Enchantment> enchantment, Holder<ContextFloatProvider> count, int limit) {
        super(condition);
        this.enchantment = enchantment;
        this.count = count;
        this.limit = limit;
    }
    public MapCodec<EnchantedCountIncreaseFunction> codec() {
        return (com.mojang.serialization.MapCodec) MAP_CODEC;
    }
    public Set<ContextKey<?>> getReferencedContextParams() {
        return Set.of(LootContextParams.ATTACKING_ENTITY);
    }
    public void validate(ValidationContext context) {
    }
    public ItemStack run(ItemStack itemStack, LootContext context) {
        if (context.getOptional(LootContextParams.ATTACKING_ENTITY) instanceof LivingEntity killer) {
            int level = EnchantmentHelper.getEnchantmentLevel(enchantment, killer);
            if (level != 0) {
                itemStack.grow(Math.round(level * count.value().getFloat(context)));
                if (limit > 0) {
                    itemStack.limitSize(limit);
                }
            }
        }
        return itemStack;
    }
    public static Builder lootingMultiplier(HolderGetter<Enchantment> enchantments, Holder<ContextFloatProvider> count) {
        return new Builder(enchantments.getOrThrow(Enchantments.LOOTING), count);
    }
    public static class Builder extends LootItemConditionalFunction.Builder<Builder> {
        private final Holder<Enchantment> enchantment;
        private final Holder<ContextFloatProvider> count;
        private int limit = 0;
        public Builder(Holder<Enchantment> enchantment, Holder<ContextFloatProvider> count) {
            this.enchantment = enchantment;
            this.count = count;
        }
        protected Builder getThis() {
            return this;
        }
        public Builder setLimit(int limit) {
            this.limit = limit;
            return this;
        }
        public LootItemFunction build() {
            return new EnchantedCountIncreaseFunction(getCondition(), enchantment, count, limit);
        }
    }
}
