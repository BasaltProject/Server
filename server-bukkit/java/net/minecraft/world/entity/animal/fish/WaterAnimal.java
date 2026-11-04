// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.animal.fish;
import net.minecraft.core.BlockPos;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.util.RandomSource;
import net.minecraft.world.entity.EntitySpawnReason;
import net.minecraft.world.entity.EntityType;
import net.minecraft.world.entity.PathfinderMob;
import net.minecraft.world.level.Level;
import net.minecraft.world.level.LevelAccessor;
import net.minecraft.world.level.LevelReader;
public abstract class WaterAnimal extends PathfinderMob {
    public static final int AMBIENT_SOUND_INTERVAL = 120;
    protected WaterAnimal(EntityType<? extends WaterAnimal> type, Level level) {
        super(type, level);
        setPathfindingMalus(net.minecraft.world.level.pathfinder.PathType.WATER, 0.0F);
    }
    public boolean checkSpawnObstruction(LevelReader level) {
        return level.isUnobstructed(this);
    }
    public int getAmbientSoundInterval() {
        return AMBIENT_SOUND_INTERVAL;
    }
    protected int getBaseExperienceReward(ServerLevel level) {
        return 1 + random.nextInt(3);
    }
    protected void handleAirSupply(ServerLevel level, int preTickAirSupply) {
        if (!isAlive() || isInWater()) {
            setAirSupply(300);
            return;
        }
        setAirSupply(preTickAirSupply - 1);
        if (shouldTakeDrowningDamage()) {
            setAirSupply(0);
            hurtServer(level, damageSources().drown(), 2.0F);
        }
    }
    public void baseTick() {
        int airSupply = getAirSupply();
        super.baseTick();
        if (level() instanceof ServerLevel serverLevel) {
            handleAirSupply(serverLevel, airSupply);
        }
    }
    public boolean isPushedByFluid() {
        return false;
    }
    public boolean canBeLeashed() {
        return false;
    }
    public static boolean checkSurfaceWaterAnimalSpawnRules(EntityType<? extends WaterAnimal> type, LevelAccessor level, EntitySpawnReason spawnReason, BlockPos pos, RandomSource random) {
        return true;
    }
}
