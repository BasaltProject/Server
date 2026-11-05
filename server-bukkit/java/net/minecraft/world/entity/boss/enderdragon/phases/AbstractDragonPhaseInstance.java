// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.boss.enderdragon.phases;
import net.minecraft.core.BlockPos;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.world.damagesource.DamageSource;
import net.minecraft.world.entity.boss.enderdragon.EndCrystal;
import net.minecraft.world.entity.boss.enderdragon.EnderDragon;
import net.minecraft.world.entity.player.Player;
import net.minecraft.world.phys.Vec3;
public abstract class AbstractDragonPhaseInstance implements DragonPhaseInstance {
    protected final EnderDragon dragon;
    public AbstractDragonPhaseInstance(EnderDragon dragon) {
        this.dragon = dragon;
    }
    public boolean isSitting() {
        return false;
    }
    public void doClientTick() {
    }
    public void doServerTick(ServerLevel level) {
    }
    public void onCrystalDestroyed(EndCrystal crystal, BlockPos pos, DamageSource source, Player player) {
    }
    public void begin() {
    }
    public void end() {
    }
    public float getFlySpeed() {
        return 0.6F;
    }
    public Vec3 getFlyTargetLocation() {
        return null;
    }
    public float onHurt(DamageSource source, float damage) {
        return damage;
    }
    public float getTurnSpeed() {
        float horizontal = (float) dragon.getDeltaMovement().horizontalDistance() + 1.0F;
        float limited = Math.min(horizontal, 40.0F);
        return 0.7F / limited / horizontal;
    }
}
