// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util.datafix.fixes;
import com.mojang.datafixers.Typed;
import com.mojang.datafixers.schemas.Schema;
import com.mojang.datafixers.util.Pair;
import com.mojang.serialization.Dynamic;
public abstract class SimpleEntityRenameFix extends EntityRenameFix {
    public SimpleEntityRenameFix(String name, Schema outputSchema, boolean changesType) {
        super(name, outputSchema, changesType);
    }
    protected Pair<String, Typed<?>> fix(String name, Typed<?> entity) {
        return org.basaltmc.bukkit.facade.NativeStep.entityTyped(name, entity, getClass().getSimpleName(), getVersionKey());
    }
    protected abstract Pair<String, Dynamic<?>> getNewNameAndTag(String name, Dynamic<?> tag);
}
