// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util.datafix.fixes;
import com.mojang.datafixers.Typed;
import com.mojang.datafixers.schemas.Schema;
public class FixWolfHealth extends NamedEntityFix {
    public FixWolfHealth(Schema outputSchema) {
        super(outputSchema, false, "FixWolfHealth", net.minecraft.util.datafix.fixes.References.ENTITY, "minecraft:wolf");
    }
    protected Typed<?> fix(Typed<?> entity) {
        return org.basaltmc.bukkit.facade.NativeStep.typed("entity", getClass().getSimpleName(), getVersionKey(), entity);
    }
}
