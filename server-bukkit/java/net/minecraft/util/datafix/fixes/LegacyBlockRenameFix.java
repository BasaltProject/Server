// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util.datafix.fixes;
import com.mojang.datafixers.DataFix;
import com.mojang.datafixers.TypeRewriteRule;
import com.mojang.datafixers.schemas.Schema;
import java.util.function.Function;
public abstract class LegacyBlockRenameFix extends DataFix {
    public LegacyBlockRenameFix(Schema outputSchema, String name) {
        super(outputSchema, false);
    }
    public TypeRewriteRule makeRule() {
        return org.basaltmc.bukkit.facade.NativeStepRule.of(getClass().getSimpleName(), getVersionKey());
    }
    protected abstract String renameBlock(String block);
    public static DataFix create(Schema outputSchema, String name, Function<String, String> renamer) {
        return new DataFix(outputSchema, false) {
            public TypeRewriteRule makeRule() {
                return org.basaltmc.bukkit.facade.NativeStepRule.of(name, getVersionKey());
            }
        };
    }
}
