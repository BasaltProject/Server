// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.animal.golem;
import net.minecraft.core.BlockPos;
import net.minecraft.network.syncher.SynchedEntityData;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.sounds.SoundEvent;
import net.minecraft.sounds.SoundSource;
import net.minecraft.world.DifficultyInstance;
import net.minecraft.world.InteractionHand;
import net.minecraft.world.InteractionResult;
import net.minecraft.world.damagesource.DamageSource;
import net.minecraft.world.entity.AnimationState;
import net.minecraft.world.entity.ContainerUser;
import net.minecraft.world.entity.EntitySpawnReason;
import net.minecraft.world.entity.EntityType;
import net.minecraft.world.entity.EquipmentSlot;
import net.minecraft.world.entity.LightningBolt;
import net.minecraft.world.entity.Shearable;
import net.minecraft.world.entity.SpawnGroupData;
import net.minecraft.world.entity.ai.Brain;
import net.minecraft.world.entity.ai.attributes.AttributeSupplier;
import net.minecraft.world.entity.player.Player;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.level.Level;
import net.minecraft.world.level.ServerLevelAccessor;
import net.minecraft.world.level.block.WeatheringCopper;
import net.minecraft.world.level.block.entity.ContainerOpenersCounter;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.storage.ValueInput;
import net.minecraft.world.level.storage.ValueOutput;
import net.minecraft.world.phys.Vec3;
public class CopperGolem extends AbstractGolem implements ContainerUser, Shearable {
    public static final EquipmentSlot EQUIPMENT_SLOT_ANTENNA = EquipmentSlot.SADDLE;
    public CopperGolem(EntityType<? extends AbstractGolem> type, Level level) {
        super(type, level);
    }
    public static AttributeSupplier.Builder createAttributes() {
        return net.minecraft.world.entity.Mob.createMobAttributes()
            .add(net.minecraft.world.entity.ai.attributes.Attributes.MOVEMENT_SPEED, 0.2)
            .add(net.minecraft.world.entity.ai.attributes.Attributes.STEP_HEIGHT, 1.0)
            .add(net.minecraft.world.entity.ai.attributes.Attributes.MAX_HEALTH, 12.0);
    }
    public CopperGolemState getState() {
        return live() instanceof org.bukkit.entity.CopperGolem api ? CopperGolemState.values()[api.getGolemState().ordinal()] : CopperGolemState.IDLE;
    }
    public void setState(CopperGolemState state) {
        if (live() instanceof org.bukkit.entity.CopperGolem api) {
            api.setGolemState(org.bukkit.entity.CopperGolem.State.values()[state.ordinal()]);
        }
    }
    public WeatheringCopper.WeatherState getWeatherState() {
        return live() instanceof org.bukkit.entity.CopperGolem api ? WeatheringCopper.WeatherState.values()[api.getWeatheringState().ordinal()] : WeatheringCopper.WeatherState.UNAFFECTED;
    }
    public void setWeatherState(WeatheringCopper.WeatherState state) {
        if (live() instanceof org.bukkit.entity.CopperGolem api) {
            api.setWeatheringState(io.papermc.paper.world.WeatheringCopperState.values()[state.ordinal()]);
        }
    }
    public void setOpenedChestPos(BlockPos openedChestPos) {
        this.openedChestPos = openedChestPos;
    }
    public void clearOpenedChestPos() {
        this.openedChestPos = null;
    }
    public AnimationState getIdleAnimationState() {
        return idleAnimationState;
    }
    public AnimationState getInteractionGetItemAnimationState() {
        return getItemAnimationState;
    }
    public AnimationState getInteractionGetNoItemAnimationState() {
        return getNoItemAnimationState;
    }
    public AnimationState getInteractionDropItemAnimationState() {
        return dropItemAnimationState;
    }
    public AnimationState getInteractionDropNoItemAnimationState() {
        return dropNoItemAnimationState;
    }
    protected Brain<CopperGolem> makeBrain(Brain.Packed packedBrain) {
        @SuppressWarnings("unchecked") Brain<CopperGolem> made = (Brain<CopperGolem>) (Brain<?>) super.makeBrain(packedBrain); return made;
    }
    public Brain<CopperGolem> getBrain() {
        @SuppressWarnings("unchecked") Brain<CopperGolem> made = (Brain<CopperGolem>) (Brain<?>) super.getBrain(); return made;
    }
    protected void defineSynchedData(SynchedEntityData.Builder entityData) {
    }
    public void addAdditionalSaveData(ValueOutput output) {
    }
    public void readAdditionalSaveData(ValueInput input) {
    }
    protected void customServerAiStep(ServerLevel level) {
    }
    public void tick() {
    }
    public InteractionResult mobInteract(Player player, InteractionHand hand) {
        return net.minecraft.world.InteractionResult.PASS;
    }
    public void spawn(WeatheringCopper.WeatherState weatherState) {
        setWeatherState(weatherState);
    }
    public SpawnGroupData finalizeSpawn(ServerLevelAccessor level, DifficultyInstance difficulty, EntitySpawnReason spawnReason, SpawnGroupData groupData) {
        return groupData;
    }
    public void playSpawnSound() {
        playSound(net.minecraft.sounds.SoundEvents.byId("entity.copper_golem.spawn"), 1.0F, 1.0F);
    }
    protected SoundEvent getHurtSound(DamageSource source) {
        return net.minecraft.sounds.SoundEvents.byId("entity.copper_golem.hurt");
    }
    protected SoundEvent getDeathSound() {
        return net.minecraft.sounds.SoundEvents.byId("entity.copper_golem.death");
    }
    protected void playStepSound(BlockPos pos, BlockState blockState) {
        playSound(net.minecraft.sounds.SoundEvents.byId("entity.copper_golem.step"), 0.15F, 1.0F);
    }
    public Vec3 getLeashOffset() {
        return new Vec3(0.0, 0.6F * getEyeHeight(), getBbWidth() * 0.4F);
    }
    public boolean hasContainerOpen(ContainerOpenersCounter container, BlockPos blockPos) {
        return openedChestPos != null && openedChestPos.equals(blockPos);
    }
    public double getContainerInteractionRange() {
        return 3.0;
    }
    public void shear(ServerLevel level, SoundSource soundSource, ItemStack tool) {
        if (live() instanceof org.bukkit.entity.CopperGolem api) {
            api.shear(net.kyori.adventure.sound.Sound.Source.NEUTRAL);
        }
    }
    public boolean readyForShearing() {
        return live() instanceof org.bukkit.entity.CopperGolem api && api.readyToBeSheared();
    }
    protected void dropEquipment(ServerLevel level) {
    }
    protected void actuallyHurt(ServerLevel level, DamageSource source, float dmg) {
        }
    public void thunderHit(ServerLevel level, LightningBolt lightningBolt) {
        super.thunderHit(level, lightningBolt);
    }
    private BlockPos openedChestPos;
    private final AnimationState idleAnimationState = new AnimationState();
    private final AnimationState getItemAnimationState = new AnimationState();
    private final AnimationState getNoItemAnimationState = new AnimationState();
    private final AnimationState dropItemAnimationState = new AnimationState();
    private final AnimationState dropNoItemAnimationState = new AnimationState();

}
