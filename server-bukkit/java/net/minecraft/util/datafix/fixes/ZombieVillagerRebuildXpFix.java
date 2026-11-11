// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util.datafix.fixes;
import com.mojang.datafixers.Typed;
import com.mojang.datafixers.schemas.Schema;
public class ZombieVillagerRebuildXpFix extends NamedEntityFix {
    public ZombieVillagerRebuildXpFix(Schema outputSchema, boolean changesType) {
        super(outputSchema, changesType, "Zombie Villager XP rebuild", net.minecraft.util.datafix.fixes.References.ENTITY, "minecraft:zombie_villager");
    }
    protected Typed<?> fix(Typed<?> entity) {
        return org.basaltmc.bukkit.facade.NativeStep.typed("entity", getClass().getSimpleName(), getVersionKey(), entity);
    }
}
