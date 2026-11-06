// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.storage.loot.predicates;
import com.mojang.serialization.MapCodec;
import com.mojang.serialization.codecs.RecordCodecBuilder;
import java.util.ArrayList;
import java.util.List;
import java.util.function.Function;
import java.util.function.Predicate;
import net.minecraft.core.Holder;
import net.minecraft.core.HolderSet;
import net.minecraft.world.level.storage.loot.LootContext;
import net.minecraft.world.level.storage.loot.ValidationContext;
public abstract class CompositeLootItemCondition implements LootItemCondition, org.basaltmc.bukkit.loot.LootJson.Node {
    protected final HolderSet<LootItemCondition> terms;
    private final Predicate<LootContext> composedPredicate;
    protected CompositeLootItemCondition(HolderSet<LootItemCondition> terms, Predicate<LootContext> composedPredicate) {
        this.terms = terms;
        this.composedPredicate = composedPredicate;
    }
    protected abstract String typeId();
    public void writeJson(StringBuilder out) {
        out.append("{\"type\":\"").append(typeId()).append("\",\"terms\":[");
        boolean first = true;
        for (Holder<LootItemCondition> term : terms) {
            if (!first) {
                out.append(',');
            }
            first = false;
            org.basaltmc.bukkit.loot.LootJson.value(out, term);
        }
        out.append("]}");
    }
    protected static List<Predicate<LootContext>> holdersToLazyPredicates(HolderSet<LootItemCondition> terms) {
        List<Predicate<LootContext>> out = new ArrayList<>();
        for (Holder<LootItemCondition> term : terms) {
            out.add(context -> term.value().test(context));
        }
        return out;
    }
    public abstract MapCodec<? extends CompositeLootItemCondition> codec();
    protected static <T extends CompositeLootItemCondition> MapCodec<T> createCodec(Function<HolderSet<LootItemCondition>, T> factory) {
        return RecordCodecBuilder.mapCodec(i -> i.group(
            LootItemCondition.LIST_CODEC.fieldOf("terms").forGetter(condition -> condition.terms)).apply(i, factory));
    }
    public final boolean test(LootContext context) {
        return composedPredicate.test(context);
    }
    public void validate(ValidationContext output) {
        return;
    }
    public static abstract class Builder implements LootItemCondition.Builder {
        private final List<Holder<LootItemCondition>> terms = new ArrayList<>();
        protected Builder(LootItemCondition.Builder... terms) {
            for (LootItemCondition.Builder term : terms) {
                this.terms.add(Holder.direct(term.build()));
            }
        }
        public void addTerm(LootItemCondition.Builder term) {
            addTerm(Holder.direct(term.build()));
        }
        public void addTerm(Holder<LootItemCondition> term) {
            terms.add(term);
        }
        public LootItemCondition build() {
            return create(HolderSet.direct(List.copyOf(terms)));
        }
        protected abstract LootItemCondition create(HolderSet<LootItemCondition> terms);
    }
}
