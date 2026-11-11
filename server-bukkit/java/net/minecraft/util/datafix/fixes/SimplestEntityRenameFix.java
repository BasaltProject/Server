// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util.datafix.fixes;
import com.mojang.datafixers.DataFix;
import com.mojang.datafixers.TypeRewriteRule;
import com.mojang.datafixers.schemas.Schema;
public abstract class SimplestEntityRenameFix extends DataFix {
    public SimplestEntityRenameFix(String name, Schema outputSchema, boolean changesType) {
        super(outputSchema, changesType);
    }
    public TypeRewriteRule makeRule() {
        return org.basaltmc.bukkit.facade.NativeStepRule.of(getClass().getSimpleName(), getVersionKey());
    }
    protected abstract String rename(String name);
}
