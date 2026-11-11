// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.storage.loot.functions;
import com.mojang.serialization.MapCodec;
import java.util.Optional;
import net.minecraft.core.Holder;
import net.minecraft.core.HolderGetter;
import net.minecraft.core.HolderSet;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.item.enchantment.Enchantment;
import net.minecraft.world.level.storage.loot.LootContext;
import net.minecraft.world.level.storage.loot.ValidationContext;
import net.minecraft.world.level.storage.loot.predicates.LootItemCondition;
import net.minecraft.world.level.storage.loot.providers.number.ints.ContextIntProvider;
public class EnchantWithLevelsFunction extends LootItemConditionalFunction {
    public static final MapCodec<EnchantWithLevelsFunction> MAP_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.storage.loot.functions.EnchantWithLevelsFunction.class, "MAP_CODEC");
    private final Holder<ContextIntProvider> levels;
    private final Optional<HolderSet<Enchantment>> options;
    private final boolean includeAdditionalCostComponent;
    private EnchantWithLevelsFunction(Optional<Holder<LootItemCondition>> condition, Holder<ContextIntProvider> levels, Optional<HolderSet<Enchantment>> options, boolean includeAdditionalCostComponent) {
        super(condition);
        this.levels = levels;
        this.options = options;
        this.includeAdditionalCostComponent = includeAdditionalCostComponent;
    }
    protected void writeFunction(StringBuilder out) {
        out.append("\"type\":\"minecraft:enchant_with_levels\",\"levels\":");
        org.basaltmc.bukkit.loot.LootJson.value(out, levels);
        out.append(",\"include_additional_cost_component\":").append(includeAdditionalCostComponent);
        if (options.isPresent()) {
            out.append(",\"options\":");
            org.basaltmc.bukkit.loot.LootJson.holderSet(out, options.get());
        }
    }
    public MapCodec<EnchantWithLevelsFunction> codec() {
        return MAP_CODEC;
    }
    public void validate(ValidationContext context) {
        return;
    }
    public ItemStack run(ItemStack itemStack, LootContext context) {
        return org.basaltmc.bukkit.loot.LootFunctionBridge.apply(this, itemStack, context);
    }
    public static Builder enchantWithLevels(HolderGetter<Enchantment> enchantments, Holder<ContextIntProvider> levels) {
        return new Builder(levels).withOptions(enchantments.getOrThrow(net.minecraft.tags.EnchantmentTags.ON_RANDOM_LOOT));
    }
    public static class Builder extends LootItemConditionalFunction.Builder<Builder> {
        private final Holder<ContextIntProvider> levels;
        private Optional<HolderSet<Enchantment>> options = Optional.empty();
        private boolean includeAdditionalCostComponent;
        public Builder(Holder<ContextIntProvider> levels) {
            this.levels = levels;
        }
        protected Builder getThis() {
            return this;
        }
        public Builder withOptions(HolderSet<Enchantment> tag) {
            options = Optional.of(tag);
            return this;
        }
        public Builder withOptions(Optional<HolderSet<Enchantment>> options) {
            this.options = options;
            return this;
        }
        public Builder includeAdditionalCostComponent() {
            includeAdditionalCostComponent = true;
            return this;
        }
        public LootItemFunction build() {
            return new EnchantWithLevelsFunction(getCondition(), levels, options, includeAdditionalCostComponent);
        }
    }
}
