// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util.datafix.fixes;
import com.mojang.datafixers.DataFix;
import com.mojang.datafixers.TypeRewriteRule;
import com.mojang.datafixers.schemas.Schema;
import java.util.UUID;
public class AttributeModifierIdFix extends DataFix {
    public AttributeModifierIdFix(Schema outputSchema) {
        super(outputSchema, false);
    }
    protected TypeRewriteRule makeRule() {
        return org.basaltmc.bukkit.facade.NativeStepRule.of(getClass().getSimpleName(), getVersionKey());
    }
    public static UUID uuidFromIntArray(int[] intArray) {
        if (intArray.length != 4) {
            return null;
        }
        return new UUID(((long) intArray[0] << 32) | (intArray[1] & 0xFFFFFFFFL), ((long) intArray[2] << 32) | (intArray[3] & 0xFFFFFFFFL));
    }
}
