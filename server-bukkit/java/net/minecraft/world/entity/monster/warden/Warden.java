// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.monster.warden;
import net.minecraft.world.entity.EntityTypes;
import net.minecraft.util.Mth;
import java.util.Optional;
import java.util.function.BiConsumer;
import net.minecraft.core.BlockPos;
import net.minecraft.core.Holder;
import net.minecraft.network.protocol.Packet;
import net.minecraft.network.protocol.game.ClientGamePacketListener;
import net.minecraft.network.protocol.game.ClientboundAddEntityPacket;
import net.minecraft.network.syncher.EntityDataAccessor;
import net.minecraft.network.syncher.SynchedEntityData;
import net.minecraft.server.level.ServerEntity;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.sounds.SoundEvent;
import net.minecraft.tags.TagKey;
import net.minecraft.world.DifficultyInstance;
import net.minecraft.world.damagesource.DamageSource;
import net.minecraft.world.entity.AnimationState;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.entity.EntityDimensions;
import net.minecraft.world.entity.EntitySpawnReason;
import net.minecraft.world.entity.EntityType;
import net.minecraft.world.entity.LivingEntity;
import net.minecraft.world.entity.Pose;
import net.minecraft.world.entity.SpawnGroupData;
import net.minecraft.world.entity.ai.Brain;
import net.minecraft.world.entity.ai.attributes.AttributeSupplier;
import net.minecraft.world.entity.ai.navigation.PathNavigation;
import net.minecraft.world.entity.monster.Monster;
import net.minecraft.world.level.Explosion;
import net.minecraft.world.level.Level;
import net.minecraft.world.level.LevelReader;
import net.minecraft.world.level.ServerLevelAccessor;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.gameevent.DynamicGameEventListener;
import net.minecraft.world.level.gameevent.GameEvent;
import net.minecraft.world.level.gameevent.PositionSource;
import net.minecraft.world.level.gameevent.vibrations.VibrationSystem;
import net.minecraft.world.level.storage.ValueInput;
import net.minecraft.world.level.storage.ValueOutput;
import net.minecraft.world.phys.Vec3;
public class Warden extends Monster implements VibrationSystem {
    private final AngerManagement angerManagement = new AngerManagement(entity -> true, java.util.List.of());
    private final VibrationSystem.Data vibrationData = new VibrationSystem.Data();
    private final VibrationSystem.User vibrationUser = new VibrationUser();

