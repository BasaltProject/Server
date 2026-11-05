// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.animal.turtle;
import net.minecraft.world.level.block.Blocks;
import net.minecraft.tags.BlockTags;
import net.minecraft.world.entity.ai.util.DefaultRandomPos;
import net.minecraft.core.BlockPos;
import net.minecraft.network.syncher.SynchedEntityData;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.sounds.SoundEvent;
import net.minecraft.util.RandomSource;
import net.minecraft.world.DifficultyInstance;
import net.minecraft.world.damagesource.DamageSource;
import net.minecraft.world.entity.AgeableMob;
import net.minecraft.world.entity.EntityDimensions;
import net.minecraft.world.entity.EntitySpawnReason;
import net.minecraft.world.entity.EntityType;
import net.minecraft.world.entity.LightningBolt;
import net.minecraft.world.entity.Pose;
import net.minecraft.world.entity.SpawnGroupData;
import net.minecraft.world.entity.ai.attributes.AttributeSupplier;
import net.minecraft.world.entity.ai.control.MoveControl;
import net.minecraft.world.entity.ai.goal.BreedGoal;
import net.minecraft.world.entity.ai.goal.Goal;
import net.minecraft.world.entity.ai.goal.MoveToBlockGoal;
import net.minecraft.world.entity.ai.goal.PanicGoal;
import net.minecraft.world.entity.ai.goal.RandomStrollGoal;
import net.minecraft.world.entity.ai.navigation.AmphibiousPathNavigation;
import net.minecraft.world.entity.ai.navigation.PathNavigation;
import net.minecraft.world.entity.ai.targeting.TargetingConditions;
import net.minecraft.world.entity.animal.Animal;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.level.Level;
import net.minecraft.world.level.LevelAccessor;
import net.minecraft.world.level.LevelReader;
import net.minecraft.world.level.ServerLevelAccessor;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.storage.ValueInput;
import net.minecraft.world.level.storage.ValueOutput;
import net.minecraft.world.phys.Vec3;
public class Turtle extends Animal {
    public static final TargetingConditions.Selector BABY_ON_LAND_SELECTOR = null;
    public Turtle(EntityType<? extends Turtle> type, Level level) {
        super(type, level);
    }
    private BlockPos homePos = BlockPos.ZERO;
    private BlockPos travelPos = BlockPos.ZERO;
    private boolean travelling;
    private boolean goingHome;
    boolean isGoingHome() {
        return goingHome;
    }
    void setGoingHome(boolean value) {
        goingHome = value;
    }
    void setTravelling(boolean value) {
        travelling = value;
    }
    public void setHomePos(BlockPos pos) {
        this.homePos = pos;
    }
    public BlockPos getHomePos() {
        return homePos;
    }
    public boolean hasEgg() {
        return live() instanceof org.bukkit.entity.Turtle api ? (boolean) api.hasEgg() : false;
    }
    public boolean isLayingEgg() {
        return live() instanceof org.bukkit.entity.Turtle api ? (boolean) api.isLayingEgg() : false;
    }
    protected void defineSynchedData(SynchedEntityData.Builder entityData) {
    }
    protected void addAdditionalSaveData(ValueOutput output) {
    }
    protected void readAdditionalSaveData(ValueInput input) {
    }
    public SpawnGroupData finalizeSpawn(ServerLevelAccessor level, DifficultyInstance difficulty, EntitySpawnReason spawnReason, SpawnGroupData groupData) {
        return groupData;
    }
    public static boolean checkTurtleSpawnRules(EntityType<Turtle> type, LevelAccessor level, EntitySpawnReason spawnReason, BlockPos pos, RandomSource random) {
        return true;
    }
    protected void registerGoals() {
        goalSelector.addGoal(2, new net.minecraft.world.entity.ai.goal.TemptGoal(this, 1.1, stack -> isFood(stack), false));
        goalSelector.addGoal(8, new net.minecraft.world.entity.ai.goal.LookAtPlayerGoal(this, net.minecraft.world.entity.player.Player.class, 8.0F));
    }
    public static AttributeSupplier.Builder createAttributes() {
        return Animal.createAnimalAttributes()
            .add(net.minecraft.world.entity.ai.attributes.Attributes.MAX_HEALTH, 30.0)
            .add(net.minecraft.world.entity.ai.attributes.Attributes.MOVEMENT_SPEED, 0.25)
            .add(net.minecraft.world.entity.ai.attributes.Attributes.STEP_HEIGHT, 1.0);
    }
    public boolean isPushedByFluid() {
        return false;
    }
    public int getAmbientSoundInterval() {
        return super.getAmbientSoundInterval();
    }
    protected SoundEvent getAmbientSound() {
        return null;
    }
    protected void playSwimSound(float volume) {
    }
    protected SoundEvent getSwimSound() {
        return null;
    }
    protected SoundEvent getHurtSound(DamageSource source) {
        return net.minecraft.sounds.SoundEvents.byId("entity.turtle.hurt");
    }
    protected SoundEvent getDeathSound() {
        return net.minecraft.sounds.SoundEvents.byId("entity.turtle.death");
    }
    protected void playStepSound(BlockPos pos, BlockState blockState) {
    }
    public boolean canFallInLove() {
        return super.canFallInLove() && !hasEgg();
    }
    protected float nextStep() {
        return 0;
    }
    public float getAgeScale() {
        return isBaby() ? 0.3F : 1.0F;
    }
    protected PathNavigation createNavigation(Level level) {
        return super.createNavigation(level);
    }
    public AgeableMob getBreedOffspring(ServerLevel level, AgeableMob partner) {
        return null;
    }
    public boolean isFood(ItemStack itemStack) {
        return live() instanceof org.bukkit.entity.Animals animals && animals.isBreedItem(org.bukkit.craftbukkit.inventory.CraftItemStack.asCraftMirror(itemStack));
    }
    public float getWalkTargetValue(BlockPos pos, LevelReader level) {
        return super.getWalkTargetValue(pos, level);
    }
    public void aiStep() {
    }
    protected void ageBoundaryReached() {
        }
    protected void travelInWater(Vec3 input, double baseGravity, boolean isFalling, double oldY) {
        }
    public boolean canBeLeashed() {
        return super.canBeLeashed();
    }
    public void thunderHit(ServerLevel level, LightningBolt lightningBolt) {
        super.thunderHit(level, lightningBolt);
    }
    public EntityDimensions getDefaultDimensions(Pose pose) {
        return net.minecraft.world.entity.EntityDimensions.scalable(getBbWidth(), getBbHeight());
    }
    static class TurtlePanicGoal extends PanicGoal {
        public TurtlePanicGoal(Turtle turtle, double speedModifier) {
            super(turtle, speedModifier);
        }
    }
    static class TurtleTravelGoal extends Goal {
        private final Turtle turtle;
        private final double speedModifier;
        private boolean stuck;
        public TurtleTravelGoal(Turtle turtle, double speedModifier) {
            this.turtle = turtle;
            this.speedModifier = speedModifier;
        }
        public boolean canUse() {
            return !turtle.isGoingHome() && !turtle.hasEgg() && turtle.isInWater();
        }
        public void start() {
            RandomSource random = turtle.getRandom();
            int dy = random.nextInt(9) - 4;
            if (dy + turtle.getBlockY() > ((org.bukkit.entity.Entity) turtle.getBukkitEntity()).getWorld().getSeaLevel() - 1) {
                dy = 0;
            }
            turtle.travelPos = new BlockPos(random.nextInt(1025) - 512 + turtle.getBlockX(), dy + turtle.getBlockY(),
                random.nextInt(1025) - 512 + turtle.getBlockZ());
            turtle.setTravelling(true);
            stuck = false;
        }
        public void tick() {
            if (!turtle.getNavigation().isDone()) {
                return;
            }
            Vec3 target = Vec3.atBottomCenterOf(turtle.travelPos);
            Vec3 towards = DefaultRandomPos.getPosTowards(turtle, 16, 3, target, (float) Math.PI / 10);
            if (towards == null) {
                towards = DefaultRandomPos.getPosTowards(turtle, 8, 7, target, (float) Math.PI / 2);
            }
            if (towards == null) {
                stuck = true;
                return;
            }
            turtle.getNavigation().moveTo(towards.x, towards.y, towards.z, speedModifier);
        }
        public boolean canContinueToUse() {
            return !turtle.getNavigation().isDone() && !stuck && !turtle.isGoingHome() && !turtle.isInLove() && !turtle.hasEgg();
        }
        public void stop() {
            turtle.setTravelling(false);
            super.stop();
        }
    }
    static class TurtleGoHomeGoal extends Goal {
        private final Turtle turtle;
        private final double speedModifier;
        private boolean stuck;
        private int closeToHomeTryTicks;
        public TurtleGoHomeGoal(Turtle turtle, double speedModifier) {
            this.turtle = turtle;
            this.speedModifier = speedModifier;
        }
        public boolean canUse() {
            if (turtle.isBaby() || turtle.getHomePos() == null) {
                return false;
            }
            return (turtle.hasEgg() || turtle.getRandom().nextInt(reducedTickDelay(700)) == 0)
                && turtle.getHomePos().distToCenterSqr(turtle.getX(), turtle.getY(), turtle.getZ()) >= 64.0;
        }
        public void start() {
            turtle.setGoingHome(true);
            stuck = false;
            closeToHomeTryTicks = 0;
        }
        public void stop() {
            turtle.setGoingHome(false);
        }
        public boolean canContinueToUse() {
            return turtle.getHomePos().distToCenterSqr(turtle.getX(), turtle.getY(), turtle.getZ()) >= 49.0 && !stuck
                && closeToHomeTryTicks <= adjustedTickDelay(600);
        }
        public void tick() {
            BlockPos home = turtle.getHomePos();
            boolean close = home.distToCenterSqr(turtle.getX(), turtle.getY(), turtle.getZ()) < 256.0;
            if (close) {
                closeToHomeTryTicks++;
            }
            if (!turtle.getNavigation().isDone()) {
                return;
            }
            Vec3 target = Vec3.atBottomCenterOf(home);
            Vec3 towards = DefaultRandomPos.getPosTowards(turtle, 16, 3, target, (float) Math.PI / 10);
            if (towards == null) {
                towards = DefaultRandomPos.getPosTowards(turtle, 8, 7, target, (float) Math.PI / 2);
            }
            if (towards != null && !close && !turtle.level().getBlockState(BlockPos.containing(towards)).is(Blocks.WATER)) {
                towards = DefaultRandomPos.getPosTowards(turtle, 16, 5, target, (float) Math.PI / 2);
            }
            if (towards == null) {
                stuck = true;
            } else {
                turtle.getNavigation().moveTo(towards.x, towards.y, towards.z, speedModifier);
            }
        }
    }
    static class TurtleBreedGoal extends BreedGoal {
        public TurtleBreedGoal(Turtle turtle, double speedModifier) {
            super(turtle, speedModifier);
        }
        protected void breed() {
                }
    }
    static class TurtleLayEggGoal extends MoveToBlockGoal {
        private final Turtle turtle;
        private int layEggCounter;
        public TurtleLayEggGoal(Turtle turtle, double speedModifier) {
            super(turtle, speedModifier, 16);
            this.turtle = turtle;
        }
        public boolean canUse() {
            return turtle.hasEgg() && turtle.getHomePos().distToCenterSqr(turtle.getX(), turtle.getY(), turtle.getZ()) < 81.0 && super.canUse();
        }
        public boolean canContinueToUse() {
            return super.canContinueToUse() && turtle.hasEgg()
                && turtle.getHomePos().distToCenterSqr(turtle.getX(), turtle.getY(), turtle.getZ()) < 81.0;
        }
        public void tick() {
            super.tick();
            if (turtle.isInWater() || !isReachedTarget()) {
                return;
            }
            if (layEggCounter > adjustedTickDelay(200)) {
                org.bukkit.block.Block block = ((org.bukkit.entity.Entity) turtle.getBukkitEntity()).getWorld()
                    .getBlockAt(blockPos.getX(), blockPos.getY() + 1, blockPos.getZ());
                block.setType(org.bukkit.Material.TURTLE_EGG);
                if (block.getBlockData() instanceof org.bukkit.block.data.type.TurtleEgg egg) {
                    egg.setEggs(1 + turtle.getRandom().nextInt(4));
                    block.setBlockData(egg);
                }
                if (turtle.live() instanceof org.bukkit.entity.Turtle api) {
                    api.setHasEgg(false);
                }
                turtle.setInLoveTime(600);
                layEggCounter = 0;
            } else {
                layEggCounter++;
            }
        }
        protected boolean isValidTarget(LevelReader level, BlockPos pos) {
            return level.isEmptyBlock(pos.above()) && level.getBlockState(pos).is(BlockTags.SAND);
        }
    }
    static class TurtleRandomStrollGoal extends RandomStrollGoal {
        private final Turtle turtle;
        private TurtleRandomStrollGoal(Turtle turtle, double speedModifier, int interval) {
            super(turtle, speedModifier, interval);
            this.turtle = turtle;
        }
        public boolean canUse() {
            return !mob.isInWater() && !turtle.isGoingHome() && !turtle.hasEgg() && super.canUse();
        }
    }
    static class TurtleGoToWaterGoal extends MoveToBlockGoal {
        private final Turtle turtle;
        private TurtleGoToWaterGoal(Turtle turtle, double speedModifier) {
            super(turtle, turtle.isBaby() ? 2.0 : speedModifier, 24);
            this.turtle = turtle;
            this.verticalSearchStart = -1;
        }
        public boolean canContinueToUse() {
            return !turtle.isInWater() && tryTicks <= 1200 && isValidTarget(turtle.level(), blockPos);
        }
        public boolean canUse() {
            if (turtle.isBaby() && !turtle.isInWater()) {
                return super.canUse();
            }
            return !turtle.isGoingHome() && !turtle.isInWater() && !turtle.hasEgg() && super.canUse();
        }
        public boolean shouldRecalculatePath() {
            return tryTicks % 160 == 0;
        }
        protected boolean isValidTarget(LevelReader level, BlockPos pos) {
            return level.getBlockState(pos).is(Blocks.WATER);
        }
    }
    static class TurtleMoveControl<T extends Turtle> extends MoveControl<T> {
        public TurtleMoveControl(T turtle) {
            super(turtle);
        }
        public void tick() {
            if (mob.isInWater() && operation == Operation.MOVE_TO && !mob.getNavigation().isDone()) {
                double dx = wantedX - mob.getX();
                double dy = wantedY - mob.getY();
                double dz = wantedZ - mob.getZ();
                double dist = Math.sqrt(dx * dx + dy * dy + dz * dz);
                if (dist > 1.0E-4) {
                    mob.setDeltaMovement(mob.getDeltaMovement().add(0.0, dy / dist * 0.025, 0.0));
                }
            }
            super.tick();
        }
    }
    static class TurtlePathNavigation extends AmphibiousPathNavigation {
        public TurtlePathNavigation(Turtle mob, Level level) {
            super(mob, level);
        }
        public boolean isStableDestination(BlockPos pos) {
            if (mob instanceof Turtle turtle && turtle.travelPos != null && !turtle.travelPos.equals(BlockPos.ZERO)) {
                return level.getBlockState(pos).is(net.minecraft.world.level.block.Blocks.WATER);
            }
            return !level.getBlockState(pos.below()).isAir();
        }
    }
}
