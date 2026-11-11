// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util.datafix.fixes;
import com.mojang.datafixers.DSL;
import com.mojang.datafixers.Typed;
import com.mojang.datafixers.schemas.Schema;
import java.util.Map;
public class VariantRenameFix extends NamedEntityFix {
    public VariantRenameFix(Schema outputSchema, String name, DSL.TypeReference type, String entityName, Map<String, String> renames) {
        super(outputSchema, false, name, type, entityName);
    }
    protected Typed<?> fix(Typed<?> typed) {
        return org.basaltmc.bukkit.facade.NativeStep.typed("entity", getClass().getSimpleName(), getVersionKey(), typed);
    }
}
