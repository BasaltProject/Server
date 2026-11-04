// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.animal.dolphin;
import net.minecraft.world.entity.ai.util.DefaultRandomPos;
import net.minecraft.core.BlockPos;
import java.util.function.Predicate;
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
import net.minecraft.world.entity.EquipmentSlot;
import net.minecraft.world.entity.LivingEntity;
import net.minecraft.world.entity.Pose;
import net.minecraft.world.entity.SpawnGroupData;
import net.minecraft.world.entity.ai.attributes.AttributeSupplier;
import net.minecraft.world.entity.ai.goal.Goal;
import net.minecraft.world.entity.ai.navigation.PathNavigation;
import net.minecraft.world.entity.animal.AgeableWaterCreature;
import net.minecraft.world.entity.item.ItemEntity;
import net.minecraft.world.entity.player.Player;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.level.Level;
import net.minecraft.world.level.ServerLevelAccessor;
import net.minecraft.world.level.storage.ValueInput;
import net.minecraft.world.level.storage.ValueOutput;
import net.minecraft.world.phys.Vec3;
public class Dolphin extends AgeableWaterCreature {
    public static final int TOTAL_AIR_SUPPLY = 4800;
    public static final Predicate<ItemEntity> ALLOWED_ITEMS = item -> !item.hasPickUpDelay() && item.isAlive() && item.isInWater();
    public static final float BABY_SCALE = 0.65F;
    public Dolphin(EntityType<? extends Dolphin> type, Level level) {
        super(type, level);
    }
    public SpawnGroupData finalizeSpawn(ServerLevelAccessor level, DifficultyInstance difficulty, EntitySpawnReason spawnReason, SpawnGroupData groupData) {
        return groupData;
    }
    public Dolphin getBreedOffspring(ServerLevel level, AgeableMob partner) {
        return null;
    }
    protected void handleAirSupply(int preTickAirSupply) {
        }
    public boolean gotFish() {
        return live() instanceof org.bukkit.entity.Dolphin api && api.hasFish();
    }
    public void setGotFish(boolean gotFish) {
        if (live() instanceof org.bukkit.entity.Dolphin api) {
            api.setHasFish(gotFish);
        }
    }
    public int getMoistnessLevel() {
        return live() instanceof org.bukkit.entity.Dolphin api ? api.getMoistness() : 2400;
    }
    public void setMoisntessLevel(int level) {
        if (live() instanceof org.bukkit.entity.Dolphin api) {
            api.setMoistness(level);
        }
    }
    protected void defineSynchedData(SynchedEntityData.Builder entityData) {
    }
    protected void addAdditionalSaveData(ValueOutput output) {
    }
    protected void readAdditionalSaveData(ValueInput input) {
    }
    protected void registerGoals() {
        goalSelector.addGoal(4, new net.minecraft.world.entity.ai.goal.RandomLookAroundGoal(this));
        goalSelector.addGoal(5, new net.minecraft.world.entity.ai.goal.LookAtPlayerGoal(this, net.minecraft.world.entity.player.Player.class, 6.0F));
        goalSelector.addGoal(6, new net.minecraft.world.entity.ai.goal.MeleeAttackGoal(this, 1.2, true));
    }
    public static AttributeSupplier.Builder createAttributes() {
        return net.minecraft.world.entity.Mob.createMobAttributes()
            .add(net.minecraft.world.entity.ai.attributes.Attributes.MAX_HEALTH, 10.0)
            .add(net.minecraft.world.entity.ai.attributes.Attributes.MOVEMENT_SPEED, 1.2)
            .add(net.minecraft.world.entity.ai.attributes.Attributes.ATTACK_DAMAGE, 3.0);
    }
    protected PathNavigation createNavigation(Level level) {
        return super.createNavigation(level);
    }
    public void playAttackSound() {
    }
    protected int increaseAirSupply(int currentSupply) {
        return 0;
    }
    public int getMaxHeadXRot() {
        return super.getMaxHeadXRot();
    }
    public int getMaxHeadYRot() {
        return super.getMaxHeadYRot();
    }
    protected boolean canDispenserEquipIntoSlot(EquipmentSlot slot) {
        return false;
    }
    protected void pickUpItem(ServerLevel level, ItemEntity entity) {
        }
    public void tick() {
    }
    public void handleEntityEvent(byte id) {
    }
    protected InteractionResult mobInteract(Player player, InteractionHand hand) {
        return net.minecraft.world.InteractionResult.PASS;
    }
    protected SoundEvent getHurtSound(DamageSource source) {
        return net.minecraft.sounds.SoundEvents.byId("entity.dolphin.hurt");
    }
    protected SoundEvent getDeathSound() {
        return net.minecraft.sounds.SoundEvents.byId("entity.dolphin.death");
    }
    protected SoundEvent getAmbientSound() {
        return net.minecraft.sounds.SoundEvents.byId("entity.dolphin.ambient");
    }
    protected SoundEvent getSwimSplashSound() {
        return null;
    }
    protected SoundEvent getSwimSound() {
        return null;
    }
    protected boolean closeToNextPos() {
        BlockPos target = getNavigation().getTargetPos();
        return target != null && target.closerToCenterThan(position(), 12.0);
    }
    public EntityDimensions getDefaultDimensions(Pose pose) {
        return net.minecraft.world.entity.EntityDimensions.scalable(getBbWidth(), getBbHeight());
    }
    protected void travelInWater(Vec3 input, double baseGravity, boolean isFalling, double oldY) {
        }
    public boolean canBeLeashed() {
        return super.canBeLeashed();
    }
    abstract class ItemGoal extends Goal {
        private ItemGoal() {
        }
        protected void drop(ItemStack itemStack) {
            if (itemStack.isEmpty() || !(live() instanceof org.bukkit.entity.Dolphin api)) {
                return;
            }
            org.bukkit.Location where = api.getEyeLocation();
            api.getWorld().dropItem(where, org.bukkit.craftbukkit.inventory.CraftItemStack.asCraftMirror(itemStack)).setVelocity(
                where.getDirection().multiply(0.3));
        }
        protected boolean dropItem() {
            ItemStack held = getItemBySlot(net.minecraft.world.entity.EquipmentSlot.MAINHAND);
            if (held.isEmpty()) {
                return false;
            }
            drop(held);
            setItemSlot(net.minecraft.world.entity.EquipmentSlot.MAINHAND, ItemStack.EMPTY);
            return true;
        }
    }
    class MoveToItemGoal extends ItemGoal {
        public MoveToItemGoal() {
            setFlags(java.util.EnumSet.of(Goal.Flag.MOVE));
        }
        private java.util.List<ItemEntity> items() {
            return level().getEntitiesOfClass(ItemEntity.class, getBoundingBox().inflate(8.0, 8.0, 8.0), ALLOWED_ITEMS);
        }
        public boolean canUse() {
            return getItemBySlot(net.minecraft.world.entity.EquipmentSlot.MAINHAND).isEmpty() && !items().isEmpty();
        }
        public void stop() {
            getNavigation().stop();
        }
        public void start() {
            java.util.List<ItemEntity> items = items();
            if (!items.isEmpty()) {
                getNavigation().moveTo(items.get(0), 1.2F);
            }
        }
        public void tick() {
            java.util.List<ItemEntity> items = items();
            if (!items.isEmpty() && getNavigation().isDone()) {
                getNavigation().moveTo(items.get(0), 1.2F);
            }
        }
    }
    class PlayWithItemsGoal extends ItemGoal {
        private int cooldown;
        private PlayWithItemsGoal() {
        }
        public boolean canUse() {
            return cooldown <= tickCount && !getItemBySlot(net.minecraft.world.entity.EquipmentSlot.MAINHAND).isEmpty();
        }
        public void stop() {
            dropItem();
            cooldown = tickCount + 100;
        }
        public void tick() {
            if (random.nextInt(adjustedTickDelay(40)) == 0) {
                dropItem();
            }
        }
    }
    static class DolphinSwimWithPlayerGoal extends Goal {
        private final Dolphin dolphin;
        private final double speedModifier;
        private Player player;
        public DolphinSwimWithPlayerGoal(Dolphin dolphin, double speedModifier) {
            this.dolphin = dolphin;
            this.speedModifier = speedModifier;
            setFlags(java.util.EnumSet.of(Goal.Flag.MOVE, Goal.Flag.LOOK));
        }
        public boolean canUse() {
            player = dolphin.level().getNearestPlayer(dolphin, 10.0);
            return player != null && !player.isSpectator() && player.isSwimming();
        }
        public boolean canContinueToUse() {
            return player != null && player.isSwimming() && !player.isSpectator() && dolphin.distanceToSqr(player) < 256.0;
        }
        public void start() {
        }
        public void stop() {
            player = null;
            dolphin.getNavigation().stop();
        }
        public void tick() {
            dolphin.getLookControl().setLookAt(player, dolphin.getMaxHeadYRot() + 20, dolphin.getMaxHeadXRot());
            if (dolphin.distanceToSqr(player) < 6.25) {
                dolphin.getNavigation().stop();
            } else {
                dolphin.getNavigation().moveTo(player, speedModifier);
            }
        }
    }
    static class DolphinSwimToTreasureGoal extends Goal {
        private final Dolphin dolphin;
        private boolean stuck;

