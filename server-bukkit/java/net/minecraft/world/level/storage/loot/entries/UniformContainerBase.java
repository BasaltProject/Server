// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.storage.loot.entries;
import net.minecraft.util.Mth;
import com.mojang.datafixers.Products;
import com.mojang.serialization.MapCodec;
import com.mojang.serialization.codecs.RecordCodecBuilder;
import java.util.Optional;
import net.minecraft.core.Holder;
import net.minecraft.world.level.storage.loot.ValidationContext;
import net.minecraft.world.level.storage.loot.functions.FunctionUserBuilder;
import net.minecraft.world.level.storage.loot.functions.LootItemFunction;
import net.minecraft.world.level.storage.loot.predicates.LootItemCondition;
public abstract class UniformContainerBase extends LootPoolEntryContainer {
    public static final int DEFAULT_WEIGHT = 1;
    public static final int DEFAULT_QUALITY = 0;
    protected final int weight;
    protected final int quality;
    protected UniformContainerBase(int weight, int quality, Optional<Holder<LootItemCondition>> condition, Optional<Holder<LootItemFunction>> modifier) {
        super(condition, modifier);
        this.weight = weight;
        this.quality = quality;
    }
    protected void writeEntry(StringBuilder out) {
        super.writeEntry(out);
        out.append(",\"weight\":").append(weight).append(",\"quality\":").append(quality);
    }
    public abstract MapCodec<? extends UniformContainerBase> codec();
    protected static <T extends UniformContainerBase> Products.P4<RecordCodecBuilder.Mu<T>, Integer, Integer, Optional<Holder<LootItemCondition>>, Optional<Holder<LootItemFunction>>> uniformFields(RecordCodecBuilder.Instance<T> i) {
        return i.group(com.mojang.serialization.Codec.INT.optionalFieldOf("weight", 1).forGetter(e -> e.weight),
            com.mojang.serialization.Codec.INT.optionalFieldOf("quality", 0).forGetter(e -> e.quality))
            .and(commonFields(i).t1())
            .and(net.minecraft.world.level.storage.loot.functions.LootItemFunctions.CODEC.optionalFieldOf("modifier").forGetter(e -> e.modifier));
    }
    public void validate(ValidationContext context) {
        return;
    }
    public static Builder<?> simpleBuilder(EntryConstructor constructor) {
        return new DummyBuilder(constructor);
    }
    public static abstract class Builder<T extends Builder<T>> extends LootPoolEntryContainer.Builder<T> implements FunctionUserBuilder<T> {
        protected int weight = 1;
        protected int quality;
        public T setWeight(int weight) {
            this.weight = weight;
            return getThis();
        }
        public T setQuality(int quality) {
            this.quality = quality;
            return getThis();
        }
    }
    protected abstract class EntryBase implements LootPoolEntry {
        protected EntryBase() {
        }
        public int getWeight(float luck) {
            return Math.max(Mth.floor(weight + quality * luck), 0);
        }
    }
    static class DummyBuilder extends Builder<DummyBuilder> {
        private final EntryConstructor constructor;
        public DummyBuilder(EntryConstructor constructor) {
            this.constructor = constructor;
        }
        protected DummyBuilder getThis() {
            return this;
        }
        public LootPoolEntryContainer build() {
            return constructor.build(weight, quality, getCondition(), getModifier());
        }
    }
    public interface EntryConstructor {
        UniformContainerBase build(int weight, int quality, Optional<Holder<LootItemCondition>> condition, Optional<Holder<LootItemFunction>> modifier);
    }
}
