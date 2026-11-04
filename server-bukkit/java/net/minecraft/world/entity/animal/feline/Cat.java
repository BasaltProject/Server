// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.animal.feline;
import net.minecraft.util.RandomSource;
import net.minecraft.world.phys.AABB;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.block.BedBlock;
import net.minecraft.tags.BlockTags;
import net.minecraft.core.BlockPos;
import java.util.function.Predicate;
import net.minecraft.core.Holder;
import net.minecraft.core.component.DataComponentGetter;
import net.minecraft.core.component.DataComponentType;
import net.minecraft.network.syncher.SynchedEntityData;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.sounds.SoundEvent;
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
import net.minecraft.world.entity.TamableAnimal;
import net.minecraft.world.entity.ai.attributes.AttributeSupplier;
import net.minecraft.world.entity.ai.goal.AvoidEntityGoal;
import net.minecraft.world.entity.ai.goal.Goal;
import net.minecraft.world.entity.ai.goal.TemptGoal;
import net.minecraft.world.entity.animal.Animal;
import net.minecraft.world.entity.player.Player;
import net.minecraft.world.item.DyeColor;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.level.Level;
import net.minecraft.world.level.ServerLevelAccessor;
import net.minecraft.world.level.storage.ValueInput;
import net.minecraft.world.level.storage.ValueOutput;
public class Cat extends TamableAnimal {
    public static final double TEMPT_SPEED_MOD = 0.6;
    public static final double WALK_SPEED_MOD = 0.8;
    public static final double SPRINT_SPEED_MOD = 1.33;
    public Cat(EntityType<? extends Cat> type, Level level) {
        super(type, level);
    }
    protected void registerGoals() {
        goalSelector.addGoal(1, new net.minecraft.world.entity.ai.goal.FloatGoal(this));
        goalSelector.addGoal(8, new net.minecraft.world.entity.ai.goal.LeapAtTargetGoal(this, 0.3F));
        goalSelector.addGoal(10, new net.minecraft.world.entity.ai.goal.BreedGoal(this, 0.8));
        goalSelector.addGoal(11, new net.minecraft.world.entity.ai.goal.WaterAvoidingRandomStrollGoal(this, 0.8, 1.0000001E-5F));
        goalSelector.addGoal(12, new net.minecraft.world.entity.ai.goal.LookAtPlayerGoal(this, net.minecraft.world.entity.player.Player.class, 10.0F));
    }
    public Holder<CatVariant> getVariant() {
        return live() instanceof org.bukkit.entity.Cat api
            ? org.basaltmc.bukkit.facade.VariantHolders.of(net.minecraft.core.registries.Registries.CAT_VARIANT, CatVariant.class, api.getCatType()) : null;
    }
    protected void applyImplicitComponents(DataComponentGetter components) {
        }
    protected <T> boolean applyImplicitComponent(DataComponentType<T> type, T value) {
        return false;
    }
    public void setLying(boolean value) {
        if (live() instanceof org.bukkit.entity.Cat api) {
            api.setLyingDown(value);
        }
    }
    public boolean isLying() {
        return live() instanceof org.bukkit.entity.Cat api && api.isLyingDown();
    }
    public DyeColor getCollarColor() {
        return live() instanceof org.bukkit.entity.Cat api ? DyeColor.byId(api.getCollarColor().ordinal()) : DyeColor.RED;
    }
    protected void defineSynchedData(SynchedEntityData.Builder entityData) {
    }
    protected void addAdditionalSaveData(ValueOutput output) {
    }
    protected void readAdditionalSaveData(ValueInput input) {
    }
    public void customServerAiStep(ServerLevel level) {
    }
    protected SoundEvent getAmbientSound() {
        return net.minecraft.sounds.SoundEvents.byId("entity.cat.ambient");
    }
    public int getAmbientSoundInterval() {
        return super.getAmbientSoundInterval();
    }
    public void hiss() {
        playSound(net.minecraft.sounds.SoundEvents.byId("entity.cat.hiss"), getSoundVolume(), getVoicePitch());
    }
    protected SoundEvent getHurtSound(DamageSource source) {
        return net.minecraft.sounds.SoundEvents.byId("entity.cat.hurt");
    }
    protected SoundEvent getDeathSound() {
        return net.minecraft.sounds.SoundEvents.byId("entity.cat.death");
    }
    public static AttributeSupplier.Builder createAttributes() {
        return Animal.createAnimalAttributes()
            .add(net.minecraft.world.entity.ai.attributes.Attributes.MAX_HEALTH, 10.0)
            .add(net.minecraft.world.entity.ai.attributes.Attributes.MOVEMENT_SPEED, 0.3)
            .add(net.minecraft.world.entity.ai.attributes.Attributes.ATTACK_DAMAGE, 3.0);
    }
    public EntityDimensions getDefaultDimensions(Pose pose) {
        return net.minecraft.world.entity.EntityDimensions.scalable(getBbWidth(), getBbHeight());
    }
    protected void playEatingSound() {
        }
    public void tick() {
    }
    public boolean isLyingOnTopOfSleepingPlayer() {
        return isLying() && live().getNearbyEntities(2.0, 2.0, 2.0).stream()
            .anyMatch(near -> near instanceof org.bukkit.entity.HumanEntity human && human.isSleeping());
    }
    public float getLieDownAmount(float a) {
        return isLying() ? 1.0F : 0.0F;
    }
    public float getLieDownAmountTail(float a) {
        return isLying() ? 1.0F : 0.0F;
    }
    public float getRelaxStateOneAmount(float a) {
        return 0.0F;
    }
    public Cat getBreedOffspring(ServerLevel level, AgeableMob partner) {
        return null;
    }
    public boolean canMate(Animal partner) {
        return partner != this && partner.getClass() == getClass() && live() instanceof org.bukkit.entity.Animals selfApi && selfApi.isLoveMode() && partner.live() instanceof org.bukkit.entity.Animals otherApi && otherApi.isLoveMode();
    }
    public SpawnGroupData finalizeSpawn(ServerLevelAccessor level, DifficultyInstance difficulty, EntitySpawnReason spawnReason, SpawnGroupData groupData) {
        return groupData;
    }
    public InteractionResult mobInteract(Player player, InteractionHand hand) {
        return net.minecraft.world.InteractionResult.PASS;
    }
    public boolean isFood(ItemStack itemStack) {
        return live() instanceof org.bukkit.entity.Animals animals && animals.isBreedItem(org.bukkit.craftbukkit.inventory.CraftItemStack.asCraftMirror(itemStack));
    }
    public boolean removeWhenFarAway(double distSqr) {
        return super.removeWhenFarAway(distSqr);
    }
    protected void reassessTameGoals() {
        }
    static class CatAvoidEntityGoal<T extends LivingEntity> extends AvoidEntityGoal<T> {
        public CatAvoidEntityGoal(Cat cat, Class<T> avoidClass, float maxDist, double walkSpeedModifier, double sprintSpeedModifier) {
            super(cat, avoidClass, maxDist, walkSpeedModifier, sprintSpeedModifier);
        }
    }
    static class CatTemptGoal extends TemptGoal {
        public CatTemptGoal(Cat mob, double speedModifier, Predicate<ItemStack> items, boolean canScare) {
            super(mob, speedModifier, items, canScare);
        }
        protected boolean canScare() {
            return false;
        }
    }
    static class CatRelaxOnOwnerGoal extends Goal {
        private final Cat cat;
        private Player ownerPlayer;
        private BlockPos goalPos;
        private int onBedTicks;

