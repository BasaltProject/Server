// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util.datafix.fixes;
import com.mojang.datafixers.DataFix;
import com.mojang.datafixers.TypeRewriteRule;
import com.mojang.datafixers.schemas.Schema;
public class VillagerRebuildLevelAndXpFix extends DataFix {
    public static int getMinXpPerLevel(int level) {
        return new int[] {0, 10, 50, 100, 150}[Math.max(0, Math.min(level - 1, 4))];
    }
    public VillagerRebuildLevelAndXpFix(Schema outputSchema, boolean changesType) {
        super(outputSchema, changesType);
    }
    public TypeRewriteRule makeRule() {
        return org.basaltmc.bukkit.facade.NativeStepRule.of(getClass().getSimpleName(), getVersionKey());
    }
}
