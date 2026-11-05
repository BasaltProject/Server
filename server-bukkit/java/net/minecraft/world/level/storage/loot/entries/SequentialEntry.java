// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.storage.loot.entries;
import com.mojang.serialization.MapCodec;
import java.util.List;
import java.util.Optional;
import net.minecraft.core.Holder;
import net.minecraft.world.level.storage.loot.functions.LootItemFunction;
import net.minecraft.world.level.storage.loot.predicates.LootItemCondition;
public class SequentialEntry extends CompositeEntryBase {
    public static final MapCodec<SequentialEntry> MAP_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.storage.loot.entries.SequentialEntry.class, "MAP_CODEC");
    public SequentialEntry(List<LootPoolEntryContainer> children, Optional<Holder<LootItemCondition>> condition, Optional<Holder<LootItemFunction>> modifier) {
        super(children, condition, modifier);
    }
    protected String typeId() {
        return "sequence";
    }
    public MapCodec<SequentialEntry> codec() {
        return MAP_CODEC;
    }
    protected ComposableEntryContainer compose(List<? extends ComposableEntryContainer> entries) {
        return null;
    }
    public static Builder sequential(LootPoolEntryContainer.Builder<?>... entries) {
        return new Builder(entries);
    }
    public static class Builder extends CompositeEntryBase.Builder<SequentialEntry, Builder> {
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
        public Builder then(LootPoolEntryContainer.Builder<?> other) {
            return addEntryAndThis(other);
        }
        public LootPoolEntryContainer build() {
            return build(SequentialEntry::new);
        }
    }
}
