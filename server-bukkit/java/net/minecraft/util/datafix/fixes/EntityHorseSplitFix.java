// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util.datafix.fixes;
import com.mojang.datafixers.Typed;
import com.mojang.datafixers.schemas.Schema;
import com.mojang.datafixers.util.Pair;
public class EntityHorseSplitFix extends EntityRenameFix {
    public EntityHorseSplitFix(Schema outputSchema, boolean changesType) {
        super("EntityHorseSplitFix", outputSchema, changesType);
    }
    protected Pair<String, Typed<?>> fix(String name, Typed<?> entity) {
        return org.basaltmc.bukkit.facade.NativeStep.entityTyped(name, entity, getClass().getSimpleName(), getVersionKey());
    }
}
