// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.storage.loot.entries;
import com.mojang.datafixers.Products;
import com.mojang.serialization.MapCodec;
import com.mojang.serialization.codecs.RecordCodecBuilder;
import java.util.Optional;
import java.util.function.Consumer;
import net.minecraft.core.Holder;
import net.minecraft.world.level.storage.loot.LootContext;
import net.minecraft.world.level.storage.loot.Validatable;
import net.minecraft.world.level.storage.loot.ValidationContext;
import net.minecraft.world.level.storage.loot.functions.FunctionUserBuilder;
import net.minecraft.world.level.storage.loot.functions.LootItemFunction;
import net.minecraft.world.level.storage.loot.predicates.ConditionUserBuilder;
import net.minecraft.world.level.storage.loot.predicates.LootItemCondition;
public abstract class LootPoolEntryContainer implements ComposableEntryContainer, Validatable, org.basaltmc.bukkit.loot.LootJson.Node {
    protected final Optional<Holder<LootItemCondition>> condition;
    protected final Optional<Holder<LootItemFunction>> modifier;
    protected LootPoolEntryContainer(Optional<Holder<LootItemCondition>> condition, Optional<Holder<LootItemFunction>> modifier) {
        this.condition = condition;
        this.modifier = modifier;
    }
    protected void writeEntry(StringBuilder out) {
        out.append("\"type\":\"minecraft:empty\"");
    }
    public final void writeJson(StringBuilder out) {
        out.append('{');
        writeEntry(out);
        org.basaltmc.bukkit.loot.LootJson.optional(out, "condition", condition);
        org.basaltmc.bukkit.loot.LootJson.optional(out, "modifier", modifier);
        out.append('}');
    }
    protected static <T extends LootPoolEntryContainer> Products.P2<RecordCodecBuilder.Mu<T>, Optional<Holder<LootItemCondition>>, Optional<Holder<LootItemFunction>>> commonFields(RecordCodecBuilder.Instance<T> i) {
        return i.group(LootItemCondition.CODEC.optionalFieldOf("condition").forGetter(e -> e.condition),
            net.minecraft.world.level.storage.loot.functions.LootItemFunctions.CODEC.optionalFieldOf("modifier").forGetter(e -> e.modifier));
    }
    public void validate(ValidationContext output) {
        return;
    }
    protected abstract boolean expandRaw(LootContext context, Consumer<LootPoolEntry> output);
    public final boolean expand(LootContext context, Consumer<LootPoolEntry> output) {
        if (condition.isPresent() && !condition.get().value().test(context)) {
            return false;
        }
        Consumer<LootPoolEntry> adjusted = modifier.isEmpty() ? output : raw -> output.accept(new LootPoolEntry() {
            public int getWeight(float luck) {
                return raw.getWeight(luck);
            }
            public void createItemStack(Consumer<net.minecraft.world.item.ItemStack> stacks, LootContext lootContext) {
                raw.createItemStack(LootItemFunction.decorate(modifier, stacks, lootContext), lootContext);
            }
        });
        return expandRaw(context, adjusted);
    }
    public abstract MapCodec<? extends LootPoolEntryContainer> codec();
    public static abstract class Builder<T extends Builder<T>> implements ConditionUserBuilder<T>, FunctionUserBuilder<T> {
        private final java.util.List<Holder<LootItemCondition>> conditions = new java.util.ArrayList<>();
        private final java.util.List<Holder<LootItemFunction>> functions = new java.util.ArrayList<>();
        protected abstract T getThis();
        public T when(Holder<LootItemCondition> condition) {
            conditions.add(condition);
            return getThis();
        }
        public T apply(Holder<LootItemFunction> function) {
            functions.add(function);
            return getThis();
        }
        public final T unwrap() {
            return getThis();
        }
        protected Optional<Holder<LootItemCondition>> getCondition() {
            return net.minecraft.world.level.storage.loot.predicates.ConditionUserBuilder.buildCondition(conditions);
        }
        protected Optional<Holder<LootItemFunction>> getModifier() {
            return net.minecraft.world.level.storage.loot.functions.FunctionUserBuilder.buildFunction(functions);
        }
        public AlternativesEntry.Builder otherwise(Builder<?> other) {
            return new AlternativesEntry.Builder(this, other);
        }
        public EntryGroup.Builder append(Builder<?> other) {
            return new EntryGroup.Builder(this, other);
        }
        public SequentialEntry.Builder then(Builder<?> other) {
            return new SequentialEntry.Builder(this, other);
        }
        public abstract LootPoolEntryContainer build();
    }
}
