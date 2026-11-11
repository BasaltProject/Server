// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util.datafix.fixes;
import com.mojang.datafixers.schemas.Schema;
public class EntityTippedArrowFix extends SimplestEntityRenameFix {
    public EntityTippedArrowFix(Schema outputSchema, boolean changesType) {
        super("EntityTippedArrowFix", outputSchema, changesType);
    }
    protected String rename(String name) {
        return "TippedArrow".equals(name) ? "Arrow" : name;
    }
}
