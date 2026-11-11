// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.animal.sniffer;
import io.netty.buffer.ByteBuf;
import java.util.Optional;
import java.util.function.IntFunction;
import net.minecraft.core.BlockPos;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.network.syncher.EntityDataAccessor;
import net.minecraft.network.syncher.SynchedEntityData;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.sounds.SoundEvent;
import net.minecraft.world.InteractionHand;
import net.minecraft.world.InteractionResult;
import net.minecraft.world.damagesource.DamageSource;
import net.minecraft.world.entity.AgeableMob;
import net.minecraft.world.entity.AnimationState;
import net.minecraft.world.entity.EntityDimensions;
import net.minecraft.world.entity.EntityType;
import net.minecraft.world.entity.Pose;
import net.minecraft.world.entity.ai.Brain;
import net.minecraft.world.entity.ai.attributes.AttributeSupplier;
import net.minecraft.world.entity.animal.Animal;
import net.minecraft.world.entity.player.Player;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.level.Level;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.phys.Vec3;
public class Sniffer extends Animal {
    public final AnimationState feelingHappyAnimationState = null;
    public final AnimationState scentingAnimationState = null;
    public final AnimationState sniffingAnimationState = null;
    public final AnimationState diggingAnimationState = null;
    public final AnimationState risingAnimationState = null;
    public static AttributeSupplier.Builder createAttributes() {
        return Animal.createAnimalAttributes()
            .add(net.minecraft.world.entity.ai.attributes.Attributes.MOVEMENT_SPEED, 0.1)
            .add(net.minecraft.world.entity.ai.attributes.Attributes.MAX_HEALTH, 14.0);
    }
    public Sniffer(EntityType<? extends Animal> type, Level level) {
        super(type, level);
    }
    protected void defineSynchedData(SynchedEntityData.Builder entityData) {
    }
    public EntityDimensions getDefaultDimensions(Pose pose) {
        return net.minecraft.world.entity.EntityDimensions.scalable(getBbWidth(), getBbHeight());
    }
    public State getState() {
        return State.values()[live() instanceof org.bukkit.entity.Sniffer api ? api.getState().ordinal() : 0];
    }
    public boolean isSearching() {
        return getState() == State.SEARCHING;
    }
    public boolean isTempted() {
        return false;
    }
    public boolean canSniff() {
        return getState() == State.IDLING && !isInWater() && !isLeashed() && onGround();
    }
    public boolean canPlayDiggingSound() {
        return getState() == State.DIGGING;
    }
    public boolean supportQuadLeash() {
        return true;
    }
    public Vec3[] getQuadLeashOffsets() {
        return new Vec3[] {new Vec3(0.0, 1.0, 0.0), new Vec3(0.0, 1.0, 0.0), new Vec3(0.0, 1.0, 0.0), new Vec3(0.0, 1.0, 0.0)};
    }
    public void onSyncedDataUpdated(EntityDataAccessor<?> accessor) {
    }
    public Sniffer transitionTo(State state) {
        if (live() instanceof org.bukkit.entity.Sniffer api) { api.setState(org.bukkit.entity.Sniffer.State.values()[state.ordinal()]); }
        return this;
    }
    public Sniffer onDiggingComplete(boolean success) {
        transitionTo(State.RISING);
        return this;
    }
    Optional<BlockPos> calculateDigPosition() {
        if (live() instanceof org.bukkit.entity.Sniffer api && api.findPossibleDigLocation() != null) {
            org.bukkit.Location loc = api.findPossibleDigLocation();
            return Optional.of(BlockPos.containing(loc.getX(), loc.getY(), loc.getZ()));
        }
        return Optional.empty();
    }
    boolean canDig() {
        return live() instanceof org.bukkit.entity.Sniffer api ? (boolean) api.canDig() : false;
    }
    public void tick() {
    }
    public InteractionResult mobInteract(Player player, InteractionHand hand) {
        return net.minecraft.world.InteractionResult.PASS;
    }
    protected void playEatingSound() {
        }
    protected void playStepSound(BlockPos pos, BlockState blockState) {
        playSound(net.minecraft.sounds.SoundEvents.byId("entity.sniffer.step"), 0.15F, 1.0F);
    }
    protected SoundEvent getAmbientSound() {
        return null;
    }
    protected SoundEvent getHurtSound(DamageSource source) {
        return net.minecraft.sounds.SoundEvents.byId("entity.sniffer.hurt");
    }
    protected SoundEvent getDeathSound() {
        return net.minecraft.sounds.SoundEvents.byId("entity.sniffer.death");
    }
    public int getMaxHeadYRot() {
        return super.getMaxHeadYRot();
    }
    public AgeableMob getBreedOffspring(ServerLevel level, AgeableMob partner) {
        return null;
    }
    public boolean canMate(Animal partner) {
        return partner != this && partner.getClass() == getClass() && live() instanceof org.bukkit.entity.Animals selfApi && selfApi.isLoveMode() && partner.live() instanceof org.bukkit.entity.Animals otherApi && otherApi.isLoveMode();
    }
    public boolean isFood(ItemStack itemStack) {
        return live() instanceof org.bukkit.entity.Animals animals && animals.isBreedItem(org.bukkit.craftbukkit.inventory.CraftItemStack.asCraftMirror(itemStack));
    }
    protected Brain<Sniffer> makeBrain(Brain.Packed packedBrain) {
        @SuppressWarnings("unchecked") Brain<Sniffer> made = (Brain<Sniffer>) (Brain<?>) super.makeBrain(packedBrain); return made;
    }
    public Brain<Sniffer> getBrain() {
        @SuppressWarnings("unchecked") Brain<Sniffer> made = (Brain<Sniffer>) (Brain<?>) super.getBrain(); return made;
    }
    protected void customServerAiStep(ServerLevel level) {
    }
    public static enum State {
        IDLING, FEELING_HAPPY, SCENTING, SNIFFING, SEARCHING, DIGGING, RISING;
        public static final IntFunction<State> BY_ID = id -> values()[id >= 0 && id < values().length ? id : 0];
        public static final StreamCodec<ByteBuf, State> STREAM_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.streamCodec(net.minecraft.world.entity.animal.sniffer.Sniffer.State.class, "STREAM_CODEC");
        public int id() {
            return ordinal();
        }
    }
}
