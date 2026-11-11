// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util.datafix.fixes;
import com.mojang.datafixers.Typed;
import com.mojang.datafixers.schemas.Schema;
public class EntitySalmonSizeFix extends NamedEntityFix {
    public EntitySalmonSizeFix(Schema outputSchema) {
        super(outputSchema, false, "EntitySalmonSizeFix", net.minecraft.util.datafix.fixes.References.ENTITY, "minecraft:salmon");
    }
    protected Typed<?> fix(Typed<?> entity) {
        return org.basaltmc.bukkit.facade.NativeStep.typed("entity", getClass().getSimpleName(), getVersionKey(), entity);
    }
}
