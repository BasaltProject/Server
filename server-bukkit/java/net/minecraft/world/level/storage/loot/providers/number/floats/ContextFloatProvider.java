// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.storage.loot.providers.number.floats;
import com.mojang.serialization.MapCodec;
import net.minecraft.world.level.storage.loot.LootContext;
import net.minecraft.world.level.storage.loot.Validatable;
public interface ContextFloatProvider extends Validatable {
    float getFloatUnsafe(LootContext context) throws ArithmeticException;
    default float getFloat(LootContext context) {
        try {
            return getFloatOrThrow(context);
        } catch (ArithmeticException e) {
            return 0.0F;
        }
    }
    default float getFloatOrThrow(LootContext context) throws ArithmeticException {
        float value = getFloatUnsafe(context);
        if (!Float.isFinite(value)) {
            throw new ArithmeticException("not finite");
        }
        return value;
    }
    MapCodec<? extends ContextFloatProvider> codec();
    static float intToFloatSafe(int value) {
        return (float) value;
    }
}
