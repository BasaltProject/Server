// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util.datafix.fixes;
import com.mojang.datafixers.DataFix;
import com.mojang.datafixers.TypeRewriteRule;
import com.mojang.datafixers.schemas.Schema;
import java.util.function.Function;
public abstract class ItemRenameFix extends DataFix {
    public ItemRenameFix(Schema outputSchema, String name) {
        super(outputSchema, false);
    }
    public TypeRewriteRule makeRule() {
        return org.basaltmc.bukkit.facade.NativeStepRule.of(getClass().getSimpleName(), getVersionKey());
    }
    protected abstract String fixItem(String item);
    public static DataFix create(Schema outputSchema, String name, Function<String, String> fixItem) {
        return new DataFix(outputSchema, false) {
            public TypeRewriteRule makeRule() {
                return org.basaltmc.bukkit.facade.NativeStepRule.of(name, getVersionKey());
            }
        };
    }
}
