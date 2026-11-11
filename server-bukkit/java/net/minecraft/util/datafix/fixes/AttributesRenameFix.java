// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util.datafix.fixes;
import com.mojang.datafixers.DataFix;
import com.mojang.datafixers.TypeRewriteRule;
import com.mojang.datafixers.schemas.Schema;
import java.util.function.UnaryOperator;
public class AttributesRenameFix extends DataFix {
    public AttributesRenameFix(Schema outputSchema, String name, UnaryOperator<String> renames) {
        super(outputSchema, false);
    }
    public AttributesRenameFix(Schema outputSchema, String name, UnaryOperator<String> renames, boolean oldDataComponentFormat) {
        super(outputSchema, false);
    }
    protected TypeRewriteRule makeRule() {
        return org.basaltmc.bukkit.facade.NativeStepRule.of(getClass().getSimpleName(), getVersionKey());
    }
}
