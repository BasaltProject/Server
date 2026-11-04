// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity;
import java.util.function.Consumer;
import java.util.function.Supplier;
import net.minecraft.network.syncher.EntityDataAccessor;
import net.minecraft.network.syncher.SynchedEntityData;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.world.DifficultyInstance;
import net.minecraft.world.InteractionHand;
import net.minecraft.world.InteractionResult;
import net.minecraft.world.entity.player.Player;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.level.Level;
import net.minecraft.world.level.ServerLevelAccessor;
import net.minecraft.world.level.storage.ValueInput;
import net.minecraft.world.level.storage.ValueOutput;
public abstract class AgeableMob extends PathfinderMob {
    public static final int BABY_START_AGE = -24000;
    public static final int AGE_LOCK_COOLDOWN_TICKS = 40;
    public static final float AGE_LOCK_DOWNWARDS_MOVING_PARTICLE_Y_OFFSET = 0.2F;
    protected static final int DEFAULT_AGE = 0;
    protected static final int DEFAULT_FORCED_AGE = 0;
    protected int age;
    protected int forcedAge;
    protected int forcedAgeTimer;
    protected int ageLockParticleTimer;
    protected AgeableMob(EntityType<? extends AgeableMob> type, Level level) {
        super(type, level);
    }
    public SpawnGroupData finalizeSpawn(ServerLevelAccessor level, DifficultyInstance difficulty, EntitySpawnReason spawnReason, SpawnGroupData groupData) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.entity.AgeableMob#finalizeSpawn");
    }
    public abstract AgeableMob getBreedOffspring(ServerLevel level, AgeableMob partner);
    protected InteractionResult mobInteract(Player player, InteractionHand hand) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.entity.AgeableMob#mobInteract");
    }
    public static boolean canUseGoldenDandelion(ItemStack itemInHand, boolean isBaby, int cooldown, Mob mob) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.entity.AgeableMob#canUseGoldenDandelion");
    }
    public static void setAgeLocked(Mob mob, Supplier<Boolean> isAgedLocked, Player player, ItemStack itemInHand, Consumer<Mob> setAgeLockData) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.entity.AgeableMob#setAgeLocked");
    }
    protected void defineSynchedData(SynchedEntityData.Builder entityData) {
    }
    public boolean canBreed() {
        return false;
    }
    public int getAge() {
        org.bukkit.entity.Ageable ageable = live() instanceof org.bukkit.entity.Ageable a ? a : null;
        return ageable == null ? 0 : ageable.getAge();
    }
    public void ageUp(int seconds, boolean forced) {
        setAge(getAge() + seconds * 20);
    }
    public void ageUp(int seconds) {
        ageUp(seconds, false);
    }
    public void setAge(int newAge) {
        if (live() instanceof org.bukkit.entity.Ageable ageable) {
            ageable.setAge(newAge);
        }
    }
    protected void setAgeLocked(boolean locked) {
        if (live() instanceof org.bukkit.entity.Ageable ageable) {
            ageable.setAgeLock(locked);
        }
    }
    protected void addAdditionalSaveData(ValueOutput output) {
    }
    protected boolean canBeABaby() {
        return true;
    }
    protected void readAdditionalSaveData(ValueInput input) {
    }
    public void onSyncedDataUpdated(EntityDataAccessor<?> accessor) {
    }
    public void aiStep() {
    }
    public boolean canAgeUp() {
        return !isAgeLocked();
    }
    public static int makeAgeLockedParticle(Level level, Mob mob, int ageLockParticleTimer, boolean isAgeLocked) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.entity.AgeableMob#makeAgeLockedParticle");
    }
    protected void ageBoundaryReached() {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.entity.AgeableMob#ageBoundaryReached");
    }
    protected int getBabyStartAge() {
        return -24000;
    }
    public final boolean isBaby() {
        return getAge() < 0;
    }
    public final void setBaby(boolean baby) {
        setAge(baby ? getBabyStartAge() : 0);
    }
    public static int getSpeedUpSecondsWhenFeeding(int ticksUntilAdult) {
        return (int) ((float) (ticksUntilAdult / 20) * 0.1F);
    }
    public int getForcedAge() {
        return getAge();
    }
    public int getForcedAgeTimer() {
        return 0;
    }
    public boolean isAgeLocked() {
        return live() instanceof org.bukkit.entity.Ageable ageable && ageable.getAgeLock();
    }
    public static class AgeableMobGroupData implements SpawnGroupData {
        public AgeableMobGroupData(boolean shouldSpawnBaby, float babySpawnChance) {
            
        }
        public AgeableMobGroupData(boolean shouldSpawnBaby) {
            
        }
        public AgeableMobGroupData(float babySpawnChance) {
            
        }
        public int getGroupSize() {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.entity.AgeableMob.AgeableMobGroupData#getGroupSize");
        }
        public void increaseGroupSizeByOne() {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.entity.AgeableMob.AgeableMobGroupData#increaseGroupSizeByOne");
        }
        public boolean isShouldSpawnBaby() {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.entity.AgeableMob.AgeableMobGroupData#isShouldSpawnBaby");
        }
        public float getBabySpawnChance() {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.entity.AgeableMob.AgeableMobGroupData#getBabySpawnChance");
        }
    }
}
