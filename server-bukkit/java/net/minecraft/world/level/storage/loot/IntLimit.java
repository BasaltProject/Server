// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.storage.loot;
import net.minecraft.world.level.storage.loot.providers.number.ints.ContextIntProviders;
import com.mojang.serialization.Codec;
import java.util.Optional;
import net.minecraft.core.Holder;
import net.minecraft.world.level.storage.loot.providers.number.ints.ContextIntProvider;
public class IntLimit implements Validatable {
    private final Optional<Holder<ContextIntProvider>> min;
    private final Optional<Holder<ContextIntProvider>> max;
    public static final Codec<IntLimit> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.level.storage.loot.IntLimit.class, "CODEC");
    private IntLimit(Optional<Holder<ContextIntProvider>> min, Optional<Holder<ContextIntProvider>> max) {
        this.min = min;
        this.max = max;
    }
    public static IntLimit range(int min, int max) {
        return new IntLimit(Optional.of(ContextIntProviders.exactly(min)), Optional.of(ContextIntProviders.exactly(max)));
    }
    public static IntLimit lowerBound(int value) {
        return new IntLimit(Optional.of(ContextIntProviders.exactly(value)), Optional.empty());
    }
    public static IntLimit upperBound(int value) {
        return new IntLimit(Optional.empty(), Optional.of(ContextIntProviders.exactly(value)));
    }
    public void validate(ValidationContext context) {
    }
    public int clamp(LootContext context, int input) {
        int result = input;
        if (min.isPresent()) {
            result = Math.max(min.get().value().getInt(context), result);
        }
        if (max.isPresent()) {
            result = Math.min(max.get().value().getInt(context), result);
        }
        return result;


    }
    interface IntLimiter {
        int apply(LootContext context, int value);
    }
}