    public final AnimationState roarAnimationState = null;
    public final AnimationState sniffAnimationState = null;
    public final AnimationState emergeAnimationState = null;
    public final AnimationState diggingAnimationState = null;
    public final AnimationState attackAnimationState = null;
    public final AnimationState sonicBoomAnimationState = null;
    public Warden(EntityType<? extends Monster> type, Level level) {
        super(type, level);
    }
    public Packet<ClientGamePacketListener> getAddEntityPacket(ServerEntity serverEntity) {
        return super.getAddEntityPacket(serverEntity);
    }
    public void recreateFromPacket(ClientboundAddEntityPacket packet) {
        super.recreateFromPacket(packet);
    }
    public boolean checkSpawnObstruction(LevelReader level) {
        return super.checkSpawnObstruction(level);
    }
    public float getWalkTargetValue(BlockPos pos, LevelReader level) {
        return super.getWalkTargetValue(pos, level);
    }
    public boolean isInvulnerableTo(ServerLevel level, DamageSource source) {
        return super.isInvulnerableTo(level, source);
    }
    protected boolean canRide(Entity vehicle) {
        return false;
    }
    protected float nextStep() {
        return 0;
    }
    public static AttributeSupplier.Builder createAttributes() {
        return Monster.createMonsterAttributes()
            .add(net.minecraft.world.entity.ai.attributes.Attributes.MAX_HEALTH, 500.0)
            .add(net.minecraft.world.entity.ai.attributes.Attributes.MOVEMENT_SPEED, 0.3)
            .add(net.minecraft.world.entity.ai.attributes.Attributes.KNOCKBACK_RESISTANCE, 1.0)
            .add(net.minecraft.world.entity.ai.attributes.Attributes.ATTACK_KNOCKBACK, 1.5)
            .add(net.minecraft.world.entity.ai.attributes.Attributes.ATTACK_DAMAGE, 30.0)
            .add(net.minecraft.world.entity.ai.attributes.Attributes.FOLLOW_RANGE, 24.0);
    }
    public boolean dampensVibrations() {
        return super.dampensVibrations();
    }
    protected float getSoundVolume() {
        return live() instanceof org.bukkit.entity.Warden api ? (float) api.getSoundVolume() : 0;
    }
    protected SoundEvent getAmbientSound() {
        return net.minecraft.sounds.SoundEvents.byId("entity.warden.ambient");
    }
    protected SoundEvent getHurtSound(DamageSource source) {
        return net.minecraft.sounds.SoundEvents.byId("entity.warden.hurt");
    }
    protected SoundEvent getDeathSound() {
        return net.minecraft.sounds.SoundEvents.byId("entity.warden.death");
    }
    protected void playStepSound(BlockPos pos, BlockState blockState) {
        playSound(net.minecraft.sounds.SoundEvents.byId("entity.warden.step"), 0.15F, 1.0F);
    }
    public boolean doHurtTarget(ServerLevel level, Entity target) {
        return super.doHurtTarget(level, target);
    }
    protected void defineSynchedData(SynchedEntityData.Builder entityData) {
    }
    public int getClientAngerLevel() {
        return live() instanceof org.bukkit.entity.Warden api ? Math.min(80, api.getHighestAnger()) : 0;
    }
    public void tick() {
    }
    protected void customServerAiStep(ServerLevel level) {
    }
    public void handleEntityEvent(byte id) {
    }
    public float getTendrilAnimation(float a) {
        return 0.0F;
    }
    public float getHeartAnimation(float a) {
        return 0.0F;
    }
    public void onSyncedDataUpdated(EntityDataAccessor<?> accessor) {
    }
    public boolean ignoreExplosion(Explosion explosion) {
        return super.ignoreExplosion(explosion);
    }
    protected Brain<Warden> makeBrain(Brain.Packed packedBrain) {
        @SuppressWarnings("unchecked") Brain<Warden> made = (Brain<Warden>) (Brain<?>) super.makeBrain(packedBrain); return made;
    }
    public Brain<Warden> getBrain() {
        @SuppressWarnings("unchecked") Brain<Warden> made = (Brain<Warden>) (Brain<?>) super.getBrain(); return made;
    }
    public boolean canTargetEntity(Entity entity) {
        return entity instanceof LivingEntity living && living.level() == level() && !(living instanceof Warden)
            && living.getType() != EntityTypes.ARMOR_STAND && !living.isInvulnerable() && !living.isDeadOrDying() && living.isAlive();
    }
    public static void applyDarknessAround(ServerLevel level, Vec3 position, Entity source, int darknessRadius) {
        if (!(source.live() instanceof org.bukkit.entity.Entity origin)) {
            return;
        }
        org.bukkit.Location where = new org.bukkit.Location(origin.getWorld(), position.x, position.y, position.z);
        for (org.bukkit.entity.Player player : origin.getWorld().getNearbyPlayers(where, darknessRadius)) {
            org.bukkit.potion.PotionEffect current = player.getPotionEffect(org.bukkit.potion.PotionEffectType.DARKNESS);
            if (current == null || current.getDuration() < 200) {
                player.addPotionEffect(new org.bukkit.potion.PotionEffect(org.bukkit.potion.PotionEffectType.DARKNESS, 260, 0, false, false));
            }
        }
    }
    protected void addAdditionalSaveData(ValueOutput output) {
    }
    protected void readAdditionalSaveData(ValueInput input) {
    }
    public AngerLevel getAngerLevel() {
        return live() instanceof org.bukkit.entity.Warden api ? AngerLevel.values()[api.getAngerLevel().ordinal()] : AngerLevel.CALM;
    }
    public void clearAnger(Entity entity) {
        angerManagement.clearAnger(entity);
        if (live() instanceof org.bukkit.entity.Warden api && entity.live() != null) { api.clearAnger(entity.live()); }
    }
    public void increaseAngerAt(Entity entity) {
        increaseAngerAt(entity, 35, true);
    }
    public void increaseAngerAt(Entity entity, int amount, boolean playSound) {
        angerManagement.increaseAnger(entity, amount);
        if (live() instanceof org.bukkit.entity.Warden api && entity.live() != null) { api.increaseAnger(entity.live(), amount); }
    }
    public Optional<LivingEntity> getEntityAngryAt() {
        if (live() instanceof org.bukkit.entity.Warden api && api.getEntityAngryAt() != null) {
            return Optional.of((LivingEntity) Entity.fromBukkit(api.getEntityAngryAt()));
        }
        return Optional.empty();
    }
    public boolean removeWhenFarAway(double distSqr) {
        return super.removeWhenFarAway(distSqr);
    }
    public SpawnGroupData finalizeSpawn(ServerLevelAccessor level, DifficultyInstance difficulty, EntitySpawnReason spawnReason, SpawnGroupData groupData) {
        return groupData;
    }
    public boolean hurtServer(ServerLevel level, DamageSource source, float damage) {
        return super.hurtServer(level, source, damage);
    }
    public void setAttackTarget(LivingEntity target) {
        setTarget(target);
    }
    public EntityDimensions getDefaultDimensions(Pose pose) {
        return net.minecraft.world.entity.EntityDimensions.scalable(getBbWidth(), getBbHeight());
    }
    protected void doPush(Entity entity) {
    }
    public AngerManagement getAngerManagement() {
        return angerManagement;
    }
    protected PathNavigation createNavigation(Level level) {
        return super.createNavigation(level);
    }
    public VibrationSystem.Data getVibrationData() {
        return vibrationData;
    }
    public VibrationSystem.User getVibrationUser() {
        return vibrationUser;
    }
    class VibrationUser implements VibrationSystem.User {
        private final net.minecraft.world.level.gameevent.PositionSource source = new net.minecraft.world.level.gameevent.EntityPositionSource(Warden.this, Warden.this.getEyeHeight());

        private VibrationUser() {
        }
        public int getListenerRadius() {
            return 16;
        }
        public PositionSource getPositionSource() {
            return source;
        }
        public TagKey<GameEvent> getListenableEvents() {
            return net.minecraft.tags.GameEventTags.WARDEN_CAN_LISTEN;
        }
        public boolean canTriggerAvoidVibration() {
            return true;
        }
        public boolean canReceiveVibration(ServerLevel level, BlockPos pos, Holder<GameEvent> event, GameEvent.Context context) {
            return !isNoAi() && !isDeadOrDying() && level.getWorldBorder().isWithinBounds(pos);
        }
        public void onReceiveVibration(ServerLevel level, BlockPos pos, Holder<GameEvent> event, Entity sourceEntity, Entity projectileOwner, float receivingDistance) {
            if (isDeadOrDying()) {
                return;
            }
            Entity culprit = projectileOwner != null ? projectileOwner : sourceEntity;
            if (culprit != null) {
                angerManagement.increaseAnger(culprit, 35);
            }
        }
    }
}
