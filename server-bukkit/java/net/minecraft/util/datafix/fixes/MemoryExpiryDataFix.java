// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util.datafix.fixes;
import com.mojang.datafixers.Typed;
import com.mojang.datafixers.schemas.Schema;
import com.mojang.serialization.Dynamic;
public class MemoryExpiryDataFix extends NamedEntityFix {
    public MemoryExpiryDataFix(Schema schema, String entityType) {
        super(schema, false, entityType, net.minecraft.util.datafix.fixes.References.ENTITY, entityType);
    }
    protected Typed<?> fix(Typed<?> entity) {
        return org.basaltmc.bukkit.facade.NativeStep.typed("entity", getClass().getSimpleName(), getVersionKey(), entity);
    }
    public Dynamic<?> fixTag(Dynamic<?> input) {
        return org.basaltmc.bukkit.facade.NativeStep.dynamic("entity", getClass().getSimpleName(), getVersionKey(), input);
    }
}
