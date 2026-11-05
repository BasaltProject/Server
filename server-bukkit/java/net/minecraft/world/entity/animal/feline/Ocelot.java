// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.animal.feline;
import java.util.function.Predicate;
import net.minecraft.core.BlockPos;
import net.minecraft.network.syncher.SynchedEntityData;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.sounds.SoundEvent;
import net.minecraft.util.RandomSource;
import net.minecraft.world.DifficultyInstance;
import net.minecraft.world.InteractionHand;
import net.minecraft.world.InteractionResult;
import net.minecraft.world.damagesource.DamageSource;
import net.minecraft.world.entity.AgeableMob;
import net.minecraft.world.entity.EntityDimensions;
import net.minecraft.world.entity.EntitySpawnReason;
import net.minecraft.world.entity.EntityType;
import net.minecraft.world.entity.LivingEntity;
import net.minecraft.world.entity.Pose;
import net.minecraft.world.entity.SpawnGroupData;
import net.minecraft.world.entity.ai.attributes.AttributeSupplier;
import net.minecraft.world.entity.ai.goal.AvoidEntityGoal;
import net.minecraft.world.entity.ai.goal.TemptGoal;
import net.minecraft.world.entity.animal.Animal;
import net.minecraft.world.entity.player.Player;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.level.Level;
import net.minecraft.world.level.LevelAccessor;
import net.minecraft.world.level.LevelReader;
import net.minecraft.world.level.ServerLevelAccessor;
import net.minecraft.world.level.storage.ValueInput;
import net.minecraft.world.level.storage.ValueOutput;
import net.minecraft.world.phys.Vec3;
public class Ocelot extends Animal {
    public static final double CROUCH_SPEED_MOD = 0.6;
    public static final double WALK_SPEED_MOD = 0.8;
    public static final double SPRINT_SPEED_MOD = 1.33;
    public Ocelot(EntityType<? extends Ocelot> type, Level level) {
        super(type, level);
    }
    protected void addAdditionalSaveData(ValueOutput output) {
    }
    protected void readAdditionalSaveData(ValueInput input) {
    }
    protected void defineSynchedData(SynchedEntityData.Builder entityData) {
    }
    protected void registerGoals() {
        goalSelector.addGoal(1, new net.minecraft.world.entity.ai.goal.FloatGoal(this));
        goalSelector.addGoal(7, new net.minecraft.world.entity.ai.goal.LeapAtTargetGoal(this, 0.3F));
        goalSelector.addGoal(9, new net.minecraft.world.entity.ai.goal.BreedGoal(this, 0.8));
        goalSelector.addGoal(10, new net.minecraft.world.entity.ai.goal.WaterAvoidingRandomStrollGoal(this, 0.8, 1.0000001E-5F));
        goalSelector.addGoal(11, new net.minecraft.world.entity.ai.goal.LookAtPlayerGoal(this, net.minecraft.world.entity.player.Player.class, 10.0F));
        targetSelector.addGoal(1, new net.minecraft.world.entity.ai.goal.target.NearestAttackableTargetGoal<>(this, net.minecraft.world.entity.animal.chicken.Chicken.class, false));
    }
    public void customServerAiStep(ServerLevel level) {
    }
    public boolean removeWhenFarAway(double distSqr) {
        return super.removeWhenFarAway(distSqr);
    }
    public static AttributeSupplier.Builder createAttributes() {
        return Animal.createAnimalAttributes()
            .add(net.minecraft.world.entity.ai.attributes.Attributes.MAX_HEALTH, 10.0)
            .add(net.minecraft.world.entity.ai.attributes.Attributes.MOVEMENT_SPEED, 0.3)
            .add(net.minecraft.world.entity.ai.attributes.Attributes.ATTACK_DAMAGE, 3.0);
    }
    protected SoundEvent getAmbientSound() {
        return net.minecraft.sounds.SoundEvents.byId("entity.ocelot.ambient");
    }
    public int getAmbientSoundInterval() {
        return super.getAmbientSoundInterval();
    }
    protected SoundEvent getHurtSound(DamageSource source) {
        return net.minecraft.sounds.SoundEvents.byId("entity.ocelot.hurt");
    }
    protected SoundEvent getDeathSound() {
        return net.minecraft.sounds.SoundEvents.byId("entity.ocelot.death");
    }
    public InteractionResult mobInteract(Player player, InteractionHand hand) {
        return net.minecraft.world.InteractionResult.PASS;
    }
    public void handleEntityEvent(byte id) {
    }
    protected void reassessTrustingGoals() {
        }
    public EntityDimensions getDefaultDimensions(Pose pose) {
        return net.minecraft.world.entity.EntityDimensions.scalable(getBbWidth(), getBbHeight());
    }
    public Ocelot getBreedOffspring(ServerLevel level, AgeableMob partner) {
        return null;
    }
    public boolean isFood(ItemStack itemStack) {
        return live() instanceof org.bukkit.entity.Animals animals && animals.isBreedItem(org.bukkit.craftbukkit.inventory.CraftItemStack.asCraftMirror(itemStack));
    }
    public static boolean checkOcelotSpawnRules(EntityType<Ocelot> type, LevelAccessor level, EntitySpawnReason spawnReason, BlockPos pos, RandomSource random) {
        return true;
    }
    public boolean checkSpawnObstruction(LevelReader level) {
        return super.checkSpawnObstruction(level);
    }
    public SpawnGroupData finalizeSpawn(ServerLevelAccessor level, DifficultyInstance difficulty, EntitySpawnReason spawnReason, SpawnGroupData groupData) {
        return groupData;
    }
    public Vec3 getLeashOffset() {
        return new Vec3(0.0, 0.5F * getEyeHeight(), getBbWidth() * 0.4F);
    }
    static class OcelotAvoidEntityGoal<T extends LivingEntity> extends AvoidEntityGoal<T> {
        public OcelotAvoidEntityGoal(Ocelot ocelot, Class<T> avoidClass, float maxDist, double walkSpeedModifier, double sprintSpeedModifier) {
            super(ocelot, avoidClass, maxDist, walkSpeedModifier, sprintSpeedModifier);
        }
    }
    static class OcelotTemptGoal extends TemptGoal {
        public OcelotTemptGoal(Ocelot ocelot, double speedModifier, Predicate<ItemStack> items, boolean canScare) {
            super(ocelot, speedModifier, items, canScare);
        }
        protected boolean canScare() {
            return false;
        }
    }
}
