// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.storage.loot.providers.number;
import com.mojang.serialization.codecs.RecordCodecBuilder;
import com.mojang.serialization.Codec;
import com.mojang.serialization.MapCodec;
import net.minecraft.core.Holder;
import net.minecraft.world.level.storage.loot.Validatable;
import net.minecraft.world.level.storage.loot.ValidationContext;
public interface BinaryProvider<Value extends Validatable> extends Validatable {
    static <Value extends Validatable, Self extends BinaryProvider<Value>> MapCodec<Self> mapCodec(Codec<Holder<Value>> valueCodec, Factory<Value, Self> factory) {
        return RecordCodecBuilder.mapCodec(instance -> instance.group(valueCodec.fieldOf("left").forGetter(BinaryProvider::left),
            valueCodec.fieldOf("right").forGetter(BinaryProvider::right)).apply(instance, factory::create));
    }
    default void validate(ValidationContext context) {
        Validatable.validateHolder(context, "left", left());
        Validatable.validateHolder(context, "right", right());
    }
    Holder<Value> left();
    Holder<Value> right();
    public interface Factory<Value extends Validatable, Self extends BinaryProvider<Value>> {
        Self create(Holder<Value> left, Holder<Value> right);
    }
}
