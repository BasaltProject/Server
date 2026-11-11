// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util.datafix.fixes;
import com.mojang.datafixers.Typed;
import com.mojang.datafixers.schemas.Schema;
public class SignTextStrictJsonFix extends NamedEntityFix {
    public SignTextStrictJsonFix(Schema outputSchema) {
        super(outputSchema, false, "SignTextStrictJsonFix", net.minecraft.util.datafix.fixes.References.BLOCK_ENTITY, "Sign");
    }
    protected Typed<?> fix(Typed<?> entity) {
        return org.basaltmc.bukkit.facade.NativeStep.typed("block_entity", getClass().getSimpleName(), getVersionKey(), entity);
    }
}
