// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.boss.enderdragon.phases;
import net.minecraft.world.entity.boss.enderdragon.EnderDragon;
public class DragonSittingScanningPhase extends AbstractDragonSittingPhase {
    public DragonSittingScanningPhase(EnderDragon dragon) {
        super(dragon);
    }
    public EnderDragonPhase<DragonSittingScanningPhase> getPhase() {
        return EnderDragonPhase.SITTING_SCANNING;
    }
}
