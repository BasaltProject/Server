// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util.datafix.fixes;
import com.mojang.datafixers.DataFix;
import com.mojang.datafixers.TypeRewriteRule;
import com.mojang.datafixers.schemas.Schema;
public class ChunkHeightAndBiomeFix extends DataFix {
    public static final String DATAFIXER_CONTEXT_TAG = "__context";
    public static final int BLOCKS_PER_SECTION = 4096;
    public static final String DEFAULT_BIOME = "minecraft:plains";
    public ChunkHeightAndBiomeFix(Schema outputSchema) {
        super(outputSchema, true);
    }
    protected TypeRewriteRule makeRule() {
        return org.basaltmc.bukkit.facade.NativeStepRule.of(getClass().getSimpleName(), getVersionKey());
    }
    public static int ceillog2(int input) {
        return input == 0 ? 0 : 32 - Integer.numberOfLeadingZeros(input - 1);
    }
}
