// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.monster;
import net.minecraft.core.BlockPos;
import net.minecraft.core.Holder;
import net.minecraft.network.syncher.EntityDataAccessor;
import net.minecraft.network.syncher.SynchedEntityData;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.sounds.SoundEvent;
import net.minecraft.util.RandomSource;
import net.minecraft.world.DifficultyInstance;
import net.minecraft.world.InteractionHand;
import net.minecraft.world.InteractionResult;
import net.minecraft.world.damagesource.DamageSource;
import net.minecraft.world.entity.AgeableMob;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.entity.EntityDimensions;
import net.minecraft.world.entity.EntitySpawnReason;
import net.minecraft.world.entity.EntityType;
import net.minecraft.world.entity.EquipmentSlot;
import net.minecraft.world.entity.ItemSteerable;
import net.minecraft.world.entity.LivingEntity;
import net.minecraft.world.entity.Pose;
import net.minecraft.world.entity.SpawnGroupData;
import net.minecraft.world.entity.ai.attributes.AttributeSupplier;
import net.minecraft.world.entity.ai.goal.MoveToBlockGoal;
import net.minecraft.world.entity.ai.navigation.GroundPathNavigation;
import net.minecraft.world.entity.ai.navigation.PathNavigation;
import net.minecraft.world.entity.animal.Animal;
import net.minecraft.world.entity.player.Player;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.item.equipment.Equippable;
import net.minecraft.world.level.Level;
import net.minecraft.world.level.LevelAccessor;
import net.minecraft.world.level.LevelReader;
import net.minecraft.world.level.ServerLevelAccessor;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.material.FluidState;
import net.minecraft.world.level.pathfinder.PathFinder;
import net.minecraft.world.phys.Vec3;
import net.minecraft.world.phys.shapes.VoxelShape;
public class Strider extends Animal implements ItemSteerable {
    public Strider(EntityType<? extends Strider> strider, Level level) {
        super(strider, level);
    }
    public static boolean checkStriderSpawnRules(EntityType<Strider> ignoredType, LevelAccessor level, EntitySpawnReason ignoredSpawnType, BlockPos pos, RandomSource ignoredRandom) {
        return true;
    }
    public void onSyncedDataUpdated(EntityDataAccessor<?> accessor) {
    }
    protected void defineSynchedData(SynchedEntityData.Builder entityData) {
    }
    public boolean canUseSlot(EquipmentSlot slot) {
        return super.canUseSlot(slot);
    }
    protected boolean canDispenserEquipIntoSlot(EquipmentSlot slot) {
        return false;
    }
    protected void registerGoals() {
        goalSelector.addGoal(1, new net.minecraft.world.entity.ai.goal.PanicGoal(this, 1.65));
        goalSelector.addGoal(2, new net.minecraft.world.entity.ai.goal.BreedGoal(this, 1.0));
        goalSelector.addGoal(5, new net.minecraft.world.entity.ai.goal.FollowParentGoal(this, 1.0));
        goalSelector.addGoal(7, new net.minecraft.world.entity.ai.goal.RandomStrollGoal(this, 1.0, 60));
        goalSelector.addGoal(8, new net.minecraft.world.entity.ai.goal.LookAtPlayerGoal(this, net.minecraft.world.entity.player.Player.class, 8.0F));
        goalSelector.addGoal(8, new net.minecraft.world.entity.ai.goal.RandomLookAroundGoal(this));
    }
    public void setSuffocating(boolean flag) {
        if (live() instanceof org.bukkit.entity.Strider api) {
            api.setShivering(flag);
        }
    }
    public boolean isSuffocating() {
        return live() instanceof org.bukkit.entity.Strider api && api.isShivering();
    }
    public EntityDimensions getDefaultDimensions(Pose pose) {
        return net.minecraft.world.entity.EntityDimensions.scalable(getBbWidth(), getBbHeight());
    }
    public boolean checkSpawnObstruction(LevelReader level) {
        return super.checkSpawnObstruction(level);
    }
    protected void tickRidden(Player controller, Vec3 riddenInput) {
        }
    protected float getRiddenSpeed(Player controller) {
        return 0;
    }
    protected float nextStep() {
        return 0;
    }
    protected void playStepSound(BlockPos pos, BlockState blockState) {
        playSound(net.minecraft.sounds.SoundEvents.byId("entity.strider.step"), 0.15F, 1.0F);
    }
    public boolean boost() {
        if (!(live() instanceof org.bukkit.entity.Strider api) || api.getBoostTicks() > 0 && api.getCurrentBoostTicks() < api.getBoostTicks()) {
            return false;
        }
        api.setCurrentBoostTicks(0);
        api.setBoostTicks(random.nextInt(841) + 140);
        return true;
    }
    protected void checkFallDamage(double ya, boolean onGround, BlockState onState, BlockPos pos) {
        }
    public void tick() {
    }
    protected boolean shouldPassengersInheritMalus() {
        return false;
    }
    public static AttributeSupplier.Builder createAttributes() {
        return Animal.createAnimalAttributes()
            .add(net.minecraft.world.entity.ai.attributes.Attributes.MOVEMENT_SPEED, 0.175);
    }
    protected SoundEvent getAmbientSound() {
        return net.minecraft.sounds.SoundEvents.byId("entity.strider.ambient");
    }
    protected SoundEvent getHurtSound(DamageSource source) {
        return net.minecraft.sounds.SoundEvents.byId("entity.strider.hurt");
    }
    protected SoundEvent getDeathSound() {
        return net.minecraft.sounds.SoundEvents.byId("entity.strider.death");
    }
    protected boolean canAddPassenger(Entity passenger) {
        return false;
    }
    public boolean isSensitiveToWater() {
        return super.isSensitiveToWater();
    }
    protected PathNavigation createNavigation(Level level) {
        return super.createNavigation(level);
    }
    public float getWalkTargetValue(BlockPos pos, LevelReader level) {
        return super.getWalkTargetValue(pos, level);
    }
    public Strider getBreedOffspring(ServerLevel level, AgeableMob partner) {
        return null;
    }
    public boolean isFood(ItemStack itemStack) {
        return live() instanceof org.bukkit.entity.Animals animals && animals.isBreedItem(org.bukkit.craftbukkit.inventory.CraftItemStack.asCraftMirror(itemStack));
    }
    public InteractionResult mobInteract(Player player, InteractionHand hand) {
        return net.minecraft.world.InteractionResult.PASS;
    }
    public Vec3 getLeashOffset() {
        return new Vec3(0.0, 0.6F * getEyeHeight(), getBbWidth() * 0.4F);
    }
    public SpawnGroupData finalizeSpawn(ServerLevelAccessor level, DifficultyInstance difficulty, EntitySpawnReason spawnReason, SpawnGroupData groupData) {
        return groupData;
    }
    static class StriderPathNavigation extends GroundPathNavigation {
        public StriderPathNavigation(Strider mob, Level level) {
            super(mob, level);
        }
        protected PathFinder createPathFinder(int maxVisitedNodes) {
            this.nodeEvaluator = new net.minecraft.world.level.pathfinder.WalkNodeEvaluator();
            return new PathFinder(this.nodeEvaluator, maxVisitedNodes);
        }
        public boolean isStableDestination(BlockPos pos) {
            return this.level.getBlockState(pos).is(net.minecraft.world.level.block.Blocks.LAVA) || super.isStableDestination(pos);
        }
    }
    static class StriderGoToLavaGoal extends MoveToBlockGoal {
        private final Strider strider;

        private StriderGoToLavaGoal(Strider strider, double speedModifier) {
            super(strider, speedModifier, 8, 2);
            this.strider = strider;
        }
        public BlockPos getMoveToTarget() {
            return this.blockPos;
        }
        public boolean canContinueToUse() {
            return !strider.isInLava() && isValidTarget(strider.level(), blockPos);
        }
        public boolean canUse() {
            return !strider.isInLava() && super.canUse();
        }
        public boolean shouldRecalculatePath() {
            return this.tryTicks % 20 == 0;
        }
        protected boolean isValidTarget(LevelReader level, BlockPos pos) {
            return level.getBlockState(pos).is(net.minecraft.world.level.block.Blocks.LAVA)
                && level.getBlockState(pos.above()).isPathfindable(net.minecraft.world.level.pathfinder.PathComputationType.LAND);
        }
    }
}
