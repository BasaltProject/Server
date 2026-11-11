// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util.datafix.fixes;
import com.mojang.datafixers.DataFix;
import com.mojang.datafixers.TypeRewriteRule;
import com.mojang.datafixers.schemas.Schema;
import com.mojang.serialization.Dynamic;
public abstract class DataComponentRemainderFix extends DataFix {
    public DataComponentRemainderFix(Schema outputSchema, String name, String componentId) {
        super(outputSchema, false);
    }
    public DataComponentRemainderFix(Schema outputSchema, String name, String componentId, String newComponentId) {
        super(outputSchema, false);
    }
    public final TypeRewriteRule makeRule() {
        return org.basaltmc.bukkit.facade.NativeStepRule.of(getClass().getSimpleName(), getVersionKey());
    }
    protected abstract <T> Dynamic<T> fixComponent(Dynamic<T> input);
}
