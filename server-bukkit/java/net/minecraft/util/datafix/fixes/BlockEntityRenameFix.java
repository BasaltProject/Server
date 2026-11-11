// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util.datafix.fixes;
import com.mojang.datafixers.DataFix;
import com.mojang.datafixers.TypeRewriteRule;
import com.mojang.datafixers.schemas.Schema;
import java.util.function.UnaryOperator;
public class BlockEntityRenameFix extends DataFix {
    private BlockEntityRenameFix(Schema outputSchema, String name, UnaryOperator<String> nameChangeLookup) {
        super(outputSchema, true);
    }
    public TypeRewriteRule makeRule() {
        return org.basaltmc.bukkit.facade.NativeStepRule.of(getClass().getSimpleName(), getVersionKey());
    }
    public static DataFix create(Schema outputSchema, String name, UnaryOperator<String> nameChangeLookup) {
        return new DataFix(outputSchema, true) {
            public TypeRewriteRule makeRule() {
                return org.basaltmc.bukkit.facade.NativeStepRule.of(name, getVersionKey());
            }
        };
    }
}
