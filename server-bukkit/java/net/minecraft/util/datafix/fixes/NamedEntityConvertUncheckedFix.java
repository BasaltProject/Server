// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util.datafix.fixes;
import com.mojang.datafixers.DSL;
import com.mojang.datafixers.Typed;
import com.mojang.datafixers.schemas.Schema;
public class NamedEntityConvertUncheckedFix extends NamedEntityFix {
    public NamedEntityConvertUncheckedFix(Schema outputSchema, String name, DSL.TypeReference type, String entityName) {
        super(outputSchema, true, name, type, entityName);
    }
    protected Typed<?> fix(Typed<?> entity) {
        return org.basaltmc.bukkit.facade.NativeStep.typed("entity", getClass().getSimpleName(), getVersionKey(), entity);
    }
}