        public DolphinSwimToTreasureGoal(Dolphin dolphin) {
            this.dolphin = dolphin;
            setFlags(java.util.EnumSet.of(Goal.Flag.MOVE, Goal.Flag.LOOK));
        }
        public boolean isInterruptable() {
            return false;
        }
        public boolean canUse() {
            return dolphin.gotFish() && dolphin.getAirSupply() >= 100;
        }
        public boolean canContinueToUse() {
            BlockPos treasure = dolphin.treasurePos;
            return treasure != null && !stuck && dolphin.getAirSupply() >= 100 && !reached(treasure);
        }
        private boolean reached(BlockPos treasure) {
            return BlockPos.containing(treasure.getX(), dolphin.getY(), treasure.getZ()).closerToCenterThan(dolphin.position(), 4.0);
        }
        public void start() {
            if (dolphin.level() instanceof ServerLevel level) {
                stuck = false;
                dolphin.getNavigation().stop();
                BlockPos found = level.findNearestMapStructure(net.minecraft.tags.StructureTags.DOLPHIN_LOCATED, dolphin.blockPosition(), 50, false);
                if (found == null) {
                    stuck = true;
                } else {
                    dolphin.treasurePos = found;
                    level.broadcastEntityEvent(dolphin, (byte) 38);
                }
            }
        }
        public void stop() {
            BlockPos treasure = dolphin.treasurePos;
            if (treasure == null || reached(treasure) || stuck) {
                dolphin.setGotFish(false);
            }
        }
        public void tick() {
            if (dolphin.treasurePos == null) {
                return;
            }
            Level level = dolphin.level();
            if (!dolphin.closeToNextPos() && !dolphin.getNavigation().isDone()) {
                return;
            }
            Vec3 center = Vec3.atCenterOf(dolphin.treasurePos);
            Vec3 next = DefaultRandomPos.getPosTowards(dolphin, 16, 1, center, Math.PI / 8);
            if (next == null) {
                next = DefaultRandomPos.getPosTowards(dolphin, 8, 4, center, Math.PI / 2);
            }
            if (next != null) {
                BlockPos nextBlock = BlockPos.containing(next);
                if (!level.getFluidState(nextBlock).is(net.minecraft.tags.FluidTags.WATER)
                    || !level.getBlockState(nextBlock).isPathfindable(net.minecraft.world.level.pathfinder.PathComputationType.WATER)) {
                    next = DefaultRandomPos.getPosTowards(dolphin, 8, 5, center, Math.PI / 2);
                }
            }
            if (next == null) {
                stuck = true;
                return;
            }
            dolphin.getLookControl().setLookAt(next.x, next.y, next.z, dolphin.getMaxHeadYRot() + 20, dolphin.getMaxHeadXRot());
            dolphin.getNavigation().moveTo(next.x, next.y, next.z, 1.3);
            if (level.getRandom().nextInt(adjustedTickDelay(80)) == 0) {
                level.broadcastEntityEvent(dolphin, (byte) 38);
            }
        }
    }
    BlockPos treasurePos;

}
