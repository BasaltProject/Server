// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util.datafix.fixes;
import com.mojang.datafixers.DataFix;
import com.mojang.datafixers.TypeRewriteRule;
import com.mojang.datafixers.schemas.Schema;
import com.mojang.serialization.Dynamic;
public abstract class LegacyAbstractBlockPropertyFix extends DataFix {
    public LegacyAbstractBlockPropertyFix(Schema outputSchema, String name) {
        super(outputSchema, false);
    }
    protected TypeRewriteRule makeRule() {
        return org.basaltmc.bukkit.facade.NativeStepRule.of(getClass().getSimpleName(), getVersionKey());
    }
    protected abstract boolean shouldFix(String blockId);
    protected abstract <T> Dynamic<T> fixProperties(String blockId, Dynamic<T> properties);
}
