// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.attribute.modifier;
import com.mojang.serialization.Codec;
import net.minecraft.world.attribute.EnvironmentAttribute;
import net.minecraft.world.attribute.LerpFunction;
public interface IntegerModifier<Argument> extends AttributeModifier<Integer, Argument> {
    Simple ADD = Integer::sum;
    Simple SUBTRACT = (a, b) -> a - b;
    Simple MULTIPLY = (a, b) -> a * b;
    Simple MINIMUM = Math::min;
    Simple MAXIMUM = Math::max;
    Integer apply(Integer integer, Argument argument);
    @FunctionalInterface
    public interface Simple extends IntegerModifier<Integer> {
        default Codec<Integer> argumentCodec(EnvironmentAttribute<Integer> type) {
            return Codec.INT;
        }
        default LerpFunction<Integer> argumentKeyframeLerp(EnvironmentAttribute<Integer> type) {
            return LerpFunction.ofInteger();
        }
    }
}
