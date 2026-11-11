// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util.datafix.fixes;
import com.mojang.datafixers.schemas.Schema;
public class AttributeIdPrefixFix extends AttributesRenameFix {
    public AttributeIdPrefixFix(Schema outputSchema) {
        super(outputSchema, "AttributeIdPrefixFix", (java.util.function.UnaryOperator) null);
    }
}
