// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util.datafix.fixes;
import com.mojang.datafixers.schemas.Schema;
import com.mojang.serialization.Dynamic;
public class HorseBodyArmorItemFix extends NamedEntityWriteReadFix {
    public HorseBodyArmorItemFix(Schema outputSchema, String entityName, String previousBodyArmorTag, boolean clearArmorItems) {
        super(outputSchema, true, entityName, net.minecraft.util.datafix.fixes.References.ENTITY, entityName);
    }
    protected <T> Dynamic<T> fix(Dynamic<T> input) {
        return org.basaltmc.bukkit.facade.NativeStep.dynamic("entity", getClass().getSimpleName(), getVersionKey(), input);
    }
}
