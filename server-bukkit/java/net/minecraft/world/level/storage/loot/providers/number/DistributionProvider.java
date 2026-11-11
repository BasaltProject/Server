// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.storage.loot.providers.number;
import com.mojang.serialization.codecs.RecordCodecBuilder;
import com.mojang.serialization.Codec;
import com.mojang.serialization.MapCodec;
import net.minecraft.core.Holder;
import net.minecraft.util.random.WeightedList;
import net.minecraft.world.level.storage.loot.Validatable;
import net.minecraft.world.level.storage.loot.ValidationContext;
public interface DistributionProvider<Value extends Validatable> extends Validatable {
    WeightedList<Holder<Value>> distribution();
    static <Value extends Validatable, Self extends DistributionProvider<Value>> MapCodec<Self> mapCodec(Codec<Holder<Value>> valueCodec, Factory<Value, Self> factory) {
        return RecordCodecBuilder.<Self>mapCodec(instance -> instance.group(
            WeightedList.nonEmptyCodec(valueCodec).fieldOf("distribution").forGetter(DistributionProvider::distribution)).apply(instance, factory::create));


    }
    default void validate(ValidationContext context) {
        Validatable.validateHolder(context, "distribution", distribution().unwrap().stream().map(net.minecraft.util.random.Weighted::value).toList());
    }
    public interface Factory<Value extends Validatable, Self extends DistributionProvider<Value>> {
        Self create(WeightedList<Holder<Value>> distribution);
    }
}
