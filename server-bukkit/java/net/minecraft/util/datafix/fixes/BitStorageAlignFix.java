// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util.datafix.fixes;
import com.mojang.datafixers.DataFix;
import com.mojang.datafixers.TypeRewriteRule;
import com.mojang.datafixers.schemas.Schema;
public class BitStorageAlignFix extends DataFix {
    public BitStorageAlignFix(Schema schema) {
        super(schema, false);
    }
    protected TypeRewriteRule makeRule() {
        return org.basaltmc.bukkit.facade.NativeStepRule.of(getClass().getSimpleName(), getVersionKey());
    }
    public static long[] addPadding(int size, int bits, long[] data) {
        if (data.length == 0) {
            return data;
        }
        int perLong = 64 / bits;
        long mask = (1L << bits) - 1L;
        long[] out = new long[(size + perLong - 1) / perLong];
        for (int i = 0; i < size; i++) {
            long at = (long) i * bits;
            int word = (int) (at >> 6);
            int shift = (int) (at & 63);
            long value = word < data.length ? data[word] >>> shift : 0L;
            if (shift + bits > 64 && word + 1 < data.length) {
                value |= data[word + 1] << (64 - shift);
            }
            out[i / perLong] |= (value & mask) << (i % perLong * bits);
        }
        return out;
    }
}
