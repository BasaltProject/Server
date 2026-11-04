// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.animal.bee;
import net.minecraft.world.level.block.state.properties.BlockStateProperties;
import net.minecraft.world.level.block.entity.BlockEntity;
import net.minecraft.world.entity.ai.util.AirAndWaterRandomPos;
import net.minecraft.world.entity.ai.util.HoverRandomPos;
import net.minecraft.tags.BlockTags;
import java.util.List;
import net.minecraft.core.BlockPos;
import net.minecraft.network.syncher.SynchedEntityData;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.sounds.SoundEvent;
import net.minecraft.tags.TagKey;
import net.minecraft.util.debug.DebugValueSource;
import net.minecraft.world.InteractionHand;
import net.minecraft.world.InteractionResult;
import net.minecraft.world.damagesource.DamageSource;
import net.minecraft.world.entity.AgeableMob;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.entity.EntityReference;
import net.minecraft.world.entity.EntityType;
import net.minecraft.world.entity.LivingEntity;
import net.minecraft.world.entity.Mob;
import net.minecraft.world.entity.NeutralMob;
import net.minecraft.world.entity.PathfinderMob;
import net.minecraft.world.entity.ai.attributes.AttributeSupplier;
import net.minecraft.world.entity.ai.control.LookControl;
import net.minecraft.world.entity.ai.goal.Goal;
import net.minecraft.world.entity.ai.goal.MeleeAttackGoal;
import net.minecraft.world.entity.ai.goal.target.HurtByTargetGoal;
import net.minecraft.world.entity.ai.goal.target.NearestAttackableTargetGoal;
import net.minecraft.world.entity.ai.navigation.PathNavigation;
import net.minecraft.world.entity.animal.Animal;
import net.minecraft.world.entity.player.Player;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.level.Level;
import net.minecraft.world.level.LevelReader;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.material.Fluid;
import net.minecraft.world.level.storage.ValueInput;
import net.minecraft.world.level.storage.ValueOutput;
import net.minecraft.world.phys.Vec3;
public class Bee extends Animal implements NeutralMob {
    private long angerEndTime = -1L;
    private net.minecraft.world.entity.EntityReference<net.minecraft.world.entity.LivingEntity> angerTarget;
    public static final float FLAP_DEGREES_PER_TICK = 120.32113F;
    public static final int TICKS_PER_FLAP = 0;
    public static final String TAG_CROPS_GROWN_SINCE_POLLINATION = "CropsGrownSincePollination";
    public static final String TAG_CANNOT_ENTER_HIVE_TICKS = "CannotEnterHiveTicks";
    public static final String TAG_TICKS_SINCE_POLLINATION = "TicksSincePollination";
    public static final String TAG_HAS_STUNG = "HasStung";
    public static final String TAG_HAS_NECTAR = "HasNectar";
    public static final String TAG_FLOWER_POS = "flower_pos";
    public static final String TAG_HIVE_POS = "hive_pos";
    public static final boolean DEFAULT_HAS_NECTAR = false;
    public Bee(EntityType<? extends Bee> type, Level level) {
        super(type, level);
    }
    private BlockPos hivePos;
    private BlockPos savedFlowerPos;
    private int travellingTicks;
    private int ticksWithoutNectar;
    private int stayOutOfHiveCountdown;
    private int cooldownLocatingFlower;
    private int cooldownLocatingHive;
    private final List<BlockPos> blacklistedHives = new java.util.ArrayList<>();
    boolean wantsToEnterHive() {
        if (stayOutOfHiveCountdown > 0 || hasStung() || getTarget() != null) {
            return false;
        }
        return hasNectar() || ticksWithoutNectar > 3600 || level().isRaining() || level().isDarkOutside();
    }
    boolean isHiveValid() {
        return hivePos != null && level().getBlockState(hivePos).is(BlockTags.BEEHIVES);
    }
    boolean isFlowerValid(BlockPos pos) {
        return level().getBlockState(pos).is(BlockTags.FLOWERS);
    }
    private boolean closeEnough(BlockPos pos, double range) {
        return pos.distToCenterSqr(getX(), getY(), getZ()) < range * range;
    }
    protected void defineSynchedData(SynchedEntityData.Builder entityData) {
    }
    public float getWalkTargetValue(BlockPos pos, LevelReader level) {
        return super.getWalkTargetValue(pos, level);
    }
    protected void registerGoals() {
    }
    protected void addAdditionalSaveData(ValueOutput output) {
    }
    protected void readAdditionalSaveData(ValueInput input) {
    }
    public boolean doHurtTarget(ServerLevel level, Entity target) {
        return super.doHurtTarget(level, target);
    }
    public void tick() {
        super.tick();
        if (cooldownLocatingFlower > 0) { cooldownLocatingFlower--; }
        if (cooldownLocatingHive > 0) { cooldownLocatingHive--; }
        if (stayOutOfHiveCountdown > 0) { stayOutOfHiveCountdown--; }
        if (!hasNectar()) { ticksWithoutNectar++; }
    }
    public BlockPos getSavedFlowerPos() {
        return savedFlowerPos;
    }
    public boolean hasSavedFlowerPos() {
        return savedFlowerPos != null;
    }
    public void setSavedFlowerPos(BlockPos savedFlowerPos) {
        this.savedFlowerPos = savedFlowerPos;
    }
    public int getTravellingTicks() {
        return travellingTicks;
    }
    public List<BlockPos> getBlacklistedHives() {
        return blacklistedHives;
    }
    public void setStayOutOfHiveCountdown(int ticks) {
        this.stayOutOfHiveCountdown = ticks;
    }
    public float getRollAmount(float a) {
        return 0.0F;
    }
    protected void customServerAiStep(ServerLevel level) {
    }
    public void resetTicksWithoutNectarSinceExitingHive() {
        this.ticksWithoutNectar = 0;
    }
    public long getPersistentAngerEndTime() {
        return angerEndTime;
    }
    public void setPersistentAngerEndTime(long endTime) {
        this.angerEndTime = endTime;
    }
    public EntityReference<LivingEntity> getPersistentAngerTarget() {
        return angerTarget;
    }
    public void setPersistentAngerTarget(EntityReference<LivingEntity> persistentAngerTarget) {
        this.angerTarget = persistentAngerTarget;
    }
    public void startPersistentAngerTimer() {
        setTimeToRemainAngry(400 + random.nextInt(381));
    }
    public boolean hasHive() {
        return hivePos != null;
    }
    public BlockPos getHivePos() {
        return hivePos;
    }
    public void aiStep() {
    }
    public boolean hasNectar() {
        return live() instanceof org.bukkit.entity.Bee api ? (boolean) api.hasNectar() : false;
    }
    public boolean hasStung() {
        return live() instanceof org.bukkit.entity.Bee api ? (boolean) api.hasStung() : false;
    }
    public static AttributeSupplier.Builder createAttributes() {
        return Animal.createAnimalAttributes()
            .add(net.minecraft.world.entity.ai.attributes.Attributes.MAX_HEALTH, 10.0)
            .add(net.minecraft.world.entity.ai.attributes.Attributes.FLYING_SPEED, 0.6)
            .add(net.minecraft.world.entity.ai.attributes.Attributes.MOVEMENT_SPEED, 0.3)
            .add(net.minecraft.world.entity.ai.attributes.Attributes.ATTACK_DAMAGE, 2.0);
    }
    protected PathNavigation createNavigation(Level level) {
        return super.createNavigation(level);
    }
    public InteractionResult mobInteract(Player player, InteractionHand hand) {
        return net.minecraft.world.InteractionResult.PASS;
    }
    public boolean isFood(ItemStack itemStack) {
        return live() instanceof org.bukkit.entity.Animals animals && animals.isBreedItem(org.bukkit.craftbukkit.inventory.CraftItemStack.asCraftMirror(itemStack));
    }
    protected void playStepSound(BlockPos pos, BlockState blockState) {
    }
    protected SoundEvent getAmbientSound() {
        return null;
    }
    protected SoundEvent getHurtSound(DamageSource source) {
        return net.minecraft.sounds.SoundEvents.byId("entity.bee.hurt");
    }
    protected SoundEvent getDeathSound() {
        return net.minecraft.sounds.SoundEvents.byId("entity.bee.death");
    }
    protected float getSoundVolume() {
        return live() instanceof org.bukkit.entity.Bee api ? (float) api.getSoundVolume() : 0;
    }
    public Bee getBreedOffspring(ServerLevel level, AgeableMob partner) {
        return null;
    }
    protected void checkFallDamage(double ya, boolean onGround, BlockState onState, BlockPos pos) {
        }
    public boolean isFlapping() {
        return super.isFlapping();
    }
    public boolean isFlying() {
        return !onGround();
    }
    protected boolean omnidirectionalAirMover() {
        return false;
    }
    public void dropOffNectar() {
        if (live() instanceof org.bukkit.entity.Bee api) { api.setHasNectar(false); }
    }
    public boolean hurtServer(ServerLevel level, DamageSource source, float damage) {
        return super.hurtServer(level, source, damage);
    }
    protected void jumpInLiquid(TagKey<Fluid> type) {
        }
    public Vec3 getLeashOffset() {
        return new Vec3(0.0, getEyeHeight() * 0.5, getBbWidth() * 0.5);
    }
    public void setHivePos(BlockPos hivePos) {
        this.hivePos = hivePos;
    }
    public static boolean attractsBees(BlockState state) {
        return state.is(BlockTags.BEE_ATTRACTIVE);
    }
    public void registerDebugValues(ServerLevel level, DebugValueSource.Registration registration) {
    }
    class BeeHurtByOtherGoal extends HurtByTargetGoal {
        public BeeHurtByOtherGoal(Bee bee) {
            super(bee);
        }
        public boolean canContinueToUse() {
            return Bee.this.isAngry() && super.canContinueToUse();
        }
        protected void alertOther(Mob other, LivingEntity hurtByMob) {
            if (other instanceof Bee && hasLineOfSight(hurtByMob)) {
                other.setTarget(hurtByMob);
            }
        }
    }
    static class BeeBecomeAngryTargetGoal extends NearestAttackableTargetGoal<Player> {
        public BeeBecomeAngryTargetGoal(Bee bee) {
            super(bee, Player.class, 10, true, false, bee::isAngryAt);
        }
        public boolean canUse() {
            return beeCanTarget() && super.canUse();
        }
        public boolean canContinueToUse() {
            return beeCanTarget() && super.canContinueToUse();
        }
        private boolean beeCanTarget() {
            Bee bee = (Bee) mob;
            return bee.isAngry() && !bee.hasStung();
        }
    }
    abstract class BaseBeeGoal extends Goal {
        private BaseBeeGoal() {
        }
        public abstract boolean canBeeUse();
        public abstract boolean canBeeContinueToUse();
        public boolean canUse() {
            return canBeeUse() && !isAngry();
        }
        public boolean canContinueToUse() {
            return canBeeContinueToUse() && !isAngry();
        }
    }
    class BeeWanderGoal extends Goal {
        public BeeWanderGoal() {
            setFlags(java.util.EnumSet.of(Goal.Flag.MOVE));
        }
        public boolean canUse() {
            return getNavigation().isDone() && random.nextInt(10) == 0;
        }
        public boolean canContinueToUse() {
            return getNavigation().isInProgress();
        }
        public void start() {
            Vec3 pos = findPos();
            if (pos != null) {
                getNavigation().moveTo(pos.x, pos.y, pos.z, 1.0);
            }
        }
        private Vec3 findPos() {
            Vec3 direction = isHiveValid() && !closeEnough(hivePos, 22.0)
                ? Vec3.atCenterOf(hivePos).subtract(position()).normalize() : getViewVector(0.0F);
            Vec3 pos = HoverRandomPos.getPos(Bee.this, 8, 7, direction.x, direction.z, (float) (Math.PI / 2), 3, 1);
            return pos != null ? pos : AirAndWaterRandomPos.getPos(Bee.this, 8, 4, -2, direction.x, direction.z, (float) (Math.PI / 2));
        }
    }
    public class BeeGoToHiveGoal extends BaseBeeGoal {
        public static final int MAX_TRAVELLING_TICKS = 600;
        public BeeGoToHiveGoal() {
            setFlags(java.util.EnumSet.of(Goal.Flag.MOVE));
        }
        public boolean canBeeUse() {
            return isHiveValid() && wantsToEnterHive() && !closeEnough(hivePos, 2.0);
        }
        public boolean canBeeContinueToUse() {
            return canBeeUse();
        }
        public void start() {
            travellingTicks = 0;
        }
        public void stop() {
            travellingTicks = 0;
            getNavigation().stop();
        }
        public void tick() {
            if (hivePos == null) {
                return;
            }
            if (++travellingTicks > adjustedTickDelay(MAX_TRAVELLING_TICKS)) {
                blacklistedHives.add(hivePos);
                hivePos = null;
            } else if (getNavigation().isDone()) {
                getNavigation().moveTo(hivePos.getX() + 0.5, hivePos.getY() + 0.5, hivePos.getZ() + 0.5, 1.0);
            }
        }
    }
    public class BeeGoToKnownFlowerGoal extends BaseBeeGoal {
        public BeeGoToKnownFlowerGoal() {
            setFlags(java.util.EnumSet.of(Goal.Flag.MOVE));
        }
        public boolean canBeeUse() {
            return savedFlowerPos != null && getTarget() == null && !hasNectar() && !closeEnough(savedFlowerPos, 2.0);
        }
        public boolean canBeeContinueToUse() {
            return canBeeUse();
        }
        public void start() {
            travellingTicks = 0;
        }
        public void stop() {
            travellingTicks = 0;
            getNavigation().stop();
        }
        public void tick() {
            if (savedFlowerPos == null) {
                return;
            }
            if (++travellingTicks > adjustedTickDelay(600) || !isFlowerValid(savedFlowerPos)) {
                savedFlowerPos = null;
            } else if (getNavigation().isDone()) {
                getNavigation().moveTo(savedFlowerPos.getX() + 0.5, savedFlowerPos.getY() + 0.5, savedFlowerPos.getZ() + 0.5, 1.0);
            }
        }
    }
    class BeeLookControl extends LookControl {
        public BeeLookControl(Mob mob) {
            super(mob);
        }
        public void tick() {
            if (!isAngry()) {
                super.tick();
            }
        }
        protected boolean resetXRotOnTick() {
            return !isPollinating();
        }
    }
    private boolean pollinating;
    boolean isPollinating() {
        return pollinating;
    }
    class BeePollinateGoal extends BaseBeeGoal {
        private int pollinatingTicks;
        public BeePollinateGoal() {
            setFlags(java.util.EnumSet.of(Goal.Flag.MOVE));
        }
        public boolean canBeeUse() {
            if (cooldownLocatingFlower > 0 || hasNectar() || level().isRaining()) {
                return false;
            }
            BlockPos flower = findFlower();
            if (flower == null) {
                cooldownLocatingFlower = 20 + random.nextInt(41);
                return false;
            }
            savedFlowerPos = flower;
            getNavigation().moveTo(flower.getX() + 0.5, flower.getY() + 0.5, flower.getZ() + 0.5, 1.0);
            return true;
        }
        public boolean canBeeContinueToUse() {
            return pollinating && savedFlowerPos != null && isFlowerValid(savedFlowerPos) && !hasNectar() && !level().isRaining();
        }
        public boolean requiresUpdateEveryTick() {
            return true;
        }
        public void start() {
            pollinatingTicks = 0;
            pollinating = true;
        }
        public void stop() {
            pollinating = false;
            if (pollinatingTicks > 200 && live() instanceof org.bukkit.entity.Bee api) {
                api.setHasNectar(true);
            }
            pollinatingTicks = 0;
            getNavigation().stop();
            cooldownLocatingFlower = 200;
        }
        public void tick() {
            if (savedFlowerPos == null) {
                return;
            }
            if (closeEnough(savedFlowerPos, 1.0)) {
                pollinatingTicks++;
                if (pollinatingTicks > 400) {
                    stop();
                    pollinating = false;
                } else {
                    getNavigation().stop();
                }
            } else if (getNavigation().isDone()) {
                getNavigation().moveTo(savedFlowerPos.getX() + 0.5, savedFlowerPos.getY() + 0.5, savedFlowerPos.getZ() + 0.5, 1.0);
            }
        }
        private BlockPos findFlower() {
            BlockPos origin = blockPosition();
            BlockPos best = null;
            double bestDist = Double.MAX_VALUE;
            for (int dx = -5; dx <= 5; dx++) {
                for (int dy = -5; dy <= 5; dy++) {
                    for (int dz = -5; dz <= 5; dz++) {
                        BlockPos pos = origin.offset(dx, dy, dz);
                        double dist = pos.distSqr(origin);
                        if (dist < bestDist && isFlowerValid(pos)) {
                            bestDist = dist;
                            best = pos;
                        }
                    }
                }
            }
            return best;
        }
    }
    class BeeLocateHiveGoal extends BaseBeeGoal {
        private BeeLocateHiveGoal() {
        }
        public boolean canBeeUse() {
            return cooldownLocatingHive == 0 && !hasHive() && wantsToEnterHive();
        }
        public boolean canBeeContinueToUse() {
            return false;
        }
        public void start() {
            cooldownLocatingHive = 200;
            BlockPos origin = blockPosition();
            BlockPos best = null;
            double bestDist = Double.MAX_VALUE;
            for (int dx = -20; dx <= 20; dx++) {
                for (int dy = -8; dy <= 8; dy++) {
                    for (int dz = -20; dz <= 20; dz++) {
                        BlockPos pos = origin.offset(dx, dy, dz);
                        double dist = pos.distSqr(origin);
                        if (dist < bestDist && !blacklistedHives.contains(pos) && level().getBlockState(pos).is(BlockTags.BEEHIVES)) {
                            bestDist = dist;
                            best = pos;
                        }
                    }
                }
            }
            if (best != null) {
                hivePos = best;
            }
        }
    }
    class BeeGrowCropGoal extends BaseBeeGoal {
        private BeeGrowCropGoal() {
            super();
        }
        public boolean canBeeUse() {
            if (cropsGrownSincePollination >= 10 || random.nextFloat() < 0.3F) {
                return false;
            }
            return hasNectar() && isHiveValid();
        }
        public boolean canBeeContinueToUse() {
            return canBeeUse();
        }
        public void tick() {
            if (random.nextInt(adjustedTickDelay(30)) != 0) {
                return;
            }
            for (int depth = 1; depth <= 2; depth++) {
                BlockPos target = blockPosition().below(depth);
                BlockState state = level().getBlockState(target);
                if (!state.is(net.minecraft.tags.BlockTags.BEE_GROWABLES)) {
                    continue;
                }
                BlockState grown = null;
                if (state.getBlock() instanceof net.minecraft.world.level.block.CropBlock crop) {
                    grown = crop.isMaxAge(state) ? null : crop.getStateForAge(crop.getAge(state) + 1);
                } else if (state.hasProperty(BlockStateProperties.AGE_7)) {
                    int age = state.getValue(BlockStateProperties.AGE_7);
                    grown = age < 7 ? state.setValue(BlockStateProperties.AGE_7, age + 1) : null;
                } else if (state.hasProperty(BlockStateProperties.AGE_3)) {
                    int age = state.getValue(BlockStateProperties.AGE_3);
                    grown = age < 3 ? state.setValue(BlockStateProperties.AGE_3, age + 1) : null;
                }
                if (grown != null) {
                    level().levelEvent(2011, target, 15);
                    level().setBlockAndUpdate(target, grown);
                    cropsGrownSincePollination++;
                }
            }
        }
    }
    class BeeAttackGoal extends MeleeAttackGoal {
        public BeeAttackGoal(PathfinderMob mob, double speedModifier, boolean trackTarget) {
            super(mob, speedModifier, trackTarget);
        }
    }
    class BeeEnterHiveGoal extends BaseBeeGoal {
        private BeeEnterHiveGoal() {
        }
        public boolean canBeeUse() {
            return hasHive() && wantsToEnterHive() && isHiveValid() && closeEnough(hivePos, 2.0);
        }
        public boolean canBeeContinueToUse() {
            return false;
        }
        public void start() {
            org.bukkit.entity.Entity self = live();
            if (self != null && self.getWorld().getBlockAt(hivePos.getX(), hivePos.getY(), hivePos.getZ()).getState() instanceof org.bukkit.block.Beehive hive
                && !hive.isFull() && self instanceof org.bukkit.entity.Bee bee) {
                hive.addEntity(bee);
                hive.update();
            }
        }
    }
    class ValidateFlowerGoal extends BaseBeeGoal {
        public void start() {
            if (savedFlowerPos != null && !isFlowerValid(savedFlowerPos)) {
                savedFlowerPos = null;
            }
        }
        public boolean canBeeUse() {
            return savedFlowerPos != null && !isFlowerValid(savedFlowerPos);
        }
        public boolean canBeeContinueToUse() {
            return false;
        }
    }
    class ValidateHiveGoal extends BaseBeeGoal {
        public void start() {
            if (hivePos != null && !isHiveValid()) {
                hivePos = null;
            }
        }
        public boolean canBeeUse() {
            return hivePos != null && !isHiveValid();
        }
        public boolean canBeeContinueToUse() {
            return false;
        }
    }
    private int cropsGrownSincePollination;

}
