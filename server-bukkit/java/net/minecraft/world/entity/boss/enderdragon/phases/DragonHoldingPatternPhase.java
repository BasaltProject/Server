// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.boss.enderdragon.phases;
import net.minecraft.world.entity.boss.enderdragon.EnderDragon;
public class DragonHoldingPatternPhase extends AbstractDragonPhaseInstance {
    public DragonHoldingPatternPhase(EnderDragon dragon) {
        super(dragon);
    }
    public EnderDragonPhase<DragonHoldingPatternPhase> getPhase() {
        return EnderDragonPhase.HOLDING_PATTERN;
    }
}
