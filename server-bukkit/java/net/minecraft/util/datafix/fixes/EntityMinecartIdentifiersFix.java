// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util.datafix.fixes;
import com.mojang.datafixers.Typed;
import com.mojang.datafixers.schemas.Schema;
import com.mojang.datafixers.util.Pair;
public class EntityMinecartIdentifiersFix extends EntityRenameFix {
    public EntityMinecartIdentifiersFix(Schema outputSchema) {
        super("EntityMinecartIdentifiersFix", outputSchema, true);
    }
    protected Pair<String, Typed<?>> fix(String name, Typed<?> entity) {
        return org.basaltmc.bukkit.facade.NativeStep.entityTyped(name, entity, getClass().getSimpleName(), getVersionKey());
    }
}
