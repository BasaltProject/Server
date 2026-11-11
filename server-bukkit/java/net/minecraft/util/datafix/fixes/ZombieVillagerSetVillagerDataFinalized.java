// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util.datafix.fixes;
import com.mojang.datafixers.Typed;
import com.mojang.datafixers.schemas.Schema;
public class ZombieVillagerSetVillagerDataFinalized extends NamedEntityFix {
    public ZombieVillagerSetVillagerDataFinalized(Schema outputSchema) {
        super(outputSchema, true, "Zombie Villager VillagerDataFinalized default value", net.minecraft.util.datafix.fixes.References.ENTITY, "minecraft:zombie_villager");
    }
    protected Typed<?> fix(Typed<?> entity) {
        return org.basaltmc.bukkit.facade.NativeStep.typed("entity", getClass().getSimpleName(), getVersionKey(), entity);
    }
}
