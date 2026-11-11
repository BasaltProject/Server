// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util.datafix.schemas;
import com.mojang.datafixers.DSL;
import com.mojang.datafixers.schemas.Schema;
import com.mojang.datafixers.types.Type;
import com.mojang.serialization.codecs.PrimitiveCodec;
public class NamespacedSchema extends Schema {
    public static final PrimitiveCodec<String> NAMESPACED_STRING_CODEC = new PrimitiveCodec<String>() {
        public <T> com.mojang.serialization.DataResult<String> read(com.mojang.serialization.DynamicOps<T> ops, T input) {
            return ops.getStringValue(input).map(NamespacedSchema::ensureNamespaced);
        }
        public <T> T write(com.mojang.serialization.DynamicOps<T> ops, String value) {
            return ops.createString(value);
        }
        public String toString() {
            return "NamespacedString";
        }
    };
    private static final Type<String> kNamespacedString = new com.mojang.datafixers.types.templates.Const.PrimitiveType<>(NAMESPACED_STRING_CODEC);
    public NamespacedSchema(int versionKey, Schema parent) {
        super(versionKey, parent);
    }
    public static String ensureNamespaced(String input) {
        return org.basaltmc.bukkit.facade.Names.namespaced(input);
    }
    public static Type<String> namespacedString() {
        return kNamespacedString;
    }
    public Type<?> getChoiceType(DSL.TypeReference type, String choiceName) {
        return super.getChoiceType(type, NamespacedSchema.ensureNamespaced(choiceName));
    }
}
