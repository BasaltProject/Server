// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util.datafix.fixes;
import com.mojang.datafixers.schemas.Schema;
import com.mojang.serialization.Dynamic;
import java.util.function.UnaryOperator;
public class LegacyBlockPropertyRenameAndFix extends LegacyAbstractBlockPropertyFix {
    public LegacyBlockPropertyRenameAndFix(Schema outputSchema, String name, String blockId, String oldPropertyName, String newPropertyName, UnaryOperator<String> valueFixer) {
        super(outputSchema, name);
        this.blockId = blockId;
        this.oldPropertyName = oldPropertyName;
        this.newPropertyName = newPropertyName;
        this.valueFixer = valueFixer;
    }
    private final String blockId;
    private final String oldPropertyName;
    private final String newPropertyName;
    private final UnaryOperator<String> valueFixer;
    protected boolean shouldFix(String blockId) {
        return blockId.equals(this.blockId);
    }
    protected <T> Dynamic<T> fixProperties(String blockId, Dynamic<T> properties) {
        return properties.renameAndFixField(oldPropertyName, newPropertyName, value -> value.createString(valueFixer.apply(value.asString(""))));
    }
}
