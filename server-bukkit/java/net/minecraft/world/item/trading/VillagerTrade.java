// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.item.trading;
import java.util.List;
import net.minecraft.core.component.DataComponents;
import net.minecraft.world.level.storage.loot.providers.number.ints.ContextIntProviders;
import net.minecraft.world.level.storage.loot.providers.number.floats.ContextFloatProviders;
import net.minecraft.world.level.storage.loot.functions.FunctionUserBuilder;
import net.minecraft.world.item.ItemStack;
import com.mojang.serialization.Codec;
import java.util.Collection;
import java.util.Optional;
import net.minecraft.core.Holder;
import net.minecraft.core.HolderSet;
import net.minecraft.world.item.ItemStackTemplate;
import net.minecraft.world.item.enchantment.Enchantment;
import net.minecraft.world.level.storage.loot.LootContext;
import net.minecraft.world.level.storage.loot.Validatable;
import net.minecraft.world.level.storage.loot.ValidationContext;
import net.minecraft.world.level.storage.loot.functions.LootItemFunction;
import net.minecraft.world.level.storage.loot.predicates.LootItemCondition;
import net.minecraft.world.level.storage.loot.providers.number.floats.ContextFloatProvider;
import net.minecraft.world.level.storage.loot.providers.number.ints.ContextIntProvider;
public class VillagerTrade implements Validatable {
    private final TradeCost wants;
    private final Optional<TradeCost> additionalWants;
    private final ItemStackTemplate gives;
    private final Holder<ContextIntProvider> maxUses;
    private final Holder<ContextIntProvider> xp;
    private final Holder<ContextFloatProvider> reputationDiscount;
    private final Optional<Holder<LootItemCondition>> merchantPredicate;
    private final Optional<Holder<LootItemFunction>> givenItemModifier;
    private final Optional<HolderSet<Enchantment>> doubleTradePriceEnchantments;
    public static final Codec<VillagerTrade> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.item.trading.VillagerTrade.class, "CODEC");
    private VillagerTrade(TradeCost wants, Optional<TradeCost> additionalWants, ItemStackTemplate gives, Holder<ContextIntProvider> maxUses, Holder<ContextIntProvider> xp, Holder<ContextFloatProvider> reputationDiscount, Optional<Holder<LootItemCondition>> merchantPredicate, Optional<Holder<LootItemFunction>> givenItemModifier, Optional<HolderSet<Enchantment>> doubleTradePriceEnchantments) {
        this.wants = wants;
        this.additionalWants = java.util.Objects.requireNonNull(additionalWants);
        this.gives = gives;
        this.maxUses = maxUses;
        this.xp = xp;
        this.reputationDiscount = java.util.Objects.requireNonNull(reputationDiscount);
        this.merchantPredicate = java.util.Objects.requireNonNull(merchantPredicate);
        this.givenItemModifier = java.util.Objects.requireNonNull(givenItemModifier);
        this.doubleTradePriceEnchantments = java.util.Objects.requireNonNull(doubleTradePriceEnchantments);
    }
    public void validate(ValidationContext context) {
        Validatable.validate(context, "wants", wants);
        Validatable.validate(context, "additional_wants", additionalWants);
        Validatable.validateHolder(context, "max_uses", maxUses);
        Validatable.validateHolder(context, "reputation_discount", reputationDiscount);
        Validatable.validateHolder(context, "xp", xp);
        Validatable.validateHolder(context, "merchant_predicate", merchantPredicate);
        Validatable.validateHolder(context, "given_item_modifier", givenItemModifier);
    }
    public MerchantOffer getOffer(LootContext lootContext) {
        if (merchantPredicate.isPresent() && !merchantPredicate.get().value().test(lootContext)) {
            return null;
        }
        ItemStack product = gives.create();
        int surcharge = 0;
        if (givenItemModifier.isPresent()) {
            product = givenItemModifier.get().value().apply(product, lootContext);
            if (product.isEmpty()) {
                return null;
            }
        }
        Integer extra = product.remove(DataComponents.ADDITIONAL_TRADE_COST);
        if (extra != null) {
            surcharge += extra;
        }
        if (doubleTradePriceEnchantments.isPresent()) {
            var stored = product.get(DataComponents.STORED_ENCHANTMENTS);
            if (stored != null && stored.keySet().stream().anyMatch(doubleTradePriceEnchantments.get()::contains)) {
                surcharge *= 2;
            }
        }
        ItemCost firstCost = wants.toItemCost(lootContext, surcharge);
        if (firstCost.count() < 1) {
            return null;
        }
        Optional<ItemCost> secondCost = additionalWants.map(cost -> cost.toItemCost(lootContext, 0));
        if (secondCost.isPresent() && secondCost.get().count() < 1) {
            return null;
        }
        return new MerchantOffer(firstCost, secondCost, product, Math.max(maxUses.value().getInt(lootContext), 1),
            Math.max(xp.value().getInt(lootContext), 0), Math.max(reputationDiscount.value().getFloat(lootContext), 0.0F));
    }
    public static Builder builder(TradeCost wants, ItemStackTemplate gives, int maxUses, int xp, float reputationDiscount) {
        Holder<ContextIntProvider> uses = ContextIntProviders.exactly(maxUses);
        Holder<ContextIntProvider> experience = ContextIntProviders.exactly(xp);
        return new Builder(wants, gives, uses, experience, ContextFloatProviders.exactly(reputationDiscount));
    }
    public static Builder builder(TradeCost wants, TradeCost additionalWants, ItemStackTemplate gives, int maxUses, int xp, float reputationDiscount) {
        return builder(wants, gives, maxUses, xp, reputationDiscount).additionalWants(additionalWants);
    }
    public static class Builder {
        private final TradeCost wants;
        private Optional<TradeCost> additionalWants = Optional.empty();
        private final ItemStackTemplate gives;
        private final Holder<ContextIntProvider> maxUses;
        private final Holder<ContextIntProvider> xp;
        private final Holder<ContextFloatProvider> reputationDiscount;
        private Optional<Holder<LootItemCondition>> merchantPredicate = Optional.empty();
        private final List<Holder<LootItemFunction>> modifiers = new java.util.ArrayList<>();
        private Optional<HolderSet<Enchantment>> doubleTradePriceEnchantments = Optional.empty();
        public Builder(TradeCost wants, ItemStackTemplate gives, Holder<ContextIntProvider> maxUses, Holder<ContextIntProvider> xp, Holder<ContextFloatProvider> reputationDiscount) {
            this.wants = wants;
            this.gives = gives;
            this.maxUses = maxUses;
            this.xp = xp;
            this.reputationDiscount = java.util.Objects.requireNonNull(reputationDiscount);
        }
        public Builder additionalWants(TradeCost additionalWants) {
            this.additionalWants = Optional.ofNullable(additionalWants);
            return this;
        }
        public Builder merchantPredicate(Holder<LootItemCondition> merchantPredicate) {
            this.merchantPredicate = Optional.ofNullable(merchantPredicate);
            return this;
        }
        public Builder addModifier(LootItemFunction.Builder function) {
            modifiers.add(Holder.direct(function.build()));
            return this;
        }
        public final Builder addModifiers(Holder<LootItemFunction>... functions) {
            return addModifiers(java.util.Arrays.asList(functions));
        }
        public Builder addModifiers(Collection<Holder<LootItemFunction>> function) {
            modifiers.addAll(function);
            return this;
        }
        public Builder doubleTradePriceEnchantments(HolderSet<Enchantment> doubleTradePriceEnchantments) {
            this.doubleTradePriceEnchantments = Optional.ofNullable(doubleTradePriceEnchantments);
            return this;
        }
        public VillagerTrade build() {
            return new VillagerTrade(wants, additionalWants, gives, maxUses, xp, reputationDiscount, merchantPredicate,
                FunctionUserBuilder.buildFunction(List.copyOf(modifiers)), doubleTradePriceEnchantments);
        }
    }
}
