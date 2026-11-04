// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.animal.equine;
import com.mojang.serialization.Codec;
import io.netty.buffer.ByteBuf;
import net.minecraft.core.BlockPos;
import net.minecraft.core.component.DataComponentGetter;
import net.minecraft.core.component.DataComponentType;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.network.syncher.SynchedEntityData;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.sounds.SoundEvent;
import net.minecraft.util.StringRepresentable;
import net.minecraft.world.DifficultyInstance;
import net.minecraft.world.damagesource.DamageSource;
import net.minecraft.world.entity.AgeableMob;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.entity.EntityDimensions;
import net.minecraft.world.entity.EntitySpawnReason;
import net.minecraft.world.entity.EntityType;
import net.minecraft.world.entity.EquipmentSlot;
import net.minecraft.world.entity.LivingEntity;
import net.minecraft.world.entity.Pose;
import net.minecraft.world.entity.SpawnGroupData;
import net.minecraft.world.entity.ai.attributes.AttributeSupplier;
import net.minecraft.world.entity.ai.goal.target.HurtByTargetGoal;
import net.minecraft.world.entity.ai.goal.target.NearestAttackableTargetGoal;
import net.minecraft.world.entity.animal.Animal;
import net.minecraft.world.entity.animal.wolf.Wolf;
import net.minecraft.world.entity.monster.RangedAttackMob;
import net.minecraft.world.entity.player.Player;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.level.Level;
import net.minecraft.world.level.ServerLevelAccessor;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.storage.ValueInput;
import net.minecraft.world.level.storage.ValueOutput;
import net.minecraft.world.phys.Vec3;
public class Llama extends AbstractChestedHorse implements RangedAttackMob {
    public Llama(EntityType<? extends Llama> type, Level level) {
        super(type, level);
    }
    public boolean isTraderLlama() {
        return getType() == net.minecraft.world.entity.EntityTypes.TRADER_LLAMA;
    }
    public int getStrength() {
        return live() instanceof org.bukkit.entity.Llama api ? (int) api.getStrength() : 0;
    }
    protected void addAdditionalSaveData(ValueOutput output) {
    }
    protected void readAdditionalSaveData(ValueInput input) {
    }
    protected void registerGoals() {
        goalSelector.addGoal(0, new net.minecraft.world.entity.ai.goal.FloatGoal(this));
        goalSelector.addGoal(3, new net.minecraft.world.entity.ai.goal.RangedAttackGoal(this, 1.25, 40, 20.0F));
        goalSelector.addGoal(5, new net.minecraft.world.entity.ai.goal.BreedGoal(this, 1.0));
        goalSelector.addGoal(6, new net.minecraft.world.entity.ai.goal.FollowParentGoal(this, 1.0));
        goalSelector.addGoal(7, new net.minecraft.world.entity.ai.goal.WaterAvoidingRandomStrollGoal(this, 0.7));
        goalSelector.addGoal(8, new net.minecraft.world.entity.ai.goal.LookAtPlayerGoal(this, Player.class, 6.0F));
        goalSelector.addGoal(9, new net.minecraft.world.entity.ai.goal.RandomLookAroundGoal(this));
        targetSelector.addGoal(1, new LlamaHurtByTargetGoal(this));
        targetSelector.addGoal(2, new LlamaAttackWolfGoal(this));
    }
    public static AttributeSupplier.Builder createAttributes() {
        return net.minecraft.world.entity.Mob.createMobAttributes();
    }
    protected void defineSynchedData(SynchedEntityData.Builder entityData) {
    }
    public Variant getVariant() {
        return Variant.values()[live() instanceof org.bukkit.entity.Llama api ? api.getColor().ordinal() : 0];
    }
    protected void applyImplicitComponents(DataComponentGetter components) {
        }
    protected <T> boolean applyImplicitComponent(DataComponentType<T> type, T value) {
        return false;
    }
    public boolean isFood(ItemStack itemStack) {
        return live() instanceof org.bukkit.entity.Animals animals && animals.isBreedItem(org.bukkit.craftbukkit.inventory.CraftItemStack.asCraftMirror(itemStack));
    }
    protected boolean handleEating(Player player, ItemStack itemStack) {
        return false;
    }
    public boolean isImmobile() {
        return isDeadOrDying();
    }
    public SpawnGroupData finalizeSpawn(ServerLevelAccessor level, DifficultyInstance difficulty, EntitySpawnReason spawnReason, SpawnGroupData groupData) {
        return groupData;
    }
    protected boolean canPerformRearing() {
        return false;
    }
    protected SoundEvent getAngrySound() {
        return null;
    }
    protected SoundEvent getAmbientSound() {
        return net.minecraft.sounds.SoundEvents.byId("entity.llama.ambient");
    }
    protected SoundEvent getHurtSound(DamageSource source) {
        return net.minecraft.sounds.SoundEvents.byId("entity.llama.hurt");
    }
    protected SoundEvent getDeathSound() {
        return net.minecraft.sounds.SoundEvents.byId("entity.llama.death");
    }
    protected SoundEvent getEatingSound() {
        return null;
    }
    protected void playStepSound(BlockPos pos, BlockState blockState) {
        playSound(net.minecraft.sounds.SoundEvents.byId("entity.llama.step"), 0.15F, 1.0F);
    }
    protected void playChestEquipsSound() {
        }
    public int getInventoryColumns() {
        return live() instanceof org.bukkit.entity.Llama api ? api.getStrength() : 0;
    }
    public boolean canUseSlot(EquipmentSlot slot) {
        return super.canUseSlot(slot);
    }
    public int getMaxTemper() {
        return 30;
    }
    public boolean canMate(Animal partner) {
        return partner != this && partner.getClass() == getClass() && live() instanceof org.bukkit.entity.Animals selfApi && selfApi.isLoveMode() && partner.live() instanceof org.bukkit.entity.Animals otherApi && otherApi.isLoveMode();
    }
    public Llama getBreedOffspring(ServerLevel level, AgeableMob partner) {
        return null;
    }
    protected Llama makeNewLlama() {
        return (Llama) getType().create(level(), net.minecraft.world.entity.EntitySpawnReason.BREEDING);
    }
    public void leaveCaravan() {
        if (caravanHead != null) {
            caravanHead.caravanTail = null;
        }
        caravanHead = null;
    }
    public void joinCaravan(Llama tail) {
        caravanHead = tail;
        caravanHead.caravanTail = this;
    }
    public boolean hasCaravanTail() {
        return caravanTail != null;
    }
    public boolean inCaravan() {
        return caravanHead != null;
    }
    private Llama caravanHead;
    private Llama caravanTail;
    boolean didSpit;
    void setDidSpit(boolean value) {
        didSpit = value;
    }
    public Llama getCaravanHead() {
        return caravanHead;
    }
    protected double followLeashSpeed() {
        return 0;
    }
    public boolean supportQuadLeash() {
        return false;
    }
    protected void followMommy(ServerLevel level) {
        }
    public boolean canEatGrass() {
        return false;
    }
    public void performRangedAttack(LivingEntity target, float power) {
        if (live() instanceof com.destroystokyo.paper.entity.RangedEntity ranged && target.live() instanceof org.bukkit.entity.LivingEntity victim) { ranged.rangedAttack(victim, power); }
    }
    public float rangedAttackUncertainty(Level level) {
        return 0.0F;
    }
    public Vec3 getLeashOffset() {
        return new Vec3(0.0, 0.75F * getEyeHeight(), getBbWidth() * 0.5F);
    }
    public EntityDimensions getDefaultDimensions(Pose pose) {
        return net.minecraft.world.entity.EntityDimensions.scalable(getBbWidth(), getBbHeight());
    }
    public static enum Variant implements StringRepresentable {
        CREAMY, WHITE, BROWN, GRAY;
        public static final Variant DEFAULT = null;
        public static final Codec<Variant> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.entity.animal.equine.Llama.Variant.class, "CODEC");
        public static final Codec<Variant> LEGACY_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.entity.animal.equine.Llama.Variant.class, "LEGACY_CODEC");
        public static final StreamCodec<ByteBuf, Variant> STREAM_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.streamCodec(net.minecraft.world.entity.animal.equine.Llama.Variant.class, "STREAM_CODEC");
        public int getId() {
            return ordinal();
        }
        public static Variant byId(int id) {
            return values()[Math.floorMod(id, values().length)];
        }
        public String getSerializedName() {
            return name().toLowerCase(java.util.Locale.ROOT);
        }
    }
    static class LlamaGroupData extends AgeableMob.AgeableMobGroupData {
        public final Variant variant = null;
        private LlamaGroupData(Variant variant) {
            super(false);
        }
    }
    static class LlamaHurtByTargetGoal extends HurtByTargetGoal {
        public LlamaHurtByTargetGoal(Llama llama) {
            super(llama);
        }
        public boolean canContinueToUse() {
            if (mob instanceof Llama llama && llama.didSpit) {
                llama.setDidSpit(false);
                return false;
            }
            return super.canContinueToUse();
        }
    }
    static class LlamaAttackWolfGoal extends NearestAttackableTargetGoal<Wolf> {
        public LlamaAttackWolfGoal(Llama llama) {
            super(llama, Wolf.class, 16, false, true, (target, level) -> !((Wolf) target).isTame());
        }
        protected double getFollowDistance() {
            return super.getFollowDistance() * 0.25;
        }
    }
}
