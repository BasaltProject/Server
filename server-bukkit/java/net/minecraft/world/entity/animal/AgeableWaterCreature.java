// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.animal;
import net.minecraft.core.BlockPos;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.util.RandomSource;
import net.minecraft.world.entity.AgeableMob;
import net.minecraft.world.entity.EntitySpawnReason;
import net.minecraft.world.entity.EntityType;
import net.minecraft.world.level.Level;
import net.minecraft.world.level.LevelAccessor;
import net.minecraft.world.level.LevelReader;
public abstract class AgeableWaterCreature extends AgeableMob {
    protected AgeableWaterCreature(EntityType<? extends AgeableWaterCreature> type, Level level) {
        super(type, level);
    }
    public boolean checkSpawnObstruction(LevelReader level) {
        return level.isUnobstructed(this);
    }
    public int getAmbientSoundInterval() {
        return 120;
    }
    public int getBaseExperienceReward(ServerLevel level) {
        return 1 + random.nextInt(3);
    }
    protected void handleAirSupply(int preTickAirSupply) {
        if (!isAlive() || isInWater()) {
            setAirSupply(300);
            return;
        }
        setAirSupply(preTickAirSupply - 1);
        if (shouldTakeDrowningDamage()) {
            setAirSupply(0);
            hurt(damageSources().drown(), 2.0F);
        }
    }
    public void baseTick() {
        int airSupply = getAirSupply();
        super.baseTick();
        handleAirSupply(airSupply);
    }
    public boolean isPushedByFluid() {
        return false;
    }
    public boolean canBeLeashed() {
        return false;
    }
    public static boolean checkSurfaceAgeableWaterCreatureSpawnRules(EntityType<? extends AgeableWaterCreature> type, LevelAccessor level, EntitySpawnReason spawnReason, BlockPos pos, RandomSource random) {
        return true;
    }
}
