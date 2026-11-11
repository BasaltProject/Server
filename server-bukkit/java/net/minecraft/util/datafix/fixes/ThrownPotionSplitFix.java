// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util.datafix.fixes;
import com.mojang.datafixers.OpticFinder;
import com.mojang.datafixers.Typed;
import com.mojang.datafixers.schemas.Schema;
import com.mojang.datafixers.util.Pair;
public class ThrownPotionSplitFix extends EntityRenameFix {
    public ThrownPotionSplitFix(Schema outputSchema) {
        super("ThrownPotionSplitFix", outputSchema, true);
    }
    protected Pair<String, Typed<?>> fix(String name, Typed<?> entity) {
        return org.basaltmc.bukkit.facade.NativeStep.entityTyped(name, entity, getClass().getSimpleName(), getVersionKey());
    }
    static record ItemIdFinder(OpticFinder<?> itemFinder, OpticFinder<Pair<String, String>> itemIdFinder) {
        public String getItemId(Typed<?> entity) {
            com.mojang.serialization.Dynamic<?> written = entity.write().getOrThrow();
            com.mojang.serialization.Dynamic<?> item = written.get("Item").orElseEmptyMap();
            return item.get("id").asString("");
        }
    }
}
