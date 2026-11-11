// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.storage.loot.providers.number;
import com.mojang.serialization.codecs.RecordCodecBuilder;
import com.mojang.serialization.Codec;
import com.mojang.serialization.MapCodec;
import net.minecraft.core.Holder;
import net.minecraft.world.level.storage.loot.Validatable;
import net.minecraft.world.level.storage.loot.ValidationContext;
public interface RangeProvider<Value extends Validatable> extends Validatable {
    static <Value extends Validatable, Self extends RangeProvider<Value>> MapCodec<Self> mapCodec(Codec<Holder<Value>> valueCodec, Factory<Value, Self> factory) {
        return RecordCodecBuilder.mapCodec(instance -> instance.group(valueCodec.fieldOf("min").forGetter(RangeProvider::min),
            valueCodec.fieldOf("max").forGetter(RangeProvider::max)).apply(instance, factory::create));
    }
    Holder<Value> min();
    Holder<Value> max();
    default void validate(ValidationContext context) {
        Validatable.validateHolder(context, "min", min());
        Validatable.validateHolder(context, "max", max());

    }
    public interface Factory<Value extends Validatable, Self extends RangeProvider<Value>> {
        Self create(Holder<Value> min, Holder<Value> max);
    }
}