        public CatRelaxOnOwnerGoal(Cat cat) {
            this.cat = cat;
        }
        public boolean canUse() {
            if (!cat.isTame() || cat.isOrderedToSit() || !(cat.getOwner() instanceof Player owner)) {
                return false;
            }
            ownerPlayer = owner;
            if (!owner.isSleeping() || cat.distanceToSqr(owner) > 100.0) {
                return false;
            }
            BlockPos ownerPos = owner.blockPosition();
            BlockState ownerState = cat.level().getBlockState(ownerPos);
            if (!ownerState.is(BlockTags.CATS_CAN_LIE_ON)) {
                return false;
            }
            goalPos = ownerState.hasProperty(BedBlock.FACING) ? ownerPos.relative(ownerState.getValue(BedBlock.FACING).getOpposite()) : ownerPos.immutable();
            return !spaceIsOccupied();
        }
        private boolean spaceIsOccupied() {
            for (Cat other : cat.level().getEntitiesOfClass(Cat.class, new AABB(goalPos).inflate(2.0))) {
                if (other != cat && (other.isLying() || other.isRelaxStateOne())) {
                    return true;
                }
            }
            return false;
        }
        public boolean canContinueToUse() {
            return cat.isTame() && !cat.isOrderedToSit() && ownerPlayer != null && ownerPlayer.isSleeping() && goalPos != null && !spaceIsOccupied();
        }
        public void start() {
            if (goalPos != null) {
                cat.setInSittingPose(false);
                cat.getNavigation().moveTo(goalPos.getX(), goalPos.getY(), goalPos.getZ(), 1.1F);
            }
        }
        public void stop() {
            cat.setLying(false);
            if (ownerPlayer.getSleepTimer() >= 100 && cat.level().getRandom().nextFloat() < kWakingGiftChance) {
                giveMorningGift();
            }
            onBedTicks = 0;
            cat.setRelaxStateOne(false);
            cat.getNavigation().stop();
        }
        private void giveMorningGift() {
            RandomSource random = cat.getRandom();
            BlockPos origin = cat.isLeashed() ? cat.getLeashHolder().blockPosition() : cat.blockPosition();
            cat.randomTeleport(origin.getX() + random.nextInt(11) - 5, origin.getY() + random.nextInt(5) - 2, origin.getZ() + random.nextInt(11) - 5,
                false, BlockTags.CAT_DOES_NOT_TELEPORT_TO);
            org.bukkit.entity.Cat api = (org.bukkit.entity.Cat) cat.live();
            org.bukkit.loot.LootTable table = org.bukkit.Bukkit.getLootTable(org.bukkit.NamespacedKey.minecraft("gameplay/cat_morning_gift"));
            if (api == null || table == null) {
                return;
            }
            org.bukkit.Location at = api.getLocation();
            org.bukkit.loot.LootContext context = new org.bukkit.loot.LootContext.Builder(at).lootedEntity(api).build();
            double yaw = Math.toRadians(api.getBodyYaw());
            for (org.bukkit.inventory.ItemStack stack : table.populateLoot(new java.util.Random(), context)) {
                at.getWorld().dropItem(at.clone().add(-Math.sin(yaw), 0.0, Math.cos(yaw)), stack);
            }
        }
        public void tick() {
            if (ownerPlayer == null || goalPos == null) {
                return;
            }
            cat.setInSittingPose(false);
            cat.getNavigation().moveTo(goalPos.getX(), goalPos.getY(), goalPos.getZ(), 1.1F);
            if (cat.distanceToSqr(ownerPlayer) >= 2.5) {
                cat.setLying(false);
                return;
            }
            if (++onBedTicks > adjustedTickDelay(16)) {
                cat.setLying(true);
                cat.setRelaxStateOne(false);
            } else {
                cat.lookAt(ownerPlayer, 45.0F, 45.0F);
                cat.setRelaxStateOne(true);
            }
        }
    }
    private static final float kWakingGiftChance = 0.7F;

    public boolean isRelaxStateOne() {
        return live() instanceof org.bukkit.entity.Cat api && api.isHeadUp();
    }
    public void setRelaxStateOne(boolean value) {
        if (live() instanceof org.bukkit.entity.Cat api) {
            api.setHeadUp(value);
        }
    }

}
