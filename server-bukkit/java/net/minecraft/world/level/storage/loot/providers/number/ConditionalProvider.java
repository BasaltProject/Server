// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.storage.loot.providers.number;
import com.mojang.serialization.codecs.RecordCodecBuilder;
import com.mojang.serialization.Codec;
import com.mojang.serialization.MapCodec;
import net.minecraft.core.Holder;
import net.minecraft.world.level.storage.loot.LootContext;
import net.minecraft.world.level.storage.loot.Validatable;
import net.minecraft.world.level.storage.loot.ValidationContext;
import net.minecraft.world.level.storage.loot.predicates.LootItemCondition;
public interface ConditionalProvider<Value extends Validatable> extends Validatable {
    static <Value extends Validatable, Self extends ConditionalProvider<Value>> MapCodec<Self> mapCodec(Codec<Holder<Value>> valueCodec, Holder<Value> defaultIfFalse, Factory<Value, Self> factory) {
        return RecordCodecBuilder.<Self>mapCodec(instance -> instance.group(
            LootItemCondition.CODEC.fieldOf("condition").forGetter(ConditionalProvider::condition),
            valueCodec.fieldOf("on_true").forGetter(ConditionalProvider::onTrue),
            valueCodec.optionalFieldOf("on_false", defaultIfFalse).forGetter(ConditionalProvider::onFalse)).apply(instance, factory::create));
    }
    Holder<LootItemCondition> condition();
    Holder<Value> onTrue();
    Holder<Value> onFalse();
    default Holder<Value> selectValue(LootContext context) {
        return condition().value().test(context) ? onTrue() : onFalse();


    }
    default void validate(ValidationContext context) {
        Validatable.validateHolder(context, "condition", condition());
        Validatable.validateHolder(context, "on_true", onTrue());
        Validatable.validateHolder(context, "on_false", onFalse());
    }
    public interface Factory<Value extends Validatable, Self extends ConditionalProvider<Value>> {
        Self create(Holder<LootItemCondition> condition, Holder<Value> onTrue, Holder<Value> onFalse);
    }
}
