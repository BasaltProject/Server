// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.storage.loot.providers.number;
import com.mojang.serialization.codecs.RecordCodecBuilder;
import com.mojang.serialization.Codec;
import com.mojang.serialization.MapCodec;
import java.util.List;
import net.minecraft.core.Holder;
import net.minecraft.world.level.storage.loot.LootContext;
import net.minecraft.world.level.storage.loot.Validatable;
import net.minecraft.world.level.storage.loot.ValidationContext;
import net.minecraft.world.level.storage.loot.predicates.LootItemCondition;
public interface DispatcherProvider<Value extends Validatable> extends Validatable {
    static <Value extends Validatable, Self extends DispatcherProvider<Value>> MapCodec<Self> mapCodec(Codec<Holder<Value>> valueCodec, Holder<Value> defaultCase, Factory<Value, Self> factory) {
        return RecordCodecBuilder.<Self>mapCodec(instance -> instance.group(
            Case.codec(valueCodec).listOf().fieldOf("cases").forGetter(DispatcherProvider::cases),
            valueCodec.optionalFieldOf("default", defaultCase).forGetter(DispatcherProvider::defaultValue)).apply(instance, factory::create));
    }
    default Holder<Value> selectValue(LootContext context) {
        for (Case<Value> candidate : cases()) {
            if (candidate.test(context)) {
                return candidate.value();
            }
        }
        return defaultValue();
    }
    List<Case<Value>> cases();
    Holder<Value> defaultValue();
    default void validate(ValidationContext context) {
        Validatable.validate(context, "cases", cases());
        Validatable.validateHolder(context, "default", defaultValue());
    }
    public static record Case<Value extends Validatable>(Holder<LootItemCondition> condition, Holder<Value> value) implements Validatable {
        public static <Value extends Validatable> Codec<Case<Value>> codec(Codec<Holder<Value>> valueCodec) {
            return RecordCodecBuilder.<Case<Value>>create(instance -> instance.group(
                LootItemCondition.CODEC.fieldOf("condition").forGetter(Case::condition),
                valueCodec.fieldOf("value").forGetter(Case::value)).apply(instance, Case::new));
        }
        public boolean test(LootContext context) {
            return condition.value().test(context);
        }
        public void validate(ValidationContext context) {
            Validatable.validateHolder(context, "condition", condition);
            Validatable.validateHolder(context, "value", value);
        }
    }
    public interface Factory<Value extends Validatable, Self extends DispatcherProvider<Value>> {
        Self create(List<Case<Value>> cases, Holder<Value> defaultValue);
    }
}
