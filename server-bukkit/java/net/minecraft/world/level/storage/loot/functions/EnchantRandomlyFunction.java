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
import net.minecraft.world.level.storage.loot.predicates.LootItemCondition;
public class EnchantRandomlyFunction extends LootItemConditionalFunction {
    public static final MapCodec<EnchantRandomlyFunction> MAP_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.storage.loot.functions.EnchantRandomlyFunction.class, "MAP_CODEC");
    private final Optional<HolderSet<Enchantment>> options;
    private final boolean onlyCompatible;
    private final boolean includeAdditionalCostComponent;
    private EnchantRandomlyFunction(Optional<Holder<LootItemCondition>> condition, Optional<HolderSet<Enchantment>> options, boolean onlyCompatible, boolean includeAdditionalCostComponent) {
        super(condition);
        this.options = options;
        this.onlyCompatible = onlyCompatible;
        this.includeAdditionalCostComponent = includeAdditionalCostComponent;
    }
    protected void writeFunction(StringBuilder out) {
        out.append("\"type\":\"minecraft:enchant_randomly\",\"only_compatible\":").append(onlyCompatible)
            .append(",\"include_additional_cost_component\":").append(includeAdditionalCostComponent);
        if (options.isPresent()) {
            out.append(",\"options\":");
            org.basaltmc.bukkit.loot.LootJson.holderSet(out, options.get());
        }
    }
    public MapCodec<EnchantRandomlyFunction> codec() {
        return MAP_CODEC;
    }
    public ItemStack run(ItemStack itemStack, LootContext context) {
        return org.basaltmc.bukkit.loot.LootFunctionBridge.apply(this, itemStack, context);
    }
    public static Builder randomEnchantment() {
        return new Builder();
    }
    public static Builder randomApplicableEnchantment(HolderGetter<Enchantment> enchantments) {
        return new Builder().withOneOf(enchantments.getOrThrow(net.minecraft.tags.EnchantmentTags.ON_RANDOM_LOOT));
    }
    public static class Builder extends LootItemConditionalFunction.Builder<Builder> {
        private Optional<HolderSet<Enchantment>> options = Optional.empty();
        private boolean onlyCompatible = true;
        private boolean includeAdditionalCostComponent;
        protected Builder getThis() {
            return this;
        }
        public Builder withEnchantment(Holder<Enchantment> enchantment) {
            options = Optional.of(HolderSet.direct(enchantment));
            return this;
        }
        public Builder withOneOf(HolderSet<Enchantment> enchantments) {
            options = Optional.of(enchantments);
            return this;
        }
        public Builder withOptions(HolderSet<Enchantment> enchantments) {
            return withOneOf(enchantments);
        }
        public Builder allowingIncompatibleEnchantments() {
            onlyCompatible = false;
            return this;
        }
        public Builder includeAdditionalCostComponent() {
            includeAdditionalCostComponent = true;
            return this;
        }
        public LootItemFunction build() {
            return new EnchantRandomlyFunction(getCondition(), options, onlyCompatible, includeAdditionalCostComponent);
        }
    }
}
