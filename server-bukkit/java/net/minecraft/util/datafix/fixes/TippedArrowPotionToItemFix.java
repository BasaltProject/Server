// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util.datafix.fixes;
import com.mojang.datafixers.schemas.Schema;
import com.mojang.serialization.Dynamic;
public class TippedArrowPotionToItemFix extends NamedEntityWriteReadFix {
    public TippedArrowPotionToItemFix(Schema outputSchema) {
        super(outputSchema, false, "TippedArrowPotionToItemFix", net.minecraft.util.datafix.fixes.References.ENTITY, "minecraft:arrow");
    }
    protected <T> Dynamic<T> fix(Dynamic<T> input) {
        return org.basaltmc.bukkit.facade.NativeStep.dynamic("entity", getClass().getSimpleName(), getVersionKey(), input);
    }
}
