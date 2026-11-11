// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util.datafix.fixes;
import com.mojang.datafixers.Typed;
import com.mojang.datafixers.schemas.Schema;
public class VillagerSetVillagerDataFinalized extends NamedEntityFix {
    public VillagerSetVillagerDataFinalized(Schema outputSchema) {
        super(outputSchema, true, "Villager VillagerDataFinalized default value", net.minecraft.util.datafix.fixes.References.ENTITY, "minecraft:villager");
    }
    protected Typed<?> fix(Typed<?> entity) {
        return org.basaltmc.bukkit.facade.NativeStep.typed("entity", getClass().getSimpleName(), getVersionKey(), entity);
    }
}
