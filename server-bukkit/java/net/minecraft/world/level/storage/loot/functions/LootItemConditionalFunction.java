// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.storage.loot.functions;
import com.mojang.datafixers.Products;
import com.mojang.serialization.MapCodec;
import com.mojang.serialization.codecs.RecordCodecBuilder;
import java.util.Optional;
import java.util.function.Function;
import net.minecraft.core.Holder;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.level.storage.loot.LootContext;
import net.minecraft.world.level.storage.loot.ValidationContext;
import net.minecraft.world.level.storage.loot.predicates.ConditionUserBuilder;
import net.minecraft.world.level.storage.loot.predicates.LootItemCondition;
public abstract class LootItemConditionalFunction implements LootItemFunction, org.basaltmc.bukkit.loot.LootJson.Node {
    protected final Optional<Holder<LootItemCondition>> condition;
    protected LootItemConditionalFunction(Optional<Holder<LootItemCondition>> condition) {
        this.condition = condition;
    }
    protected void writeFunction(StringBuilder out) {
        out.append("\"type\":\"minecraft:set_count\",\"count\":1");
    }
    public final void writeJson(StringBuilder out) {
        out.append('{');
        writeFunction(out);
        org.basaltmc.bukkit.loot.LootJson.optional(out, "condition", condition);
        out.append('}');
    }
    public abstract MapCodec<? extends LootItemConditionalFunction> codec();
    protected static <T extends LootItemConditionalFunction> Products.P1<RecordCodecBuilder.Mu<T>, Optional<Holder<LootItemCondition>>> commonFields(RecordCodecBuilder.Instance<T> i) {
        return i.group(LootItemCondition.CODEC.optionalFieldOf("condition").forGetter(f -> f.condition));
    }
    public final ItemStack apply(ItemStack itemStack, LootContext context) {
        if (condition.isPresent() && !condition.get().value().test(context)) {
            return itemStack;
        }
        return run(itemStack, context);
    }
    protected abstract ItemStack run(ItemStack itemStack, LootContext context);
    public void validate(ValidationContext context) {
        return;
    }
    protected static Builder<?> simpleBuilder(Function<Optional<Holder<LootItemCondition>>, LootItemFunction> constructor) {
        return new DummyBuilder(constructor);
    }
    public static abstract class Builder<T extends Builder<T>> implements LootItemFunction.Builder, ConditionUserBuilder<T> {
        public T when(Holder<LootItemCondition> condition) {
            conditions.add(condition);
            return getThis();
        }
        public final T unwrap() {
            return getThis();
        }
        private final java.util.List<Holder<LootItemCondition>> conditions = new java.util.ArrayList<>();
        protected abstract T getThis();
        protected Optional<Holder<LootItemCondition>> getCondition() {
            return ConditionUserBuilder.buildCondition(conditions);
        }
    }
    static final class DummyBuilder extends Builder<DummyBuilder> {
        private final Function<Optional<Holder<LootItemCondition>>, LootItemFunction> constructor;
        public DummyBuilder(Function<Optional<Holder<LootItemCondition>>, LootItemFunction> constructor) {
            this.constructor = constructor;
        }
        protected DummyBuilder getThis() {
            return this;
        }
        public LootItemFunction build() {
            return constructor.apply(getCondition());
        }
    }
}
