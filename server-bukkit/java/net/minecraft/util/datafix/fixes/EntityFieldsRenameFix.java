// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util.datafix.fixes;
import com.mojang.datafixers.Typed;
import com.mojang.datafixers.schemas.Schema;
import com.mojang.serialization.Dynamic;
import java.util.Map;
public class EntityFieldsRenameFix extends NamedEntityFix {
    public EntityFieldsRenameFix(Schema outputSchema, String name, String entityType, Map<String, String> renames) {
        super(outputSchema, false, name, net.minecraft.util.datafix.fixes.References.ENTITY, entityType);
    }
    public Dynamic<?> fixTag(Dynamic<?> data) {
        return org.basaltmc.bukkit.facade.NativeStep.dynamic("entity", getClass().getSimpleName(), getVersionKey(), data);
    }
    protected Typed<?> fix(Typed<?> entity) {
        return org.basaltmc.bukkit.facade.NativeStep.typed("entity", getClass().getSimpleName(), getVersionKey(), entity);
    }
}
