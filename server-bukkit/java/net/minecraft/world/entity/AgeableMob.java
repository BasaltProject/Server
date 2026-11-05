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
import net.minecraft.world.entity.vehicle.boat.AbstractBoat;
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
                AgeableMobGroupData group = groupData instanceof AgeableMobGroupData existing ? existing : new AgeableMobGroupData(true);
        boolean rollBaby = group.isShouldSpawnBaby() && group.getGroupSize() > 0;
        if (rollBaby && level.getRandom().nextFloat() <= group.getBabySpawnChance()) {
            setAge(getBabyStartAge());
        }
        group.increaseGroupSizeByOne();
        return super.finalizeSpawn(level, difficulty, spawnReason, group);
    }
    public abstract AgeableMob getBreedOffspring(ServerLevel level, AgeableMob partner);
    protected InteractionResult mobInteract(Player player, InteractionHand hand) {
                ItemStack held = player.getItemInHand(hand);
        if (!canUseGoldenDandelion(held, isBaby(), ageLockParticleTimer, this)) {
            return super.mobInteract(player, hand);
        }
        setAgeLocked(this, this::isAgeLocked, player, held, mob -> {
            setAgeLocked(!isAgeLocked());
            setAge(getBabyStartAge());
            ageLockParticleTimer = AGE_LOCK_COOLDOWN_TICKS;
        });
        return InteractionResult.SUCCESS;
    }
    public static boolean canUseGoldenDandelion(ItemStack itemInHand, boolean isBaby, int cooldown, Mob mob) {
                boolean fresh = isBaby && cooldown == 0;
        return fresh && itemInHand.getItem() == net.minecraft.world.item.Items.GOLDEN_DANDELION
            && !mob.is(net.minecraft.tags.EntityTypeTags.CANNOT_BE_AGE_LOCKED);
    }
    public static void setAgeLocked(Mob mob, Supplier<Boolean> isAgedLocked, Player player, ItemStack itemInHand, Consumer<Mob> setAgeLockData) {
                setAgeLockData.accept(mob);
        itemInHand.consume(1, player);
        boolean locked = isAgedLocked.get();
        if (locked) {
            mob.setPersistenceRequired();
        }
        net.minecraft.sounds.SoundEvent sound = locked ? net.minecraft.sounds.SoundEvents.GOLDEN_DANDELION_USE
            : net.minecraft.sounds.SoundEvents.GOLDEN_DANDELION_UNUSE;
        mob.level().playSound(null, mob.blockPosition(), sound, net.minecraft.sounds.SoundSource.PLAYERS, 1.0F, 1.0F);
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
        if (ageLockParticleTimer <= 0) {
            return ageLockParticleTimer;
        }
        if (ageLockParticleTimer % 2 == 0 && level instanceof ServerLevel server) {
            double lift = (isAgeLocked ? 0.2F : 0.0F) + mob.getBbHeight();
            server.sendParticles(isAgeLocked ? net.minecraft.core.particles.ParticleTypes.PAUSE_MOB_GROWTH
                : net.minecraft.core.particles.ParticleTypes.RESET_MOB_GROWTH, mob.getRandomX(1.0), mob.getRandomY(0.2) + lift,
                mob.getRandomZ(1.0), 1, 0.0, 0.0, 0.0, 0.0);
        }
        return ageLockParticleTimer - 1;
    }
    protected void ageBoundaryReached() {
                if (!isBaby() && isPassenger() && getVehicle() instanceof AbstractBoat boat && !boat.hasEnoughSpaceFor(this)) {
            stopRiding();
        }
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
        private int groupSize;
        private final boolean shouldSpawnBaby;
        private final float babySpawnChance;
        public AgeableMobGroupData(boolean shouldSpawnBaby, float babySpawnChance) {
            this.shouldSpawnBaby = shouldSpawnBaby;
            this.babySpawnChance = babySpawnChance;
        }
        public AgeableMobGroupData(boolean shouldSpawnBaby) {
            this(shouldSpawnBaby, 0.05F);
        }
        public AgeableMobGroupData(float babySpawnChance) {
            this(true, babySpawnChance);
        }
        public int getGroupSize() {
            return groupSize;
        }
        public void increaseGroupSizeByOne() {
            groupSize++;
        }
        public boolean isShouldSpawnBaby() {
            return shouldSpawnBaby;
        }
        public float getBabySpawnChance() {
            return babySpawnChance;
        }
    }
}
