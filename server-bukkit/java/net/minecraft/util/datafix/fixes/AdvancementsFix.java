// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util.datafix.fixes;
import com.mojang.datafixers.schemas.Schema;
public class AdvancementsFix extends AdvancementsRenameFix {
    public AdvancementsFix(Schema outputSchema, boolean changesType) {
        super(outputSchema, changesType, "AdvancementsFix", (java.util.function.Function) null);
    }
}
