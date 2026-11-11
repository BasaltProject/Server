// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.storage.loot.providers.number;
import com.mojang.serialization.codecs.RecordCodecBuilder;
import com.mojang.serialization.Codec;
import com.mojang.serialization.MapCodec;
import net.minecraft.core.HolderSet;
import net.minecraft.world.level.storage.loot.Validatable;
import net.minecraft.world.level.storage.loot.ValidationContext;
public interface AggregateProvider<Value extends Validatable> extends Validatable {
    static <Value extends Validatable, Self extends AggregateProvider<Value>> MapCodec<Self> mapCodec(Codec<HolderSet<Value>> valueCodec, Factory<Value, Self> factory) {
        return RecordCodecBuilder.mapCodec(instance -> instance.group(valueCodec.fieldOf("inputs").forGetter(AggregateProvider::inputs))
            .apply(instance, factory::create));
    }
    HolderSet<Value> inputs();
    default void validate(ValidationContext context) {
        Validatable.validateHolderSet(context, "inputs", inputs(), 1);
    }
    public interface Factory<Value extends Validatable, Self extends AggregateProvider<Value>> {
        Self create(HolderSet<Value> value);
    }
}
