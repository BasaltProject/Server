// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util.datafix.fixes;
import com.mojang.datafixers.DataFix;
import com.mojang.datafixers.TypeRewriteRule;
import com.mojang.datafixers.schemas.Schema;
import com.mojang.serialization.Dynamic;
public class BlockEntityCustomNameToComponentFix extends DataFix {
    public BlockEntityCustomNameToComponentFix(Schema outputSchema) {
        super(outputSchema, true);
    }
    public TypeRewriteRule makeRule() {
        return org.basaltmc.bukkit.facade.NativeStepRule.of(getClass().getSimpleName(), getVersionKey());
    }
    public static <T> Dynamic<T> fixTagCustomName(Dynamic<T> tag) {
        String name = tag.get("CustomName").asString("");
        return name.isEmpty() ? tag.remove("CustomName")
            : tag.set("CustomName", net.minecraft.util.datafix.LegacyComponentDataFixUtils.createPlainTextComponent(tag.getOps(), name));
    }
}
