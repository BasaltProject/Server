// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util.datafix.fixes;
import com.mojang.datafixers.Typed;
import com.mojang.datafixers.schemas.Schema;
public class VillagerSetCanPickUpLootFix extends NamedEntityFix {
    public VillagerSetCanPickUpLootFix(Schema outputSchema) {
        super(outputSchema, true, "Villager CanPickUpLoot default value", net.minecraft.util.datafix.fixes.References.ENTITY, "Villager");
    }
    protected Typed<?> fix(Typed<?> entity) {
        return org.basaltmc.bukkit.facade.NativeStep.typed("entity", getClass().getSimpleName(), getVersionKey(), entity);
    }
}
