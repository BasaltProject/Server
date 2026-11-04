// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.monster.zombie;
import net.minecraft.core.BlockPos;
import net.minecraft.network.syncher.EntityDataAccessor;
import net.minecraft.network.syncher.SynchedEntityData;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.sounds.SoundEvent;
import net.minecraft.util.RandomSource;
import net.minecraft.world.DifficultyInstance;
import net.minecraft.world.damagesource.DamageSource;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.entity.EntityDimensions;
import net.minecraft.world.entity.EntitySpawnReason;
import net.minecraft.world.entity.EntityType;
import net.minecraft.world.entity.LivingEntity;
import net.minecraft.world.entity.Mob;
import net.minecraft.world.entity.PathfinderMob;
import net.minecraft.world.entity.Pose;
import net.minecraft.world.entity.SpawnGroupData;
import net.minecraft.world.entity.ai.attributes.AttributeSupplier;
import net.minecraft.world.entity.ai.goal.RemoveBlockGoal;
import net.minecraft.world.entity.monster.Monster;
import net.minecraft.world.entity.npc.villager.Villager;
import net.minecraft.world.entity.player.Player;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.level.Level;
import net.minecraft.world.level.LevelAccessor;
import net.minecraft.world.level.ServerLevelAccessor;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.storage.ValueInput;
import net.minecraft.world.level.storage.ValueOutput;
public class Zombie extends Monster {
    public static final float ZOMBIE_LEADER_CHANCE = 0.05F;
    public static final int REINFORCEMENT_ATTEMPTS = 50;
    public static final int REINFORCEMENT_RANGE_MAX = 40;
    public static final int REINFORCEMENT_RANGE_MIN = 7;
    public Zombie(EntityType<? extends Zombie> type, Level level) {
        super(type, level);
    }
    public Zombie(Level level) {
        super((net.minecraft.world.entity.EntityType) null, (net.minecraft.world.level.Level) null);
    }
    protected void registerGoals() {
        goalSelector.addGoal(8, new net.minecraft.world.entity.ai.goal.LookAtPlayerGoal(this, net.minecraft.world.entity.player.Player.class, 8.0F));
        goalSelector.addGoal(8, new net.minecraft.world.entity.ai.goal.RandomLookAroundGoal(this));
        goalSelector.addGoal(3, new net.minecraft.world.entity.ai.goal.ZombieAttackGoal(this, 1.0, false));
        goalSelector.addGoal(7, new net.minecraft.world.entity.ai.goal.WaterAvoidingRandomStrollGoal(this, 1.0));
        targetSelector.addGoal(1, new net.minecraft.world.entity.ai.goal.target.HurtByTargetGoal(this));
        targetSelector.addGoal(2, new net.minecraft.world.entity.ai.goal.target.NearestAttackableTargetGoal<>(this, net.minecraft.world.entity.player.Player.class, true));
        targetSelector.addGoal(3, new net.minecraft.world.entity.ai.goal.target.NearestAttackableTargetGoal<>(this, net.minecraft.world.entity.npc.villager.AbstractVillager.class, false));
        targetSelector.addGoal(3, new net.minecraft.world.entity.ai.goal.target.NearestAttackableTargetGoal<>(this, net.minecraft.world.entity.animal.golem.IronGolem.class, true));
    }
    protected void addBehaviourGoals() {
        }
    public static AttributeSupplier.Builder createAttributes() {
        return Monster.createMonsterAttributes()
            .add(net.minecraft.world.entity.ai.attributes.Attributes.FOLLOW_RANGE, 35.0)
            .add(net.minecraft.world.entity.ai.attributes.Attributes.MOVEMENT_SPEED, 0.23)
            .add(net.minecraft.world.entity.ai.attributes.Attributes.ATTACK_DAMAGE, 3.0)
            .add(net.minecraft.world.entity.ai.attributes.Attributes.ARMOR, 2.0)
            .add(net.minecraft.world.entity.ai.attributes.Attributes.SPAWN_REINFORCEMENTS_CHANCE, 0.0);
    }
    protected void defineSynchedData(SynchedEntityData.Builder entityData) {
    }
    public boolean isUnderWaterConverting() {
        return live() instanceof org.bukkit.entity.Zombie api && api.isConverting();
    }
    public boolean canBreakDoors() {
        return live() instanceof org.bukkit.entity.Zombie api ? (boolean) api.canBreakDoors() : false;
    }
    public void setCanBreakDoors(boolean canBreakDoors) {
        if (live() instanceof org.bukkit.entity.Zombie api) { api.setCanBreakDoors(canBreakDoors); }
    }
    public boolean isBaby() {
        return live() instanceof org.bukkit.entity.Zombie api ? (boolean) api.isBaby() : false;
    }
    protected int getBaseExperienceReward(ServerLevel level) {
        return super.getBaseExperienceReward(level);
    }
    public void setBaby(boolean baby) {
        if (live() instanceof org.bukkit.entity.Zombie api) { api.setBaby(baby); }
    }
    public void onSyncedDataUpdated(EntityDataAccessor<?> accessor) {
    }
    protected boolean convertsInWater() {
        return false;
    }
    public void tick() {
    }
    private int inWaterTime;
    public boolean convertVillagerToZombieVillager(ServerLevel level, Villager villager) {
        ZombieVillager converted = villager.convertTo(net.minecraft.world.entity.EntityTypes.ZOMBIE_VILLAGER, net.minecraft.world.entity.ConversionParams.single(villager, true, true), zombie -> {
            zombie.setVillagerData(villager.getVillagerData());
            zombie.setVillagerXp(villager.getVillagerXp());
            zombie.setTradeOffers(villager.getOffers().copy());
        });
        return converted != null;
    }
    protected boolean isSunSensitive() {
        return false;
    }
    public boolean hurtServer(ServerLevel level, DamageSource source, float damage) {
        return super.hurtServer(level, source, damage);
    }
    public boolean doHurtTarget(ServerLevel level, Entity target) {
        return super.doHurtTarget(level, target);
    }
    protected SoundEvent getAmbientSound() {
        return net.minecraft.sounds.SoundEvents.byId("entity.zombie.ambient");
    }
    protected SoundEvent getHurtSound(DamageSource source) {
        return net.minecraft.sounds.SoundEvents.byId("entity.zombie.hurt");
    }
    protected SoundEvent getDeathSound() {
        return net.minecraft.sounds.SoundEvents.byId("entity.zombie.death");
    }
    protected SoundEvent getStepSound() {
        return net.minecraft.sounds.SoundEvents.ZOMBIE_STEP;
    }
    protected int getConversionSound() {
        return 0;
    }
    protected EntityType<? extends Zombie> convertsToWhenDrowning() {
        return net.minecraft.world.entity.EntityTypes.DROWNED;
    }
    protected void playStepSound(BlockPos pos, BlockState blockState) {
        playSound(net.minecraft.sounds.SoundEvents.byId("entity.zombie.step"), 0.15F, 1.0F);
    }
    public EntityType<? extends Zombie> getType() {
        return super.getType();
    }
    protected boolean canSpawnInLiquids() {
        return false;
    }
    protected void populateDefaultEquipmentSlots(RandomSource random, DifficultyInstance difficulty) {
        }
    protected void addAdditionalSaveData(ValueOutput output) {
    }
    protected void readAdditionalSaveData(ValueInput input) {
    }
    public EntityDimensions getDefaultDimensions(Pose pose) {
        return net.minecraft.world.entity.EntityDimensions.scalable(getBbWidth(), getBbHeight());
    }
    public SpawnGroupData finalizeSpawn(ServerLevelAccessor level, DifficultyInstance difficulty, EntitySpawnReason spawnReason, SpawnGroupData groupData) {
        return groupData;
    }
    protected void onOffspringSpawnedFromEgg(Player spawner, Mob offspring) {
        }
    public void setInWaterTime(int inWaterTime) {
        this.inWaterTime = inWaterTime;
    }
    public void setConversionTime(int conversionTime) {
        if (live() instanceof org.bukkit.entity.Zombie api) { api.setConversionTime(conversionTime); }
    }
    public static boolean getSpawnAsBabyOdds(RandomSource random) {
        return random.nextFloat() < 0.05F;
    }
    protected void handleAttributes(float difficultyModifier, EntitySpawnReason spawnReason) {
        }
    protected void randomizeReinforcementsChance() {
        }
    public static class ZombieGroupData implements SpawnGroupData {
        public final boolean isBaby = false;
        public final boolean canSpawnJockey = false;
        public ZombieGroupData(boolean baby, boolean canSpawnJockey) {
            
        }
    }
    class ZombieAttackTurtleEggGoal extends RemoveBlockGoal {
        public ZombieAttackTurtleEggGoal(PathfinderMob mob, double speedModifier, int verticalSearchRange) {
            super(net.minecraft.world.level.block.Blocks.TURTLE_EGG, mob, speedModifier, verticalSearchRange);
        }
        public void playDestroyProgressSound(LevelAccessor level, BlockPos pos) {
            ((org.bukkit.entity.Entity) Zombie.this.getBukkitEntity()).getWorld().playSound(soundAt(pos), org.bukkit.Sound.ENTITY_ZOMBIE_DESTROY_EGG, 0.5F, 0.9F);
        }
        public void playBreakSound(Level level, BlockPos pos) {
            ((org.bukkit.entity.Entity) Zombie.this.getBukkitEntity()).getWorld().playSound(soundAt(pos), org.bukkit.Sound.ENTITY_TURTLE_EGG_BREAK, 0.7F, 0.9F);
        }
        private org.bukkit.Location soundAt(BlockPos pos) {
            return new org.bukkit.Location(((org.bukkit.entity.Entity) Zombie.this.getBukkitEntity()).getWorld(), pos.getX() + 0.5, pos.getY(), pos.getZ() + 0.5);
        }
        public double acceptedDistance() {
            return 1.14;
        }
    }
}
