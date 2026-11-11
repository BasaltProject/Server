// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.storage.loot.providers.number;
import com.mojang.serialization.codecs.RecordCodecBuilder;
import com.mojang.serialization.Codec;
import com.mojang.serialization.MapCodec;
import net.minecraft.core.Holder;
import net.minecraft.world.level.storage.loot.Validatable;
import net.minecraft.world.level.storage.loot.ValidationContext;
public interface PowerProvider<Value extends Validatable> extends Validatable {
    static <Value extends Validatable, Self extends PowerProvider<Value>> MapCodec<Self> mapCodec(Codec<Holder<Value>> valueCodec, Factory<Value, Self> factory) {
        return RecordCodecBuilder.<Self>mapCodec(instance -> instance.group(
            valueCodec.fieldOf("base").forGetter(PowerProvider::base),
            valueCodec.fieldOf("exponent").forGetter(PowerProvider::exponent)).apply(instance, factory::create));
    }
    default void validate(ValidationContext context) {
        Validatable.validateHolder(context, "base", base());
        Validatable.validateHolder(context, "exponent", exponent());
    }
    Holder<Value> base();
    Holder<Value> exponent();
    public interface Factory<Value extends Validatable, Self extends PowerProvider<Value>> {
        Self create(Holder<Value> left, Holder<Value> right);
    }
}
