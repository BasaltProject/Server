// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.attribute.modifier;
import com.mojang.serialization.Codec;
import java.util.List;
import net.minecraft.util.Util;
import net.minecraft.world.attribute.EnvironmentAttribute;
import net.minecraft.world.attribute.LerpFunction;
public interface ListModifier<Element> extends AttributeModifier<List<Element>, List<Element>> {
    @SuppressWarnings("unchecked")
    static <Element> ListModifier<Element> append() {
        return (ListModifier<Element>) Append.INSTANCE;
    }
    default Codec<List<Element>> argumentCodec(EnvironmentAttribute<List<Element>> attribute) {
        return attribute.valueCodec();
    }
    default LerpFunction<List<Element>> argumentKeyframeLerp(EnvironmentAttribute<List<Element>> attribute) {
        return attribute.type().keyframeLerp();
    }
    public static record Append<Element>() implements ListModifier<Element> {
        private static final Append<?> INSTANCE = new Append<>();
        public List<Element> apply(List<Element> subject, List<Element> argument) {
            if (argument.isEmpty()) {
                return subject;
            }
            return subject.isEmpty() ? argument : Util.join(subject, argument);
        }
    }
}
