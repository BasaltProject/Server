// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.storage.loot.providers.number.ints;
import com.mojang.serialization.MapCodec;
import net.minecraft.world.level.storage.loot.LootContext;
import net.minecraft.world.level.storage.loot.Validatable;
public interface ContextIntProvider extends Validatable {
    int getIntUnsafe(LootContext context) throws ArithmeticException;
    default int getInt(LootContext context) {
        try {
            return getIntUnsafe(context);
        } catch (ArithmeticException e) {
            return 0;
        }
    }
    MapCodec<? extends ContextIntProvider> codec();
    static int floatToIntSafe(float value) {
        if (!Float.isFinite(value)) {
            throw new ArithmeticException("not finite");
        }
        return (int) value;
    }
    static int longToIntSafe(long value) {
        return Math.toIntExact(value);
    }
}
