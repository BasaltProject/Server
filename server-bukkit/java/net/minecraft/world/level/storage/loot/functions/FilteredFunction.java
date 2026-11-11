// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.storage.loot.functions;
import com.mojang.serialization.MapCodec;
import java.util.Optional;
import net.minecraft.advancements.predicates.ItemPredicate;
import net.minecraft.core.Holder;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.level.storage.loot.LootContext;
import net.minecraft.world.level.storage.loot.ValidationContext;
import net.minecraft.world.level.storage.loot.predicates.LootItemCondition;
public class FilteredFunction extends LootItemConditionalFunction {
    public static final MapCodec<FilteredFunction> MAP_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.storage.loot.functions.FilteredFunction.class, "MAP_CODEC");
    private final ItemPredicate filter;
    private final Optional<Holder<LootItemFunction>> onPass;
    private final Optional<Holder<LootItemFunction>> onFail;
    private FilteredFunction(Optional<Holder<LootItemCondition>> condition, ItemPredicate filter, Optional<Holder<LootItemFunction>> onPass, Optional<Holder<LootItemFunction>> onFail) {
        super(condition);
        this.filter = filter;
        this.onPass = onPass;
        this.onFail = onFail;
    }
    protected void writeFunction(StringBuilder out) {
        out.append("\"type\":\"minecraft:filtered\",\"item_filter\":");
        org.basaltmc.bukkit.loot.LootJson.itemFilter(out, filter);
        org.basaltmc.bukkit.loot.LootJson.optional(out, "on_pass", onPass);
        org.basaltmc.bukkit.loot.LootJson.optional(out, "on_fail", onFail);
    }
    public MapCodec<FilteredFunction> codec() {
        return MAP_CODEC;
    }
    public ItemStack run(ItemStack itemStack, LootContext context) {
        Optional<Holder<LootItemFunction>> branch = filter.test(itemStack) ? onPass : onFail;
        return branch.isPresent() ? branch.get().value().apply(itemStack, context) : itemStack;
    }
    public void validate(ValidationContext context) {
        return;
    }
    public static Builder filtered(ItemPredicate predicate) {
        return new Builder(predicate);
    }
    public static class Builder extends LootItemConditionalFunction.Builder<Builder> {
        private final ItemPredicate itemPredicate;
        private Optional<LootItemFunction> onPass = Optional.empty();
        private Optional<LootItemFunction> onFail = Optional.empty();
        private Builder(ItemPredicate itemPredicate) {
            this.itemPredicate = itemPredicate;
        }
        protected Builder getThis() {
            return this;
        }
        public Builder onPass(LootItemFunction onPass) {
            this.onPass = Optional.of(onPass);
            return this;
        }
        public Builder onFail(LootItemFunction onFail) {
            this.onFail = Optional.of(onFail);
            return this;
        }
        public LootItemFunction build() {
            return new FilteredFunction(getCondition(), itemPredicate, onPass.map(Holder::direct), onFail.map(Holder::direct));
        }
    }
}
