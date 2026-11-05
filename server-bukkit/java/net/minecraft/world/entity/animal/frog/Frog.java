// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.animal.frog;
import net.minecraft.world.entity.EntityTypes;
import java.util.Optional;
import net.minecraft.core.BlockPos;
import net.minecraft.core.Holder;
import net.minecraft.core.component.DataComponentGetter;
import net.minecraft.core.component.DataComponentType;
import net.minecraft.network.syncher.EntityDataAccessor;
import net.minecraft.network.syncher.SynchedEntityData;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.sounds.SoundEvent;
import net.minecraft.util.RandomSource;
import net.minecraft.world.DifficultyInstance;
import net.minecraft.world.damagesource.DamageSource;
import net.minecraft.world.entity.AgeableMob;
import net.minecraft.world.entity.AnimationState;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.entity.EntitySpawnReason;
import net.minecraft.world.entity.EntityType;
import net.minecraft.world.entity.LivingEntity;
import net.minecraft.world.entity.Mob;
import net.minecraft.world.entity.SpawnGroupData;
import net.minecraft.world.entity.ai.Brain;
import net.minecraft.world.entity.ai.attributes.AttributeSupplier;
import net.minecraft.world.entity.ai.control.LookControl;
import net.minecraft.world.entity.ai.navigation.AmphibiousPathNavigation;
import net.minecraft.world.entity.ai.navigation.PathNavigation;
import net.minecraft.world.entity.animal.Animal;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.level.Level;
import net.minecraft.world.level.LevelAccessor;
import net.minecraft.world.level.ServerLevelAccessor;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.pathfinder.AmphibiousNodeEvaluator;
import net.minecraft.world.level.pathfinder.Node;
import net.minecraft.world.level.pathfinder.PathFinder;
import net.minecraft.world.level.pathfinder.PathType;
import net.minecraft.world.level.pathfinder.PathfindingContext;
import net.minecraft.world.level.storage.ValueInput;
import net.minecraft.world.level.storage.ValueOutput;
import net.minecraft.world.phys.Vec3;
public class Frog extends Animal {
    public final AnimationState jumpAnimationState = null;
    public final AnimationState croakAnimationState = null;
    public final AnimationState tongueAnimationState = null;
    public final AnimationState swimIdleAnimationState = null;
    public Frog(EntityType<? extends Animal> type, Level level) {
        super(type, level);
    }
    protected Brain<Frog> makeBrain(Brain.Packed packedBrain) {
        @SuppressWarnings("unchecked") Brain<Frog> made = (Brain<Frog>) (Brain<?>) super.makeBrain(packedBrain); return made;
    }
    public Brain<Frog> getBrain() {
        @SuppressWarnings("unchecked") Brain<Frog> made = (Brain<Frog>) (Brain<?>) super.getBrain(); return made;
    }
    protected void defineSynchedData(SynchedEntityData.Builder entityData) {
    }
    public void eraseTongueTarget() {
        if (live() instanceof org.bukkit.entity.Frog api) { api.setTongueTarget(null); }
    }
    public Optional<Entity> getTongueTarget() {
        if (live() instanceof org.bukkit.entity.Frog api && api.getTongueTarget() != null) {
            return Optional.of(Entity.fromBukkit(api.getTongueTarget()));
        }
        return Optional.empty();
    }
    public void setTongueTarget(Entity target) {
        if (live() instanceof org.bukkit.entity.Frog api) { api.setTongueTarget(target.live()); }
    }
    public int getMaxHeadYRot() {
        return super.getMaxHeadYRot();
    }
    public Holder<FrogVariant> getVariant() {
        return live() instanceof org.bukkit.entity.Frog api
            ? org.basaltmc.bukkit.facade.VariantHolders.of(net.minecraft.core.registries.Registries.FROG_VARIANT, net.minecraft.world.entity.animal.frog.FrogVariant.class, api.getVariant()) : null;
    }
    public <T> T get(DataComponentType<? extends T> type) {
        return super.get(type);
    }
    protected void applyImplicitComponents(DataComponentGetter components) {
        }
    protected <T> boolean applyImplicitComponent(DataComponentType<T> type, T value) {
        return false;
    }
    protected void addAdditionalSaveData(ValueOutput output) {
    }
    protected void readAdditionalSaveData(ValueInput input) {
    }
    protected void customServerAiStep(ServerLevel level) {
    }
    public void tick() {
    }
    public void onSyncedDataUpdated(EntityDataAccessor<?> accessor) {
    }
    protected void updateWalkAnimation(float distance) {
        }
    public void playEatingSound() {
        playSound(net.minecraft.sounds.SoundEvents.byId("entity.frog.eat"), 2.0F, 1.0F);
    }
    public AgeableMob getBreedOffspring(ServerLevel level, AgeableMob partner) {
        return null;
    }
    protected boolean canBeABaby() {
        return false;
    }
    public SpawnGroupData finalizeSpawn(ServerLevelAccessor level, DifficultyInstance difficulty, EntitySpawnReason spawnReason, SpawnGroupData groupData) {
        return groupData;
    }
    public static AttributeSupplier.Builder createAttributes() {
        return Animal.createAnimalAttributes()
            .add(net.minecraft.world.entity.ai.attributes.Attributes.MOVEMENT_SPEED, 1.0)
            .add(net.minecraft.world.entity.ai.attributes.Attributes.MAX_HEALTH, 10.0)
            .add(net.minecraft.world.entity.ai.attributes.Attributes.ATTACK_DAMAGE, 10.0)
            .add(net.minecraft.world.entity.ai.attributes.Attributes.STEP_HEIGHT, 1.0);
    }
    protected SoundEvent getAmbientSound() {
        return net.minecraft.sounds.SoundEvents.byId("entity.frog.ambient");
    }
    protected SoundEvent getHurtSound(DamageSource source) {
        return net.minecraft.sounds.SoundEvents.byId("entity.frog.hurt");
    }
    protected SoundEvent getDeathSound() {
        return net.minecraft.sounds.SoundEvents.byId("entity.frog.death");
    }
    protected void playStepSound(BlockPos pos, BlockState blockState) {
        playSound(net.minecraft.sounds.SoundEvents.byId("entity.frog.step"), 0.15F, 1.0F);
    }
    protected int calculateFallDamage(double fallDistance, float damageModifier) {
        return 0;
    }
    protected void travelInWater(Vec3 input, double baseGravity, boolean isFalling, double oldY) {
        }
    public static boolean canEat(LivingEntity entity) {
        return entity.getType() != EntityTypes.FROG && entity.getType() != EntityTypes.ARMOR_STAND && entity.isAlive() && entity.getBbWidth() < 1.0F;
    }
    protected PathNavigation createNavigation(Level level) {
        return super.createNavigation(level);
    }
    public boolean isFood(ItemStack itemStack) {
        return live() instanceof org.bukkit.entity.Animals animals && animals.isBreedItem(org.bukkit.craftbukkit.inventory.CraftItemStack.asCraftMirror(itemStack));
    }
    public static boolean checkFrogSpawnRules(EntityType<? extends Animal> type, LevelAccessor level, EntitySpawnReason spawnReason, BlockPos pos, RandomSource random) {
        return level.getBlockState(pos.below()).is(net.minecraft.tags.BlockTags.FROGS_SPAWNABLE_ON) && isBrightEnoughToSpawn(level, pos);
    }
    class FrogLookControl extends LookControl {
        public FrogLookControl(Mob mob) {
            super(mob);
        }
        protected boolean resetXRotOnTick() {
            return false;
        }
    }
    static class FrogPathNavigation extends AmphibiousPathNavigation {
        public FrogPathNavigation(Frog mob, Level level) {
            super(mob, level);
        }
        public boolean canCutCorner(PathType pathType) {
            return pathType != PathType.WATER_BORDER && super.canCutCorner(pathType);
        }
        protected PathFinder createPathFinder(int maxVisitedNodes) {
            nodeEvaluator = new FrogNodeEvaluator(true);
            return new PathFinder(nodeEvaluator, maxVisitedNodes);
        }
    }
    static class FrogNodeEvaluator extends AmphibiousNodeEvaluator {
        public FrogNodeEvaluator(boolean prefersShallowSwimming) {
            super(prefersShallowSwimming);
        }
        public Node getStart() {
            if (!mob.isInWater()) {
                return super.getStart();
            }
            net.minecraft.world.phys.AABB box = mob.getBoundingBox();
            return getStartNode(new BlockPos(net.minecraft.util.Mth.floor(box.minX), net.minecraft.util.Mth.floor(box.minY),
                net.minecraft.util.Mth.floor(box.minZ)));
        }
        public PathType getPathType(PathfindingContext context, int x, int y, int z) {
            return context.getBlockState(new BlockPos(x, y - 1, z)).is(net.minecraft.tags.BlockTags.FROG_PREFER_JUMP_TO) ? PathType.OPEN
                : super.getPathType(context, x, y, z);
        }
    }
}
