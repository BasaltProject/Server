// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util.datafix.fixes;
import com.mojang.datafixers.schemas.Schema;
public class RenameNameplateToNameTagFix extends AttributesRenameFix {
    public RenameNameplateToNameTagFix(Schema outputSchema) {
        super(outputSchema, "RenameNameplateToNameTag", (java.util.function.UnaryOperator) null);
    }
}
