// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util.datafix.fixes;
import com.mojang.datafixers.Typed;
import com.mojang.datafixers.schemas.Schema;
public class PotDecorationsBlockEntityUnflatteningFix extends NamedEntityFix {
    public PotDecorationsBlockEntityUnflatteningFix(Schema outputSchema) {
        super(outputSchema, true, "PotDecorationsBlockEntityUnflatteningFix", net.minecraft.util.datafix.fixes.References.BLOCK_ENTITY, "minecraft:decorated_pot");
    }
    protected Typed<?> fix(Typed<?> entity) {
        return org.basaltmc.bukkit.facade.NativeStep.typed("block_entity", getClass().getSimpleName(), getVersionKey(), entity);
    }
}
