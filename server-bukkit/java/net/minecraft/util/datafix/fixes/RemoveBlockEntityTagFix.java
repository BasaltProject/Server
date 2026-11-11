// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util.datafix.fixes;
import com.mojang.datafixers.DataFix;
import com.mojang.datafixers.TypeRewriteRule;
import com.mojang.datafixers.schemas.Schema;
import java.util.Set;
public class RemoveBlockEntityTagFix extends DataFix {
    public RemoveBlockEntityTagFix(Schema outputSchema, Set<String> blockEntityIdsToDrop) {
        super(outputSchema, false);
    }
    public RemoveBlockEntityTagFix(Schema outputSchema, boolean useLegacyDataStructure, Set<String> blockEntityIdsToDrop) {
        super(outputSchema, true);
    }
    public TypeRewriteRule makeRule() {
        return org.basaltmc.bukkit.facade.NativeStepRule.of(getClass().getSimpleName(), getVersionKey());
    }
}
