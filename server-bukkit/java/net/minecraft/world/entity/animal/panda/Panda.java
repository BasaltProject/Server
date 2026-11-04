// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.animal.panda;
import net.minecraft.world.entity.ai.attributes.Attributes;
import com.mojang.serialization.Codec;
import net.minecraft.core.BlockPos;
import net.minecraft.network.syncher.SynchedEntityData;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.sounds.SoundEvent;
import net.minecraft.util.RandomSource;
import net.minecraft.util.StringRepresentable;
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
import net.minecraft.world.entity.LivingEntity;
import net.minecraft.world.entity.Mob;
import net.minecraft.world.entity.Pose;
import net.minecraft.world.entity.SpawnGroupData;
import net.minecraft.world.entity.ai.attributes.AttributeSupplier;
import net.minecraft.world.entity.ai.control.MoveControl;
import net.minecraft.world.entity.ai.goal.AvoidEntityGoal;
import net.minecraft.world.entity.ai.goal.BreedGoal;
import net.minecraft.world.entity.ai.goal.Goal;
import net.minecraft.world.entity.ai.goal.LookAtPlayerGoal;
import net.minecraft.world.entity.ai.goal.MeleeAttackGoal;
import net.minecraft.world.entity.ai.goal.PanicGoal;
import net.minecraft.world.entity.ai.goal.target.HurtByTargetGoal;
import net.minecraft.world.entity.animal.Animal;
import net.minecraft.world.entity.item.ItemEntity;
import net.minecraft.world.entity.player.Player;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.level.Level;
import net.minecraft.world.level.ServerLevelAccessor;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.storage.ValueInput;
import net.minecraft.world.level.storage.ValueOutput;
public class Panda extends Animal {
    public static final int TOTAL_ROLL_STEPS = 32;
    public int rollCounter;
    public Panda(EntityType<? extends Panda> type, Level level) {
        super(type, level);
        this.moveControl = new PandaMoveControl<>(this);
    }
    protected boolean canDispenserEquipIntoSlot(EquipmentSlot slot) {
        return false;
    }
    public int getUnhappyCounter() {
        return live() instanceof org.bukkit.entity.Panda api ? api.getUnhappyTicks() : 0;
    }
    public void setUnhappyCounter(int value) {
        if (live() instanceof org.bukkit.entity.Panda api) { api.setUnhappyTicks(value); }
    }
    public boolean isSneezing() {
        return live() instanceof org.bukkit.entity.Panda api ? (boolean) api.isSneezing() : false;
    }
    public boolean isSitting() {
        return live() instanceof org.bukkit.entity.Panda api ? (boolean) api.isSitting() : false;
    }
    public void sit(boolean value) {
        if (live() instanceof org.bukkit.entity.Panda api) { api.setSitting(value); }
    }
    public boolean isOnBack() {
        return live() instanceof org.bukkit.entity.Panda api ? (boolean) api.isOnBack() : false;
    }
    public void setOnBack(boolean value) {
        if (live() instanceof org.bukkit.entity.Panda api) { api.setOnBack(value); }
    }
    public boolean isEating() {
        return live() instanceof org.bukkit.entity.Panda api ? (boolean) api.isEating() : false;
    }
    public void eat(boolean value) {
        if (live() instanceof org.bukkit.entity.Panda api) { api.setEating(value); }
    }
    public void sneeze(boolean value) {
        if (live() instanceof org.bukkit.entity.Panda api) { api.setSneezing(value); }
    }
    public int getSneezeCounter() {
        return live() instanceof org.bukkit.entity.Panda api ? api.getSneezeTicks() : 0;
    }
    public void setSneezeCounter(int value) {
        if (live() instanceof org.bukkit.entity.Panda api) { api.setSneezeTicks(value); }
    }
    public Gene getMainGene() {
        return Gene.values()[live() instanceof org.bukkit.entity.Panda api ? api.getMainGene().ordinal() : 0];
    }
    public void setMainGene(Gene gene) {
        if (live() instanceof org.bukkit.entity.Panda api) { api.setMainGene(org.bukkit.entity.Panda.Gene.values()[gene.ordinal()]); }
    }
    public Gene getHiddenGene() {
        return Gene.values()[live() instanceof org.bukkit.entity.Panda api ? api.getHiddenGene().ordinal() : 0];
    }
    public void setHiddenGene(Gene gene) {
        if (live() instanceof org.bukkit.entity.Panda api) { api.setHiddenGene(org.bukkit.entity.Panda.Gene.values()[gene.ordinal()]); }
    }
    public boolean isRolling() {
        return live() instanceof org.bukkit.entity.Panda api ? (boolean) api.isRolling() : false;
    }
    public void roll(boolean value) {
        if (live() instanceof org.bukkit.entity.Panda api) { api.setRolling(value); }
    }
    protected void defineSynchedData(SynchedEntityData.Builder entityData) {
    }
    protected void addAdditionalSaveData(ValueOutput output) {
    }
    protected void readAdditionalSaveData(ValueInput input) {
    }
    public AgeableMob getBreedOffspring(ServerLevel level, AgeableMob partner) {
        return null;
    }
    protected void registerGoals() {
    }
    public static AttributeSupplier.Builder createAttributes() {
        return Animal.createAnimalAttributes()
            .add(net.minecraft.world.entity.ai.attributes.Attributes.MOVEMENT_SPEED, 0.15)
            .add(net.minecraft.world.entity.ai.attributes.Attributes.ATTACK_DAMAGE, 6.0);
    }
    public Gene getVariant() {
        return Gene.values()[live() instanceof org.bukkit.entity.Panda api ? api.getCombinedGene().ordinal() : 0];
    }
    public boolean isLazy() {
        return getVariant() == Gene.LAZY;
    }
    public boolean isWorried() {
        return getVariant() == Gene.WORRIED;
    }
    public boolean isPlayful() {
        return getVariant() == Gene.PLAYFUL;
    }
    public boolean isBrown() {
        return getVariant() == Gene.BROWN;
    }
    public boolean isWeak() {
        return getVariant() == Gene.WEAK;
    }
    public boolean isAggressive() {
        return live() instanceof org.bukkit.entity.Panda api ? (boolean) api.isAggressive() : false;
    }
    public boolean canBeLeashed() {
        return super.canBeLeashed();
    }
    public boolean doHurtTarget(ServerLevel level, Entity target) {
        return super.doHurtTarget(level, target);
    }
    public void playAttackSound() {
    }
    public void tick() {
    }
    public boolean isScared() {
        return live() instanceof org.bukkit.entity.Panda api ? (boolean) api.isScared() : false;
    }
    public float getSitAmount(float a) {
        return isSitting() ? 1.0F : 0.0F;
    }
    public float getLieOnBackAmount(float a) {
        return live() instanceof org.bukkit.entity.Panda api && api.isOnBack() ? 1.0F : 0.0F;
    }
    public float getRollAmount(float a) {
        return live() instanceof org.bukkit.entity.Panda api && api.isRolling() ? 1.0F : 0.0F;
    }
    protected void pickUpItem(ServerLevel level, ItemEntity entity) {
        }
    public boolean hurtServer(ServerLevel level, DamageSource source, float damage) {
        return super.hurtServer(level, source, damage);
    }
    public SpawnGroupData finalizeSpawn(ServerLevelAccessor level, DifficultyInstance difficulty, EntitySpawnReason spawnReason, SpawnGroupData groupData) {
        return groupData;
    }
    public void setGeneFromParents(Panda parent1, Panda parent2) {
        Gene first = random.nextBoolean() ? parent1.getMainGene() : parent1.getHiddenGene();
        Gene second = random.nextBoolean() ? parent2.getMainGene() : parent2.getHiddenGene();
        if (random.nextBoolean()) {
            setMainGene(first);
            setHiddenGene(second);
        } else {
            setMainGene(second);
            setHiddenGene(first);
        }
        if (random.nextInt(32) == 0) {
            setMainGene(Gene.getRandom(random));
        }
        if (random.nextInt(32) == 0) {
            setHiddenGene(Gene.getRandom(random));
        }
    }
    public void setAttributes() {
        if (isWeak()) {
            getAttribute(Attributes.MAX_HEALTH).setBaseValue(10.0);
        }
        if (getVariant() == Gene.AGGRESSIVE) {
            getAttribute(Attributes.ATTACK_DAMAGE).setBaseValue(6.0);
        }
    }
    public InteractionResult mobInteract(Player player, InteractionHand hand) {
        return net.minecraft.world.InteractionResult.PASS;
    }
    protected SoundEvent getAmbientSound() {
        return net.minecraft.sounds.SoundEvents.byId("entity.panda.ambient");
    }
    protected void playStepSound(BlockPos pos, BlockState blockState) {
        playSound(net.minecraft.sounds.SoundEvents.byId("entity.panda.step"), 0.15F, 1.0F);
    }
    public boolean isFood(ItemStack itemStack) {
        return live() instanceof org.bukkit.entity.Animals animals && animals.isBreedItem(org.bukkit.craftbukkit.inventory.CraftItemStack.asCraftMirror(itemStack));
    }
    protected SoundEvent getDeathSound() {
        return net.minecraft.sounds.SoundEvents.byId("entity.panda.death");
    }
    protected SoundEvent getHurtSound(DamageSource source) {
        return net.minecraft.sounds.SoundEvents.byId("entity.panda.hurt");
    }
    public boolean canPerformAction() {
        return live() instanceof org.bukkit.entity.Panda api && !api.isOnBack() && !api.isScared() && !api.isEating()
            && !api.isRolling() && !api.isSitting();
    }
    public EntityDimensions getDefaultDimensions(Pose pose) {
        return net.minecraft.world.entity.EntityDimensions.scalable(getBbWidth(), getBbHeight());
    }
    public static enum Gene implements StringRepresentable {
        NORMAL, LAZY, WORRIED, PLAYFUL, BROWN, WEAK, AGGRESSIVE;
        public static final Codec<Gene> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.entity.animal.panda.Panda.Gene.class, "CODEC");
        public int getId() {
            return ordinal();
        }
        public String getSerializedName() {
            return name().toLowerCase(java.util.Locale.ROOT);
        }
        public boolean isRecessive() {
            return this == NORMAL || this == BROWN || this == WEAK;
        }
        public static Gene byId(int id) {
            return values()[Math.floorMod(id, values().length)];
        }
        public static Gene getRandom(RandomSource random) {
            return Gene.values()[random.nextInt(Gene.values().length)];
        }
    }
    static class PandaMoveControl<T extends Panda> extends MoveControl<T> {
        public PandaMoveControl(T mob) {
            super(mob);
        }
        public void tick() {
            if (mob.canPerformAction()) {
                super.tick();
            }
        }
    }
    static class PandaAttackGoal extends MeleeAttackGoal {
        private final Panda panda;
        public PandaAttackGoal(Panda mob, double speedModifier, boolean trackTarget) {
            super(mob, speedModifier, trackTarget);
            this.panda = mob;
        }
        public boolean canUse() {
            return panda.canPerformAction() && super.canUse();
        }
    }
    static class PandaLookAtPlayerGoal extends LookAtPlayerGoal {
        private final Panda panda;
        public PandaLookAtPlayerGoal(Panda mob, Class<? extends LivingEntity> lookAtType, float lookDistance) {
            super(mob, lookAtType, lookDistance);
            this.panda = mob;
        }
        public boolean canUse() {
            return panda.canPerformAction() && super.canUse();
        }
    }
    static class PandaRollGoal extends Goal {
        private final Panda panda;
        public PandaRollGoal(Panda panda) {
            this.panda = panda;
            setFlags(java.util.EnumSet.of(Goal.Flag.MOVE, Goal.Flag.LOOK, Goal.Flag.JUMP));
        }
        public boolean canUse() {
            return (panda.isBaby() || panda.isPlayful()) && panda.onGround() && panda.canPerformAction()
                && panda.getRandom().nextInt(reducedTickDelay(60)) == 0;
        }
        public boolean canContinueToUse() {
            return false;
        }
        public void start() {
            panda.roll(true);
        }
        public boolean isInterruptable() {
            return false;
        }
    }
    static class PandaSneezeGoal extends Goal {
        private final Panda panda;
        public PandaSneezeGoal(Panda panda) {
            this.panda = panda;
        }
        public boolean canUse() {
            return panda.isBaby() && panda.canPerformAction() && panda.getRandom().nextInt(reducedTickDelay(500)) == 0;
        }
        public boolean canContinueToUse() {
            return false;
        }
        public void start() {
            panda.sneeze(true);
        }
    }
    static class PandaBreedGoal extends BreedGoal {
        private final Panda panda;
        public PandaBreedGoal(Panda panda, double speedModifier) {
            super(panda, speedModifier);
            this.panda = panda;
        }
        public boolean canUse() {
            return super.canUse() && panda.getUnhappyCounter() == 0;
        }
    }
    static class PandaAvoidGoal<T extends LivingEntity> extends AvoidEntityGoal<T> {
        private final Panda panda;
        public PandaAvoidGoal(Panda panda, Class<T> avoidClass, float maxDist, double walkSpeedModifier, double sprintSpeedModifier) {
            super(panda, avoidClass, maxDist, walkSpeedModifier, sprintSpeedModifier);
            this.panda = panda;
        }
        public boolean canUse() {
            return panda.isWorried() && panda.canPerformAction() && super.canUse();
        }
    }
    class PandaSitGoal extends Goal {
        private int cooldown;
        public PandaSitGoal() {
            setFlags(java.util.EnumSet.of(Goal.Flag.MOVE));
        }
        private boolean foodNearby() {
            return !level().getEntitiesOfClass(ItemEntity.class, getBoundingBox().inflate(6.0, 6.0, 6.0),
                item -> item.getItem().is(net.minecraft.world.item.Items.BAMBOO) || item.getItem().is(net.minecraft.world.item.Items.CAKE)).isEmpty();
        }
        public boolean canUse() {
            if (cooldown > tickCount || isBaby() || isInWater() || !canPerformAction() || getUnhappyCounter() > 0) {
                return false;
            }
            return foodNearby() || isSitting();
        }
        public boolean canContinueToUse() {
            return foodNearby() && isSitting();
        }
        public void tick() {
            if (!isSitting() && foodNearby()) {
                sit(true);
            }
        }
        public void start() {
            sit(true);
            getNavigation().stop();
        }
        public void stop() {
            sit(false);
            cooldown = tickCount + 600;
        }
    }
    static class PandaLieOnBackGoal extends Goal {
        private final Panda panda;
        private int cooldown;
        public PandaLieOnBackGoal(Panda panda) {
            this.panda = panda;
            setFlags(java.util.EnumSet.of(Goal.Flag.MOVE, Goal.Flag.LOOK, Goal.Flag.JUMP));
        }
        public boolean canUse() {
            return cooldown < panda.tickCount && panda.isLazy() && panda.canPerformAction() && panda.getRandom().nextInt(reducedTickDelay(400)) == 0;
        }
        public boolean canContinueToUse() {
            return panda.live() instanceof org.bukkit.entity.Panda api && api.isOnBack() && panda.getRandom().nextInt(reducedTickDelay(80)) != 0;
        }
        public void start() {
            if (panda.live() instanceof org.bukkit.entity.Panda api) {
                api.setOnBack(true);
            }
            panda.getNavigation().stop();
        }
        public void stop() {
            if (panda.live() instanceof org.bukkit.entity.Panda api) {
                api.setOnBack(false);
            }
            cooldown = panda.tickCount + 200;
        }
    }
    static class PandaHurtByTargetGoal extends HurtByTargetGoal {
        public PandaHurtByTargetGoal(Panda mob, Class<?>... ignoreDamageFromTheseTypes) {
            super(mob, ignoreDamageFromTheseTypes);
        }
        protected void alertOther(Mob other, LivingEntity hurtByMob) {
            if (other instanceof Panda panda && panda.getVariant() == Gene.AGGRESSIVE) {
                super.alertOther(other, hurtByMob);
            }
        }
    }
    static class PandaPanicGoal extends PanicGoal {
        public PandaPanicGoal(Panda mob, double speedModifier) {
            super(mob, speedModifier);
        }
    }
}
