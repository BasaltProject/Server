// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util.datafix.fixes;
import com.mojang.datafixers.DataFix;
import com.mojang.datafixers.TypeRewriteRule;
import com.mojang.datafixers.Typed;
import com.mojang.datafixers.schemas.Schema;
public class MergeTerrainChunkStatusFix extends DataFix {
    public MergeTerrainChunkStatusFix(Schema schema) {
        super(schema, false);
    }
    protected TypeRewriteRule makeRule() {
        return org.basaltmc.bukkit.facade.NativeStepRule.of(getClass().getSimpleName(), getVersionKey());
    }
    static record FixedAndStatuses(Typed<?> fixed, String oldStatus, String oldTargetStatus) {
    }
}
