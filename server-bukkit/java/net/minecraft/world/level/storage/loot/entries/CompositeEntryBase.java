// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.storage.loot.entries;
import com.google.common.collect.ImmutableList;
import com.mojang.serialization.MapCodec;
import java.util.List;
import java.util.Optional;
import java.util.function.Consumer;
import net.minecraft.core.Holder;
import net.minecraft.util.ProblemReporter;
import net.minecraft.world.level.storage.loot.LootContext;
import net.minecraft.world.level.storage.loot.ValidationContext;
import net.minecraft.world.level.storage.loot.functions.LootItemFunction;
import net.minecraft.world.level.storage.loot.predicates.LootItemCondition;
public abstract class CompositeEntryBase extends LootPoolEntryContainer {
    public static final ProblemReporter.Problem NO_CHILDREN_PROBLEM = () -> "Empty children list";
    protected final List<LootPoolEntryContainer> children;
    protected CompositeEntryBase(List<LootPoolEntryContainer> children, Optional<Holder<LootItemCondition>> condition, Optional<Holder<LootItemFunction>> modifier) {
        super(condition, modifier);
        this.children = children;
    }
    protected abstract String typeId();
    protected void writeEntry(StringBuilder out) {
        out.append("\"type\":\"minecraft:").append(typeId()).append("\",\"children\":");
        org.basaltmc.bukkit.loot.LootJson.nodes(out, children);
    }
    public abstract MapCodec<? extends CompositeEntryBase> codec();
    public void validate(ValidationContext context) {
        return;
    }
    protected abstract ComposableEntryContainer compose(List<? extends ComposableEntryContainer> entries);
    public final boolean expandRaw(LootContext context, Consumer<LootPoolEntry> output) {
        return false;
    }
    public static <T extends CompositeEntryBase> MapCodec<T> createCodec(CompositeEntryConstructor<T> constructor) {
        return null;
    }
    public static abstract class Builder<T extends CompositeEntryBase, B extends Builder<T, B>> extends LootPoolEntryContainer.Builder<B> {
        private final java.util.List<LootPoolEntryContainer.Builder<?>> entries = new java.util.ArrayList<>();
        public Builder(LootPoolEntryContainer.Builder<?>... entries) {
            this.entries.addAll(java.util.Arrays.asList(entries));
        }
        protected ImmutableList.Builder<LootPoolEntryContainer> addEntry(LootPoolEntryContainer.Builder<?> entry) {
            this.entries.add(entry);
            return null;
        }
        protected T build(CompositeEntryConstructor<T> constructor) {
            java.util.List<LootPoolEntryContainer> built = new java.util.ArrayList<>();
            for (LootPoolEntryContainer.Builder<?> entry : entries) {
                built.add(entry.build());
            }
            return constructor.create(built, getCondition(), getModifier());
        }
    }
    public interface CompositeEntryConstructor<T extends CompositeEntryBase> {
        T create(List<LootPoolEntryContainer> children, Optional<Holder<LootItemCondition>> condition, Optional<Holder<LootItemFunction>> modifier);
    }
}
