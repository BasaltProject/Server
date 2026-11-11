// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util.datafix.fixes;
import com.mojang.datafixers.DataFix;
import com.mojang.datafixers.TypeRewriteRule;
import com.mojang.datafixers.schemas.Schema;
import com.mojang.serialization.Dynamic;
import java.util.List;
public class ChunkProtoTickListFix extends DataFix {
    public ChunkProtoTickListFix(Schema outputSchema) {
        super(outputSchema, false);
    }
    protected TypeRewriteRule makeRule() {
        return org.basaltmc.bukkit.facade.NativeStepRule.of(getClass().getSimpleName(), getVersionKey());
    }
    public static final class PoorMansPalettedContainer {
        private final List<? extends Dynamic<?>> palette;
        private final long[] data;
        private final int bits;
        public PoorMansPalettedContainer(List<? extends Dynamic<?>> palette, long[] data) {
            this.palette = palette;
            this.data = data;
            this.bits = Math.max(4, 32 - Integer.numberOfLeadingZeros(Math.max(palette.size() - 1, 0)));
        }
        public Dynamic<?> get(int x, int y, int z) {
            if (data.length == 0) {
            return palette.get(0);
        }
        int index = (y << 8) | (z << 4) | x;
        int perLong = 64 / bits;
        long entry = data[index / perLong] >>> (index % perLong * bits);
        return palette.get((int) (entry & ((1L << bits) - 1L)));
        }
        public List<? extends Dynamic<?>> palette() {
            return palette;
        }
        public long[] data() {
            return data;
        }
    }
}
