// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util.datafix.fixes;
import com.mojang.datafixers.Typed;
import com.mojang.datafixers.schemas.Schema;
import com.mojang.serialization.Dynamic;
import java.util.function.Predicate;
public abstract class ItemStackTagRemainderFix extends ItemStackTagFix {
    public ItemStackTagRemainderFix(Schema outputSchema, String name, Predicate<String> idFilter) {
        super(outputSchema, name, idFilter);
    }
    protected abstract <T> Dynamic<T> fixItemStackTag(Dynamic<T> tag);
    protected final Typed<?> fixItemStackTag(Typed<?> tag) {
        return org.basaltmc.bukkit.facade.NativeStep.typed("chunk", getClass().getSimpleName(), getVersionKey(), tag);
    }
}
