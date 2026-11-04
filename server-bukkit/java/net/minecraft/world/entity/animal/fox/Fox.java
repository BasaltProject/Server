// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.animal.fox;
import net.minecraft.tags.BiomeTags;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.util.Mth;
import com.mojang.serialization.Codec;
import io.netty.buffer.ByteBuf;
import net.minecraft.core.BlockPos;
import net.minecraft.core.Holder;
import net.minecraft.core.component.DataComponentGetter;
import net.minecraft.core.component.DataComponentType;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.network.syncher.SynchedEntityData;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.sounds.SoundEvent;
import net.minecraft.util.RandomSource;
import net.minecraft.util.StringRepresentable;
import net.minecraft.world.DifficultyInstance;
import net.minecraft.world.damagesource.DamageSource;
import net.minecraft.world.entity.AgeableMob;
import net.minecraft.world.entity.EntityDimensions;
import net.minecraft.world.entity.EntitySpawnReason;
import net.minecraft.world.entity.EntityType;
import net.minecraft.world.entity.EquipmentSlot;
import net.minecraft.world.entity.LivingEntity;
import net.minecraft.world.entity.Mob;
import net.minecraft.world.entity.Pose;
import net.minecraft.world.entity.SpawnGroupData;
import net.minecraft.world.entity.ai.attributes.AttributeSupplier;
import net.minecraft.world.entity.ai.control.LookControl;
import net.minecraft.world.entity.ai.control.MoveControl;
import net.minecraft.world.entity.ai.goal.BreedGoal;
import net.minecraft.world.entity.ai.goal.FleeSunGoal;
import net.minecraft.world.entity.ai.goal.FloatGoal;
import net.minecraft.world.entity.ai.goal.FollowParentGoal;
import net.minecraft.world.entity.ai.goal.Goal;
import net.minecraft.world.entity.ai.goal.JumpGoal;
import net.minecraft.world.entity.ai.goal.LookAtPlayerGoal;
import net.minecraft.world.entity.ai.goal.MeleeAttackGoal;
import net.minecraft.world.entity.ai.goal.MoveToBlockGoal;
import net.minecraft.world.entity.ai.goal.PanicGoal;
import net.minecraft.world.entity.ai.goal.StrollThroughVillageGoal;
import net.minecraft.world.entity.ai.goal.target.NearestAttackableTargetGoal;
import net.minecraft.world.entity.ai.targeting.TargetingConditions;
import net.minecraft.world.entity.animal.Animal;
import net.minecraft.world.entity.item.ItemEntity;
import net.minecraft.world.entity.player.Player;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.level.Level;
import net.minecraft.world.level.LevelAccessor;
import net.minecraft.world.level.LevelReader;
import net.minecraft.world.level.ServerLevelAccessor;
import net.minecraft.world.level.biome.Biome;
import net.minecraft.world.level.storage.ValueInput;
import net.minecraft.world.level.storage.ValueOutput;
import net.minecraft.world.phys.Vec3;
public class Fox extends Animal {
    public static final int FLAG_CROUCHING = 4;
    public static final int FLAG_INTERESTED = 8;
    public static final int FLAG_POUNCING = 16;
    public Fox(EntityType<? extends Fox> type, Level level) {
        super(type, level);
        this.moveControl = new FoxMoveControl<>(this);
        this.lookControl = new FoxLookControl();
    }
    float crouchAmount;
    float crouchAmountO;
    float interestedAngle;
    float interestedAngleO;
    static final java.util.function.Predicate<ItemEntity> ALLOWED_ITEMS = item -> !item.hasPickUpDelay() && item.isAlive();
    static final java.util.function.Predicate<Entity> STALKABLE_PREY = entity ->
        entity instanceof net.minecraft.world.entity.animal.chicken.Chicken || entity instanceof net.minecraft.world.entity.animal.rabbit.Rabbit;
    void setFaceplanted(boolean faceplanted) {
        if (live() instanceof org.bukkit.entity.Fox api) { api.setFaceplanted(faceplanted); }
    }
    boolean isDefending() {
        return live() instanceof org.bukkit.entity.Fox api && api.isDefending();
    }
    void setDefending(boolean defending) {
        if (live() instanceof org.bukkit.entity.Fox api) { api.setDefending(defending); }
    }
    void setSleeping(boolean sleeping) {
        if (live() instanceof org.bukkit.entity.Fox api) { api.setSleeping(sleeping); }
    }
    void wakeUp() {
        setSleeping(false);
    }
    void clearStates() {
        setIsPouncing(false);
        setIsCrouching(false);
        setIsInterested(false);
        setSleeping(false);
        setSitting(false);
        setFaceplanted(false);
    }
    boolean trusts(LivingEntity entity) {
        if (!(live() instanceof org.bukkit.entity.Fox api)) {
            return false;
        }
        java.util.UUID id = entity.getUUID();
        return api.getFirstTrustedPlayer() != null && api.getFirstTrustedPlayer().getUniqueId().equals(id)
            || api.getSecondTrustedPlayer() != null && api.getSecondTrustedPlayer().getUniqueId().equals(id);
    }
    void addTrustedEntity(LivingEntity entity) {
        if (live() instanceof org.bukkit.entity.Fox api && entity.getBukkitEntity() instanceof org.bukkit.entity.AnimalTamer tamer) {
            if (api.getFirstTrustedPlayer() == null) {
                api.setFirstTrustedPlayer(tamer);
            } else {
                api.setSecondTrustedPlayer(tamer);
            }
        }
    }
    protected void defineSynchedData(SynchedEntityData.Builder entityData) {
    }
    protected void registerGoals() {
    }
    public void aiStep() {
    }
    protected boolean isImmobile() {
        return isDeadOrDying() || isSleeping();
    }
    protected void populateDefaultEquipmentSlots(RandomSource random, DifficultyInstance difficulty) {
        }
    public void handleEntityEvent(byte id) {
    }
    public static AttributeSupplier.Builder createAttributes() {
        return Animal.createAnimalAttributes()
            .add(net.minecraft.world.entity.ai.attributes.Attributes.MOVEMENT_SPEED, 0.3)
            .add(net.minecraft.world.entity.ai.attributes.Attributes.MAX_HEALTH, 10.0)
            .add(net.minecraft.world.entity.ai.attributes.Attributes.ATTACK_DAMAGE, 2.0)
            .add(net.minecraft.world.entity.ai.attributes.Attributes.SAFE_FALL_DISTANCE, 5.0)
            .add(net.minecraft.world.entity.ai.attributes.Attributes.FOLLOW_RANGE, 32.0);
    }
    public Fox getBreedOffspring(ServerLevel level, AgeableMob partner) {
        return null;
    }
    public static boolean checkFoxSpawnRules(EntityType<Fox> type, LevelAccessor level, EntitySpawnReason spawnReason, BlockPos pos, RandomSource random) {
        return level.getBlockState(pos.below()).is(net.minecraft.tags.BlockTags.FOXES_SPAWNABLE_ON) && isBrightEnoughToSpawn(level, pos);
    }
    public SpawnGroupData finalizeSpawn(ServerLevelAccessor level, DifficultyInstance difficulty, EntitySpawnReason spawnReason, SpawnGroupData groupData) {
        return groupData;
    }
    protected void playEatingSound() {
        }
    public EntityDimensions getDefaultDimensions(Pose pose) {
        return net.minecraft.world.entity.EntityDimensions.scalable(getBbWidth(), getBbHeight());
    }
    public Variant getVariant() {
        return Variant.values()[live() instanceof org.bukkit.entity.Fox api ? api.getFoxType().ordinal() : 0];
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
    public boolean isSitting() {
        return live() instanceof org.bukkit.entity.Fox api ? (boolean) api.isSitting() : false;
    }
    public void setSitting(boolean value) {
        if (live() instanceof org.bukkit.entity.Fox api) { api.setSitting(value); }
    }
    public boolean isFaceplanted() {
        return live() instanceof org.bukkit.entity.Fox api ? (boolean) api.isFaceplanted() : false;
    }
    public boolean isSleeping() {
        return live() instanceof org.bukkit.entity.Fox api ? (boolean) api.isSleeping() : false;
    }
    protected boolean canDispenserEquipIntoSlot(EquipmentSlot slot) {
        return false;
    }
    public boolean canHoldItem(ItemStack itemStack) {
        return getItemBySlot(EquipmentSlot.MAINHAND).isEmpty();
    }
    protected void pickUpItem(ServerLevel level, ItemEntity entity) {
        }
    public void tick() {
        super.tick();
        crouchAmountO = crouchAmount;
        crouchAmount = isCrouching() ? Math.min(3.0F, crouchAmount + 1.0F) : Math.max(0.0F, crouchAmount - 1.0F);
        interestedAngleO = interestedAngle;
        interestedAngle = isInterested() ? Math.min(1.0F, interestedAngle + 0.1F) : Math.max(0.0F, interestedAngle - 0.1F);
    }
    public boolean isFood(ItemStack itemStack) {
        return live() instanceof org.bukkit.entity.Animals animals && animals.isBreedItem(org.bukkit.craftbukkit.inventory.CraftItemStack.asCraftMirror(itemStack));
    }
    protected void onOffspringSpawnedFromEgg(Player spawner, Mob offspring) {
        }
    public boolean isPouncing() {
        return live() instanceof org.bukkit.entity.Fox api && api.isLeaping();
    }
    public void setIsPouncing(boolean pouncing) {
        if (live() instanceof org.bukkit.entity.Fox api) { api.setLeaping(pouncing); }
    }
    public boolean isFullyCrouched() {
        return crouchAmount >= 3.0F;
    }
    public void setIsCrouching(boolean isCrouching) {
        if (live() instanceof org.bukkit.entity.Fox api) { api.setCrouching(isCrouching); }
    }
    public boolean isCrouching() {
        return live() instanceof org.bukkit.entity.Fox api ? (boolean) api.isCrouching() : false;
    }
    public void setIsInterested(boolean value) {
        if (live() instanceof org.bukkit.entity.Fox api) { api.setInterested(value); }
    }
    public boolean isInterested() {
        return live() instanceof org.bukkit.entity.Fox api ? (boolean) api.isInterested() : false;
    }
    public float getHeadRollAngle(float a) {
        return Mth.lerp(a, interestedAngleO, interestedAngle) * 0.11F * (float) Math.PI;
    }
    public float getCrouchAmount(float a) {
        return Mth.lerp(a, crouchAmountO, crouchAmount);
    }
    public void setTarget(LivingEntity target) {
        super.setTarget(target);
        if (target == null && isDefending()) {
            setDefending(false);
        }
    }
    public boolean canMove() {
        return !isSleeping() && !isSitting() && !isFaceplanted();
    }
    public void playAmbientSound() {
    }
    protected SoundEvent getAmbientSound() {
        return net.minecraft.sounds.SoundEvents.byId("entity.fox.ambient");
    }
    protected SoundEvent getHurtSound(DamageSource source) {
        return net.minecraft.sounds.SoundEvents.byId("entity.fox.hurt");
    }
    protected SoundEvent getDeathSound() {
        return net.minecraft.sounds.SoundEvents.byId("entity.fox.death");
    }
    protected void dropAllDeathLoot(ServerLevel level, DamageSource source) {
        }
    public static boolean isPathClear(Fox fox, LivingEntity target) {
        double dx = target.getX() - fox.getX();
        double dz = target.getZ() - fox.getZ();
        for (int i = 0; i < 6; i++) {
            double fraction = i / 6.0;
            for (int j = 1; j < 4; j++) {
                net.minecraft.world.level.block.state.BlockState state = fox.level().getBlockState(
                    BlockPos.containing(fox.getX() + dx * fraction, fox.getY() + j, fox.getZ() + dz * fraction));
                if (state.isSolid()) {
                    return false;
                }
            }
        }
        return true;
    }
    public Vec3 getLeashOffset() {
        return new Vec3(0.0, 0.55F * getEyeHeight(), getBbWidth() * 0.4F);
    }
    public static enum Variant implements StringRepresentable {
        RED, SNOW;
        public static final Variant DEFAULT = RED;
        public static final Codec<Variant> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.entity.animal.fox.Fox.Variant.class, "CODEC");
        public static final StreamCodec<ByteBuf, Variant> STREAM_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.streamCodec(net.minecraft.world.entity.animal.fox.Fox.Variant.class, "STREAM_CODEC");
        public String getSerializedName() {
            return name().toLowerCase(java.util.Locale.ROOT);
        }
        public int getId() {
            return ordinal();
        }
        public static Variant byId(int id) {
            return values()[Math.floorMod(id, values().length)];
        }
        public static Variant byBiome(Holder<Biome> biome) {
            return biome.is(BiomeTags.SPAWNS_SNOW_FOXES) ? SNOW : RED;
        }
    }
    class FoxSearchForItemsGoal extends Goal {
        public FoxSearchForItemsGoal() {
            setFlags(java.util.EnumSet.of(Goal.Flag.MOVE));
        }
        private java.util.List<ItemEntity> items() {
            return level().getEntitiesOfClass(ItemEntity.class, getBoundingBox().inflate(8.0, 8.0, 8.0), ALLOWED_ITEMS);
        }
        public boolean canUse() {
            if (!getItemBySlot(EquipmentSlot.MAINHAND).isEmpty() || getTarget() != null || getLastHurtByMob() != null) {
                return false;
            }
            return canMove() && getRandom().nextInt(reducedTickDelay(10)) == 0 && !items().isEmpty();
        }
        public void tick() {
            java.util.List<ItemEntity> items = items();
            if (getItemBySlot(EquipmentSlot.MAINHAND).isEmpty() && !items.isEmpty()) {
                getNavigation().moveTo(items.get(0), 1.2F);
            }
        }
        public void start() {
            java.util.List<ItemEntity> items = items();
            if (!items.isEmpty()) {
                getNavigation().moveTo(items.get(0), 1.2F);
            }
        }
    }
    static class FoxMoveControl<T extends Fox> extends MoveControl<T> {
        public FoxMoveControl(T fox) {
            super(fox);
        }
        public void tick() {
            if (mob.canMove()) {
                super.tick();
            }
        }
    }
    class StalkPreyGoal extends Goal {
        public StalkPreyGoal() {
            setFlags(java.util.EnumSet.of(Goal.Flag.MOVE, Goal.Flag.LOOK));
        }
        public boolean canUse() {
            if (isSleeping()) {
                return false;
            }
            LivingEntity target = getTarget();
            return target != null && target.isAlive() && STALKABLE_PREY.test(target) && distanceToSqr(target) > 36.0
                && !isCrouching() && !isInterested() && !isFaceplanted();
        }
        public void start() {
            setSitting(false);
            setFaceplanted(false);
        }
        public void stop() {
            LivingEntity target = getTarget();
            if (target != null && isPathClear(Fox.this, target)) {
                setIsInterested(true);
                setIsCrouching(true);
                getNavigation().stop();
                getLookControl().setLookAt(target, getMaxHeadYRot(), getMaxHeadXRot());
            } else {
                setIsInterested(false);
                setIsCrouching(false);
            }
        }
        public void tick() {
            LivingEntity target = getTarget();
            if (target == null) {
                return;
            }
            getLookControl().setLookAt(target, getMaxHeadYRot(), getMaxHeadXRot());
            if (distanceToSqr(target) <= 36.0) {
                setIsInterested(true);
                setIsCrouching(true);
                getNavigation().stop();
            } else {
                getNavigation().moveTo(target, 1.5);
            }
        }
    }
    class FoxMeleeAttackGoal extends MeleeAttackGoal {
        public FoxMeleeAttackGoal(double speedModifier, boolean trackTarget) {
            super(Fox.this, speedModifier, trackTarget);
        }
        protected void checkAndPerformAttack(LivingEntity target) {
            if (canPerformAttack(target)) {
                resetAttackCooldown();
                mob.doHurtTarget(null, target);
            }
        }
        public void start() {
            setIsInterested(false);
            super.start();
        }
        public boolean canUse() {
            return !isSitting() && !isSleeping() && !isCrouching() && !isFaceplanted() && super.canUse();
        }
    }
    class FoxBreedGoal extends BreedGoal {
        public FoxBreedGoal(double speedModifier) {
            super(Fox.this, speedModifier);
        }
        public void start() {
            Fox.this.clearStates();
            super.start();
        }
    }
    class DefendTrustedTargetGoal extends NearestAttackableTargetGoal<LivingEntity> {
        private LivingEntity trustedLastHurtBy;
        private LivingEntity trustedLastHurt;
        private int timestamp;
        public DefendTrustedTargetGoal(Class<LivingEntity> targetType, boolean mustSee, boolean mustReach, TargetingConditions.Selector subselector) {
            super(Fox.this, targetType, 10, mustSee, mustReach, subselector);
        }
        public boolean canUse() {
            if (randomInterval > 0 && mob.getRandom().nextInt(randomInterval) != 0 || !(live() instanceof org.bukkit.entity.Fox api)) {
                return false;
            }
            for (org.bukkit.entity.AnimalTamer tamer : new org.bukkit.entity.AnimalTamer[] {api.getFirstTrustedPlayer(), api.getSecondTrustedPlayer()}) {
                if (tamer == null) {
                    continue;
                }
                org.bukkit.entity.Entity found = org.bukkit.Bukkit.getEntity(tamer.getUniqueId());
                if (found != null && Entity.fromBukkit(found) instanceof LivingEntity trusted) {
                    trustedLastHurt = trusted;
                    trustedLastHurtBy = trusted.getLastHurtByMob();
                    return trusted.getLastHurtByMobTimestamp() != timestamp && trustedLastHurtBy != null && Fox.this.canAttack(trustedLastHurtBy);
                }
            }
            return false;
        }
        public void start() {
            setTarget(trustedLastHurtBy);
            target = trustedLastHurtBy;
            if (trustedLastHurt != null) {
                timestamp = trustedLastHurt.getLastHurtByMobTimestamp();
            }
            setDefending(true);
            wakeUp();
            super.start();
        }
    }
    class SeekShelterGoal extends FleeSunGoal {
        private int interval = reducedTickDelay(100);
        public SeekShelterGoal(double speedModifier) {
            super(Fox.this, speedModifier);
        }
        public boolean canUse() {
            if (isSleeping() || mob.getTarget() != null) {
                return false;
            }
            if (level().isThundering() && level().canSeeSky(mob.blockPosition())) {
                return setWantedPos();
            }
            if (interval > 0) {
                interval--;
                return false;
            }
            interval = 100;
            return level().isBrightOutside() && level().canSeeSky(mob.blockPosition()) && setWantedPos();
        }
        public void start() {
            Fox.this.clearStates();
            super.start();
        }
    }
    public class FoxAlertableEntitiesSelector implements TargetingConditions.Selector {
        public FoxAlertableEntitiesSelector() {
        }
        public boolean test(LivingEntity target, ServerLevel level) {
            if (target instanceof Fox) {
                return false;
            }
            if (target instanceof net.minecraft.world.entity.animal.chicken.Chicken
                || target instanceof net.minecraft.world.entity.animal.rabbit.Rabbit
                || target instanceof net.minecraft.world.entity.monster.Monster) {
                return true;
            }
            if (target instanceof net.minecraft.world.entity.TamableAnimal tamable) {
                return !tamable.isTame();
            }
            if (target instanceof Player player && (player.isSpectator() || player.isCreative())) {
                return false;
            }
            return !trusts(target) && !target.isSleeping() && !target.isDiscrete();
        }
    }
    abstract class FoxBehaviorGoal extends Goal {
        private FoxBehaviorGoal() {
        }
        protected boolean hasShelter() {
            BlockPos pos = BlockPos.containing(getX(), getBoundingBox().maxY, getZ());
            return !level().canSeeSky(pos) && getWalkTargetValue(pos) >= 0.0F;
        }
        protected boolean alertable() {
            FoxAlertableEntitiesSelector selector = new FoxAlertableEntitiesSelector();
            for (LivingEntity other : level().getEntitiesOfClass(LivingEntity.class, getBoundingBox().inflate(12.0, 6.0, 12.0))) {
                if (other != Fox.this && other.isAlive() && selector.test(other, null) && distanceToSqr(other) <= 144.0) {
                    return true;
                }
            }
            return false;
        }
    }
    class SleepGoal extends FoxBehaviorGoal {
        private final int waitTime = reducedTickDelay(140);
        private int countdown;
        public SleepGoal() {
            countdown = random.nextInt(waitTime);
            setFlags(java.util.EnumSet.of(Goal.Flag.MOVE, Goal.Flag.LOOK, Goal.Flag.JUMP));
        }
        public boolean canUse() {
            if (xxa == 0.0F && yya == 0.0F && zza == 0.0F) {
                return canSleep() || isSleeping();
            }
            return false;
        }
        public boolean canContinueToUse() {
            return canSleep();
        }
        private boolean canSleep() {
            if (countdown > 0) {
                countdown--;
                return false;
            }
            return level().isBrightOutside() && hasShelter() && !alertable();
        }
        public void stop() {
            countdown = random.nextInt(waitTime);
            clearStates();
        }
        public void start() {
            setSitting(false);
            setIsCrouching(false);
            setIsInterested(false);
            setSleeping(true);
            getNavigation().stop();
            getMoveControl().setWantedPosition(getX(), getY(), getZ(), 0.0);
        }
    }
    class PerchAndSearchGoal extends FoxBehaviorGoal {
        private double relX;
        private double relZ;
        private int lookTime;
        private int looksRemaining;
        public PerchAndSearchGoal() {
            setFlags(java.util.EnumSet.of(Goal.Flag.MOVE, Goal.Flag.LOOK));
        }
        public boolean canUse() {
            return getLastHurtByMob() == null && getRandom().nextFloat() < 0.02F && !isSleeping() && getTarget() == null
                && getNavigation().isDone() && !alertable() && !isPouncing() && !isCrouching();
        }
        public boolean canContinueToUse() {
            return looksRemaining > 0;
        }
        public void start() {
            resetLook();
            looksRemaining = 2 + getRandom().nextInt(3);
            setSitting(true);
            getNavigation().stop();
        }
        public void stop() {
            setSitting(false);
        }
        public void tick() {
            lookTime--;
            if (lookTime <= 0) {
                looksRemaining--;
                resetLook();
            }
            getLookControl().setLookAt(getX() + relX, getEyeY(), getZ() + relZ, getMaxHeadYRot(), getMaxHeadXRot());
        }
        private void resetLook() {
            double angle = Math.PI * 2.0 * getRandom().nextDouble();
            relX = Math.cos(angle);
            relZ = Math.sin(angle);
            lookTime = adjustedTickDelay(80 + getRandom().nextInt(20));
        }
    }
    public class FoxEatBerriesGoal extends MoveToBlockGoal {
        protected int ticksWaited;
        public FoxEatBerriesGoal(double speedModifier, int searchRange, int verticalSearchRange) {
            super(Fox.this, speedModifier, searchRange, verticalSearchRange);
        }
        public double acceptedDistance() {
            return 2.0;
        }
        public boolean shouldRecalculatePath() {
            return tryTicks % 100 == 0;
        }
        protected boolean isValidTarget(LevelReader level, BlockPos pos) {
            BlockState state = level.getBlockState(pos);
            return state.is(net.minecraft.world.level.block.Blocks.SWEET_BERRY_BUSH)
                && state.getValue(net.minecraft.world.level.block.SweetBerryBushBlock.AGE) >= 2;
        }
        public void tick() {
            if (isReachedTarget()) {
                if (ticksWaited >= 40) {
                    onReachedTarget();
                } else {
                    ticksWaited++;
                }
            }
            super.tick();
        }
        protected void onReachedTarget() {
            if (!Boolean.TRUE.equals(live().getWorld().getGameRuleValue(org.bukkit.GameRule.MOB_GRIEFING))) {
                return;
            }
            BlockState state = level().getBlockState(blockPos);
            if (!state.is(net.minecraft.world.level.block.Blocks.SWEET_BERRY_BUSH)) {
                return;
            }
            int age = state.getValue(net.minecraft.world.level.block.SweetBerryBushBlock.AGE);
            int count = 1 + level().getRandom().nextInt(2) + (age == 3 ? 1 : 0);
            if (getItemBySlot(EquipmentSlot.MAINHAND).isEmpty()) {
                setItemSlot(EquipmentSlot.MAINHAND, new ItemStack(net.minecraft.world.item.Items.SWEET_BERRIES));
                count--;
            }
            if (count > 0) {
                net.minecraft.world.level.block.Block.popResource(level(), blockPos, new ItemStack(net.minecraft.world.item.Items.SWEET_BERRIES, count));
            }
            level().setBlock(blockPos, state.setValue(net.minecraft.world.level.block.SweetBerryBushBlock.AGE, 1), 2);
        }
        public boolean canUse() {
            return !isSleeping() && super.canUse();
        }
        public void start() {
            ticksWaited = 0;
            setSitting(false);
            super.start();
        }
    }
    public static class FoxGroupData extends AgeableMob.AgeableMobGroupData {
        public final Variant variant;
        public FoxGroupData(Variant variant) {
            super(false);
            this.variant = variant;
        }
    }
    class FaceplantGoal extends Goal {
        private int countdown;
        public FaceplantGoal() {
            setFlags(java.util.EnumSet.of(Goal.Flag.LOOK, Goal.Flag.JUMP, Goal.Flag.MOVE));
        }
        public boolean canUse() {
            return isFaceplanted();
        }
        public boolean canContinueToUse() {
            return canUse() && countdown > 0;
        }
        public void start() {
            countdown = adjustedTickDelay(40);
        }
        public void stop() {
            setFaceplanted(false);
        }
        public void tick() {
            countdown--;
        }
    }
    class FoxPanicGoal extends PanicGoal {
        public FoxPanicGoal(double speedModifier) {
            super(Fox.this, speedModifier);
        }
        public boolean shouldPanic() {
            return !isDefending() && super.shouldPanic();
        }
    }
    class FoxStrollThroughVillageGoal extends StrollThroughVillageGoal {
        public FoxStrollThroughVillageGoal(int searchRadius, int interval) {
            super(Fox.this, interval);
        }
        public void start() {
            clearStates();
            super.start();
        }
        public boolean canUse() {
            return super.canUse() && canFoxMove();
        }
        public boolean canContinueToUse() {
            return super.canContinueToUse() && canFoxMove();
        }
        private boolean canFoxMove() {
            return !isSleeping() && !isSitting() && !isDefending() && getTarget() == null;
        }
    }
    class FoxFloatGoal extends FloatGoal {
        public FoxFloatGoal() {
            super(Fox.this);
        }
        public void start() {
            super.start();
            clearStates();
        }
    }
    public class FoxPounceGoal extends JumpGoal {
        public FoxPounceGoal() {
        }
        public boolean canUse() {
            if (!isFullyCrouched()) {
                return false;
            }
            LivingEntity target = getTarget();
            if (target == null || !target.isAlive() || target.getMotionDirection() != target.getDirection()) {
                return false;
            }
            boolean clear = isPathClear(Fox.this, target);
            if (!clear) {
                getNavigation().createPath(target, 0);
                setIsCrouching(false);
                setIsInterested(false);
            }
            return clear;
        }
        public boolean canContinueToUse() {
            LivingEntity target = getTarget();
            if (target == null || !target.isAlive()) {
                return false;
            }
            double yd = getDeltaMovement().y;
            return (yd * yd >= 0.05 || Math.abs(getXRot()) >= 15.0F || !onGround()) && !isFaceplanted();
        }
        public boolean isInterruptable() {
            return false;
        }
        public void start() {
            setJumping(true);
            setIsPouncing(true);
            setIsInterested(false);
            LivingEntity target = getTarget();
            if (target != null) {
                getLookControl().setLookAt(target, 60.0F, 30.0F);
                Vec3 unit = new Vec3(target.getX() - getX(), target.getY() - getY(), target.getZ() - getZ()).normalize();
                setDeltaMovement(getDeltaMovement().add(unit.x * 0.8, 0.9, unit.z * 0.8));
            }
            getNavigation().stop();
        }
        public void stop() {
            setIsCrouching(false);
            crouchAmount = 0.0F;
            crouchAmountO = 0.0F;
            setIsInterested(false);
            setIsPouncing(false);
        }
        public void tick() {
            LivingEntity target = getTarget();
            if (target != null) {
                getLookControl().setLookAt(target, 60.0F, 30.0F);
            }
            if (!isFaceplanted()) {
                Vec3 motion = getDeltaMovement();
                if (motion.y * motion.y < 0.03 && getXRot() != 0.0F) {
                    setXRot(Mth.rotLerp(0.2F, getXRot(), 0.0F));
                } else {
                    double flat = motion.horizontalDistance();
                    double biasedY = motion.y * (jumping && motion.y > 0.0 ? 6.5F : 1.0F);
                    double length = Math.sqrt(flat * flat + biasedY * biasedY);
                    if (length > 1.0E-5F) {
                        setXRot((float) (Math.signum(-biasedY) * Math.acos(flat / length) * (180.0F / (float) Math.PI)));
                    }
                }
            }
            if (target != null && distanceTo(target) <= 2.0F) {
                doHurtTarget(null, target);
            } else if (getXRot() > 0.0F && onGround() && (float) getDeltaMovement().y != 0.0F
                && level().getBlockState(blockPosition()).is(net.minecraft.world.level.block.Blocks.SNOW)) {
                setXRot(60.0F);
                setTarget(null);
                setFaceplanted(true);
            }
        }
    }
    public class FoxLookControl extends LookControl {
        public FoxLookControl() {
            super(Fox.this);
        }
        public void tick() {
            if (!isSleeping()) {
                super.tick();
            }
        }
        protected boolean resetXRotOnTick() {
            return !isPouncing() && !isCrouching() && !isInterested() && !isFaceplanted();
        }
    }
    static class FoxFollowParentGoal extends FollowParentGoal {
        private final Fox fox;
        public FoxFollowParentGoal(Fox fox, double speedModifier) {
            super(fox, speedModifier);
            this.fox = fox;
        }
        public boolean canUse() {
            return !fox.isDefending() && super.canUse();
        }
        public boolean canContinueToUse() {
            return !fox.isDefending() && super.canContinueToUse();
        }
        public void start() {
            fox.clearStates();
            super.start();
        }
    }
    class FoxLookAtPlayerGoal extends LookAtPlayerGoal {
        public FoxLookAtPlayerGoal(Mob mob, Class<? extends LivingEntity> lookAtType, float lookDistance) {
            super(mob, lookAtType, lookDistance);
        }
        public boolean canUse() {
            return super.canUse() && !isFaceplanted() && !isInterested();
        }
        public boolean canContinueToUse() {
            return super.canContinueToUse() && !isFaceplanted() && !isInterested();
        }
    }
}
