// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util.datafix.fixes;
import com.mojang.datafixers.schemas.Schema;
import com.mojang.serialization.Dynamic;
public class GoatHornIdFix extends ItemStackTagRemainderFix {
    public GoatHornIdFix(Schema outputSchema) {
        super(outputSchema, "GoatHornIdFix", (java.util.function.Predicate) null);
    }
    protected <T> Dynamic<T> fixItemStackTag(Dynamic<T> tag) {
        return org.basaltmc.bukkit.facade.NativeStep.dynamic("chunk", getClass().getSimpleName(), getVersionKey(), tag);
    }
}
