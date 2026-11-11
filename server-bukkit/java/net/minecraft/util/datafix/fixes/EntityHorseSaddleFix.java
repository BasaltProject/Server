// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util.datafix.fixes;
import com.mojang.datafixers.Typed;
import com.mojang.datafixers.schemas.Schema;
public class EntityHorseSaddleFix extends NamedEntityFix {
    public EntityHorseSaddleFix(Schema outputSchema, boolean changesType) {
        super(outputSchema, changesType, "EntityHorseSaddleFix", net.minecraft.util.datafix.fixes.References.ENTITY, "EntityHorse");
    }
    protected Typed<?> fix(Typed<?> entity) {
        return org.basaltmc.bukkit.facade.NativeStep.typed("entity", getClass().getSimpleName(), getVersionKey(), entity);
    }
}
