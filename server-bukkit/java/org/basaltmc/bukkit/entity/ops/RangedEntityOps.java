package org.basaltmc.bukkit.entity.ops;

import com.destroystokyo.paper.entity.RangedEntity;
import org.basaltmc.bukkit.entity.Cored;
import org.bukkit.entity.LivingEntity;

public interface RangedEntityOps extends RangedEntity, Cored {
    default void rangedAttack(LivingEntity target, float charge) {
        org.bukkit.Location at = target.getLocation();
        if (!core().test("ranged_attack", at.getX(), at.getY(), at.getZ(), (double) charge)) {
            throw org.basaltmc.bukkit.Unsupported.hit("RangedEntity#rangedAttack for " + core().keyName());
        }
    }

    default void setChargingAttack(boolean raiseHands) {
        core().set("aggressive", raiseHands);
    }
}
