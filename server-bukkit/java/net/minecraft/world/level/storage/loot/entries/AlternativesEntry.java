// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.storage.loot.entries;
import com.mojang.serialization.MapCodec;
import java.util.Collection;
import java.util.List;
import java.util.Optional;
import java.util.function.Function;
import net.minecraft.core.Holder;
import net.minecraft.util.ProblemReporter;
import net.minecraft.world.level.storage.loot.ValidationContext;
import net.minecraft.world.level.storage.loot.functions.LootItemFunction;
import net.minecraft.world.level.storage.loot.predicates.LootItemCondition;
public class AlternativesEntry extends CompositeEntryBase {
    public static final MapCodec<AlternativesEntry> MAP_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.storage.loot.entries.AlternativesEntry.class, "MAP_CODEC");
    public static final ProblemReporter.Problem UNREACHABLE_PROBLEM = null;
    public AlternativesEntry(List<LootPoolEntryContainer> children, Optional<Holder<LootItemCondition>> condition, Optional<Holder<LootItemFunction>> modifier) {
        super(children, condition, modifier);
    }
    protected String typeId() {
        return "alternatives";
    }
    public MapCodec<AlternativesEntry> codec() {
        return MAP_CODEC;
    }
    protected ComposableEntryContainer compose(List<? extends ComposableEntryContainer> entries) {
        return null;
    }
    public void validate(ValidationContext context) {
        return;
    }
    public static Builder alternatives(LootPoolEntryContainer.Builder<?>... entries) {
        return new Builder(entries);
    }
    public static <E> Builder alternatives(Collection<E> items, Function<E, LootPoolEntryContainer.Builder<?>> provider) {
        Builder builder = new Builder();
        for (E item : items) {
            builder.otherwise(provider.apply(item));
        }
        return builder;
    }
    public static class Builder extends CompositeEntryBase.Builder<AlternativesEntry, Builder> {
        public Builder(LootPoolEntryContainer.Builder<?>... entries) {
            super(entries);
        }
        private Builder addEntryAndThis(LootPoolEntryContainer.Builder<?> entry) {
            addEntry(entry);
            return this;
        }
        protected Builder getThis() {
            return this;
        }
        public Builder otherwise(LootPoolEntryContainer.Builder<?> other) {
            return addEntryAndThis(other);
        }
        public LootPoolEntryContainer build() {
            return build(AlternativesEntry::new);
        }
    }
}
