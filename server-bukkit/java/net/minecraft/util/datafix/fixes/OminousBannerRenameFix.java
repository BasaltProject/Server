// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util.datafix.fixes;
import com.mojang.datafixers.Typed;
import com.mojang.datafixers.schemas.Schema;
public class OminousBannerRenameFix extends ItemStackTagFix {
    public OminousBannerRenameFix(Schema outputSchema) {
        super(outputSchema, "OminousBannerRenameFix", (java.util.function.Predicate) null);
    }
    protected Typed<?> fixItemStackTag(Typed<?> tag) {
        return org.basaltmc.bukkit.facade.NativeStep.typed("item_stack", getClass().getSimpleName(), getVersionKey(), tag);
    }
}
