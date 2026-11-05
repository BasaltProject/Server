// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.boss.enderdragon.phases;
import net.minecraft.world.entity.boss.enderdragon.EnderDragon;
public class DragonLandingPhase extends AbstractDragonPhaseInstance {
    public DragonLandingPhase(EnderDragon dragon) {
        super(dragon);
    }
    public EnderDragonPhase<DragonLandingPhase> getPhase() {
        return EnderDragonPhase.LANDING;
    }
}
