// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.boss.enderdragon.phases;
import net.minecraft.world.entity.boss.enderdragon.EnderDragon;
public class DragonLandingApproachPhase extends AbstractDragonPhaseInstance {
    public DragonLandingApproachPhase(EnderDragon dragon) {
        super(dragon);
    }
    public EnderDragonPhase<DragonLandingApproachPhase> getPhase() {
        return EnderDragonPhase.LANDING_APPROACH;
    }
}
