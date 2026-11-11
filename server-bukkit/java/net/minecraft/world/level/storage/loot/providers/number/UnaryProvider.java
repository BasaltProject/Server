// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.storage.loot.providers.number;
import com.mojang.serialization.codecs.RecordCodecBuilder;
import com.mojang.serialization.Codec;
import com.mojang.serialization.MapCodec;
import net.minecraft.core.Holder;
import net.minecraft.world.level.storage.loot.Validatable;
import net.minecraft.world.level.storage.loot.ValidationContext;
public interface UnaryProvider<Value extends Validatable> extends Validatable {
    static <Value extends Validatable, Self extends UnaryProvider<Value>> MapCodec<Self> codec(Codec<Holder<Value>> valueCodec, Factory<Value, Self> factory) {
        return RecordCodecBuilder.mapCodec(instance -> instance.group(valueCodec.fieldOf("input").forGetter(UnaryProvider::input))
            .apply(instance, factory::create));
    }
    Holder<Value> input();
    default void validate(ValidationContext context) {
        Validatable.validateHolder(context, "input", input());
    }
    public interface Factory<Value extends Validatable, Self extends UnaryProvider<Value>> {
        Self create(Holder<Value> input);
    }
}
