// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.boss.enderdragon.phases;
import net.minecraft.core.BlockPos;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.world.damagesource.DamageSource;
import net.minecraft.world.entity.boss.enderdragon.EndCrystal;
import net.minecraft.world.entity.player.Player;
import net.minecraft.world.phys.Vec3;
public interface DragonPhaseInstance {
    boolean isSitting();
    void doClientTick();
    void doServerTick(ServerLevel level);
    void onCrystalDestroyed(EndCrystal crystal, BlockPos pos, DamageSource source, Player player);
    void begin();
    void end();
    float getFlySpeed();
    float getTurnSpeed();
    EnderDragonPhase<? extends DragonPhaseInstance> getPhase();
    Vec3 getFlyTargetLocation();
    float onHurt(DamageSource source, float damage);
}
