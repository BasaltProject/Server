// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity;

import net.minecraft.server.level.ServerLevel;
import net.minecraft.world.waypoints.WaypointTransmitter;
import com.google.common.collect.ImmutableList;
import it.unimi.dsi.fastutil.objects.Object2LongMap;
import java.util.Collection;
import java.util.Map;
import java.util.Optional;
import java.util.Set;
import java.util.UUID;
import java.util.function.BiConsumer;
import java.util.function.Consumer;
import java.util.function.Function;
import java.util.function.Predicate;
import net.minecraft.commands.arguments.EntityAnchorArgument;
import net.minecraft.core.BlockPos;
import net.minecraft.core.Direction;
import net.minecraft.core.Holder;
import net.minecraft.network.protocol.game.ClientboundAddEntityPacket;
import net.minecraft.network.syncher.EntityDataAccessor;
import net.minecraft.network.syncher.SynchedEntityData;
import net.minecraft.resources.ResourceKey;
import net.minecraft.server.level.ServerPlayer;
import net.minecraft.sounds.SoundEvent;
import net.minecraft.tags.TagKey;
import net.minecraft.util.BlockUtil;
import net.minecraft.util.Prediction;
import net.minecraft.world.InteractionHand;
import net.minecraft.world.damagesource.CombatTracker;
import net.minecraft.world.damagesource.DamageSource;
import net.minecraft.world.effect.MobEffect;
import net.minecraft.world.effect.MobEffectInstance;
import net.minecraft.world.entity.ai.Brain;
import net.minecraft.world.entity.ai.attributes.Attribute;
import net.minecraft.world.entity.ai.attributes.AttributeInstance;
import net.minecraft.nbt.CompoundTag;
import net.minecraft.util.Mth;
import net.minecraft.world.entity.ai.attributes.Attributes;
import net.minecraft.world.entity.ai.attributes.AttributeMap;
import net.minecraft.world.entity.ai.attributes.AttributeSupplier;
import net.minecraft.world.entity.item.ItemEntity;
import net.minecraft.world.entity.player.Player;
import net.minecraft.world.item.Item;
import net.minecraft.world.item.ItemInstance;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.item.component.AttackRange;
import net.minecraft.world.item.component.SwingAnimation;
import net.minecraft.world.item.enchantment.Enchantment;
import net.minecraft.world.item.enchantment.effects.EnchantmentLocationBasedEffect;
import net.minecraft.world.item.equipment.Equippable;
import net.minecraft.world.level.ClipContext;
import net.minecraft.world.level.Level;
import net.minecraft.world.level.block.Block;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.material.Fluid;
import net.minecraft.world.level.material.FluidState;
import net.minecraft.world.level.storage.ValueInput;
import net.minecraft.world.level.storage.ValueOutput;
import net.minecraft.world.level.storage.loot.LootParams;
import net.minecraft.world.level.storage.loot.LootTable;
import net.minecraft.world.phys.AABB;
import net.minecraft.world.phys.Vec3;
import net.minecraft.world.phys.shapes.VoxelShape;
import net.minecraft.world.waypoints.Waypoint;
public class LivingEntity extends Entity implements Attackable, WaypointTransmitter {
    protected LivingEntity() {
    }

    LivingEntity(EntityType<?> type, ServerLevel level) {
        super(type, level);
    }
    private AttributeMap attributeMap;
    private float speed;
    private boolean discardFriction;
    private boolean skipDropExperience;
    private BlockPos sleepingPos;

    protected static org.bukkit.inventory.EquipmentSlot bukkitSlot(EquipmentSlot slot) {
        return switch (slot) {
            case MAINHAND -> org.bukkit.inventory.EquipmentSlot.HAND;
            case OFFHAND -> org.bukkit.inventory.EquipmentSlot.OFF_HAND;
            case FEET -> org.bukkit.inventory.EquipmentSlot.FEET;
            case LEGS -> org.bukkit.inventory.EquipmentSlot.LEGS;
            case CHEST -> org.bukkit.inventory.EquipmentSlot.CHEST;
            case HEAD -> org.bukkit.inventory.EquipmentSlot.HEAD;
            case BODY -> org.bukkit.inventory.EquipmentSlot.BODY;
            case SADDLE -> org.bukkit.inventory.EquipmentSlot.SADDLE;
        };
    }

    private static org.bukkit.potion.PotionEffectType typeOf(Holder<MobEffect> effect) {
        return effect.unwrapKey().map(key -> org.bukkit.potion.PotionEffectType.getByKey(
            org.bukkit.NamespacedKey.fromString(key.identifier().toString()))).orElse(null);
    }

    private static Holder<MobEffect> effectOf(org.bukkit.potion.PotionEffectType type) {
        return net.minecraft.world.effect.MobEffects.registry().get(net.minecraft.resources.Identifier.parse(type.getKey().toString())).orElse(null);
    }

    private static MobEffectInstance instanceOf(Holder<MobEffect> holder, org.bukkit.potion.PotionEffect active) {
        return new MobEffectInstance(holder, active.getDuration(), active.getAmplifier(), active.isAmbient(), active.hasParticles(), active.hasIcon());
    }

    private org.bukkit.entity.LivingEntity living() {
        return live() instanceof org.bukkit.entity.LivingEntity living ? living : null;
    }
    public static final String TAG_ATTRIBUTES = "attributes";
    public static final String TAG_SLEEPING_POS = "sleeping_pos";
    public static final String TAG_EQUIPMENT = "equipment";
    public static final String TAG_BRAIN = "Brain";
    public static final String TAG_FALL_FLYING = "FallFlying";
    public static final String TAG_HURT_TIME = "HurtTime";
    public static final String TAG_DEATH_TIME = "DeathTime";
    public static final String TAG_HEALTH = "Health";
    public static final int EQUIPMENT_SLOT_OFFSET = 98;
    public static final int ARMOR_SLOT_OFFSET = 100;
    public static final int BODY_ARMOR_OFFSET = 105;
    public static final int SADDLE_OFFSET = 106;
    public static final int PLAYER_HURT_EXPERIENCE_TIME = 100;
    public static final double MIN_MOVEMENT_DISTANCE = 0.003;
    public static final double DEFAULT_BASE_GRAVITY = 0.08;
    public static final int DEATH_DURATION = 20;
    protected static final float INPUT_FRICTION = 0.98F;
    public static final float BASE_JUMP_POWER = 0.42F;
    protected static final float DEFAULT_KNOCKBACK = 0.4F;
    protected static final int DAMAGE_COOLDOWN_DURATION = 20;
    protected static final int HURT_DURATION_TICKS = 10;
    protected static final int LIVING_ENTITY_FLAG_IS_USING = 1;
    protected static final int LIVING_ENTITY_FLAG_OFF_HAND = 2;
    protected static final int LIVING_ENTITY_FLAG_SPIN_ATTACK = 4;
    protected static final EntityDataAccessor<Byte> DATA_LIVING_ENTITY_FLAGS = null;
    protected static final EntityDimensions SLEEPING_DIMENSIONS = null;
    public static final float EXTRA_RENDER_CULLING_SIZE_WITH_BIG_HAT = 0.5F;
    public static final float DEFAULT_BABY_SCALE = 0.5F;
    protected static final float SWIMMING_VERTICAL_SPEED = 0.04F;
    public static final float BASE_HORIZONTAL_AIR_DRAG = 0.91F;
    public static final float BASE_VERTICAL_AIR_DRAG = 0.98F;
    public static final float WATER_DRAG = 0.8F;
    public static final float SPRINTING_WATER_DRAG = 0.9F;
    public static final float LAVA_DRAG = 0.5F;
    public static final float LAVA_SHALLOW_VERTICAL_DRAG = 0.8F;
    public static final float DOLPHINS_GRACE_WATER_DRAG = 0.96F;
    public static final float FLYING_AIR_DRAG = 0.91F;
    public static final float FLYING_VERTICAL_AIR_DRAG = 0.91F;
    public static final float FLYING_LAVA_DRAG = 0.5F;
    public static final float FLYING_WATER_DRAG = 0.8F;
    public static final float ELYTRA_HORIZONTAL_AIR_DRAG = 0.99F;
    public static final float ELYTRA_VERTICAL_AIR_DRAG = 0.98F;
    public static final float BASE_SWIM_SPEED = 0.02F;
    public static final Predicate<LivingEntity> PLAYER_NOT_WEARING_DISGUISE_ITEM = null;
    public int removeArrowTime;
    public int removeStingerTime;
    public int hurtTime;
    public int hurtDuration;
    public int deathTime;
    public int damageCooldownTime;
    protected int attackStrengthTicker;
    protected int itemSwapTicker;
    public final WalkAnimationState walkAnimation = null;
    public float yBodyRot;
    public float yBodyRotO;
    public float yHeadRot;
    public float yHeadRotO;
    public final ElytraAnimationState elytraAnimationState = null;
    protected EntityReference<Player> lastHurtByPlayer;
    protected int lastHurtByPlayerMemoryTime;
    protected boolean dead;
    protected int noActionTime;
    protected float lastHurt;
    protected boolean jumping;
    public float xxa;
    public float yya;
    public float zza;
    protected double lerpYHeadRot;
    protected int lerpHeadSteps;
    protected ItemStack useItem;
    protected int useItemRemaining;
    protected int fallFlyTicks;
    protected int autoSpinAttackTicks;
    protected float autoSpinAttackDmg;
    protected ItemStack autoSpinAttackItemStack;
    protected Object2LongMap<Entity> recentKineticEnemies;
    protected Brain<?> brain;
    protected final EntityEquipment equipment = null;
    public Vec3 currentImpulseImpactPos;
    protected LivingEntity(EntityType<? extends LivingEntity> type, Level level) {
        super(type, level);
    }
    public LivingEntity asLivingEntity() {
        return this;
    }
    protected EntityEquipment createEquipment() {
        return null;
    }
    public Brain<? extends LivingEntity> getBrain() {
        if (brain == null) {
            brain = new Brain<LivingEntity>();
        }
        return brain;
    }
    protected Brain<? extends LivingEntity> makeBrain(Brain.Packed packedBrain) {
        return new Brain<LivingEntity>();
    }
    public void kill(ServerLevel level) {
        org.bukkit.entity.LivingEntity living = living();
        if (living != null) {
            living.setHealth(0.0);
        }
    }
    protected void defineSynchedData(SynchedEntityData.Builder entityData) {
        return;
    }
    public static AttributeSupplier.Builder createLivingAttributes() {
        return AttributeSupplier.builder().add(Attributes.MAX_HEALTH).add(Attributes.KNOCKBACK_RESISTANCE).add(Attributes.MOVEMENT_SPEED)
            .add(Attributes.ARMOR).add(Attributes.ARMOR_TOUGHNESS).add(Attributes.MAX_ABSORPTION).add(Attributes.STEP_HEIGHT)
            .add(Attributes.SCALE).add(Attributes.GRAVITY).add(Attributes.SAFE_FALL_DISTANCE).add(Attributes.FALL_DAMAGE_MULTIPLIER)
            .add(Attributes.JUMP_STRENGTH).add(Attributes.ENTITY_INTERACTION_RANGE).add(Attributes.OXYGEN_BONUS).add(Attributes.BURNING_TIME)
            .add(Attributes.EXPLOSION_KNOCKBACK_RESISTANCE).add(Attributes.WATER_MOVEMENT_EFFICIENCY).add(Attributes.MOVEMENT_EFFICIENCY)
            .add(Attributes.ATTACK_KNOCKBACK).add(Attributes.CAMERA_DISTANCE).add(Attributes.WAYPOINT_TRANSMIT_RANGE).add(Attributes.BOUNCINESS)
            .add(Attributes.AIR_DRAG_MODIFIER).add(Attributes.FRICTION_MODIFIER).add(Attributes.NAME_TAG_DISTANCE).add(Attributes.BELOW_NAME_DISTANCE);
    }
    protected void checkFallDamage(double ya, boolean onGround, BlockState onState, BlockPos pos) {
        if (onGround && fallDistance > 0.0) {
            causeFallDamage(fallDistance, 1.0F, damageSources().fall());
            resetFallDistance();
        } else if (ya < 0.0) {
            fallDistance -= ya;
        }
    }
    public boolean canBreatheUnderwater() {
        return false;
    }
    public float getSwimAmount(float a) {
        return isSwimming() ? 1.0F : 0.0F;
    }
    public boolean hasLandedInLiquid() {
        return getDeltaMovement().y < 1.0E-5 && isInLiquid();
    }
    public void baseTick() {
        tickCount++;
    }
    protected boolean shouldTakeDrowningDamage() {
        return getAirSupply() <= -20;
    }
    protected float getBlockSpeedFactor() {
        return super.getBlockSpeedFactor();
    }
    public float getLuck() {
        return 0.0F;
    }
    protected void removeFrost() {
        return;
    }
    protected void tryAddFrost() {
        return;
    }
    protected void onChangedBlock(ServerLevel level, BlockPos pos) {
        return;
    }
    public boolean isBaby() {
        org.bukkit.entity.LivingEntity living = living();
        return living instanceof org.bukkit.entity.Ageable ageable && !ageable.isAdult();
    }
    public float getAgeScale() {
        return 1.0F;
    }
    public float getScale() {
        AttributeMap map = getAttributes();
        return map == null ? 1.0F : sanitizeScale((float) map.getValue(Attributes.SCALE));
    }
    protected float sanitizeScale(float scale) {
        return scale;
    }
    public boolean isAffectedByFluids() {
        return true;
    }
    protected void tickDeath() {
        return;
    }
    public boolean shouldDropExperience() {
        return !isBaby();
    }
    protected boolean shouldDropLoot(ServerLevel level) {
        return true;
    }
    protected int decreaseAirSupply(int currentSupply) {
        double bonus = getAttributeValue(Attributes.OXYGEN_BONUS);
        return bonus > 0.0 && random.nextDouble() >= 1.0 / (bonus + 1.0) ? currentSupply : currentSupply - 1;
    }
    protected int increaseAirSupply(int currentSupply) {
        return Math.min(currentSupply + 4, getMaxAirSupply());
    }
    public int getExperienceReward(ServerLevel level, Entity killer) {
        return 0;
    }
    protected int getBaseExperienceReward(ServerLevel level) {
        return 0;
    }
    protected boolean isAlwaysExperienceDropper() {
        return false;
    }
    public LivingEntity getLastHurtByMob() {
        return lastHurtByMobRef;
    }
    public Player getLastHurtByPlayer() {
        return living() != null && living().getKiller() != null ? (Player) Entity.fromBukkit(living().getKiller()) : null;
    }
    public LivingEntity getLastAttacker() {
        return getLastHurtByMob();
    }
    public int getLastHurtByMobTimestamp() {
        return lastHurtByMobStamp;
    }
    public void setLastHurtByPlayer(Player player, int timeToRemember) {
        lastHurtByPlayerMemoryTime = timeToRemember;
        lastHurtByPlayerRef = player;
    }
    public void setLastHurtByPlayer(UUID player, int timeToRemember) {
        lastHurtByPlayerMemoryTime = timeToRemember;
        lastHurtByPlayerRef = level().getPlayerByUUID(player);
    }
    public void setLastHurtByMob(LivingEntity hurtBy) {
        lastHurtByMobRef = hurtBy;
        lastHurtByMobStamp = tickCount;
    }
    public LivingEntity getLastHurtMob() {
        return lastHurtMobRef;
    }
    public int getLastHurtMobTimestamp() {
        return lastHurtMobStamp;
    }
    public void setLastHurtMob(Entity target) {
        lastHurtMobRef = target instanceof LivingEntity victim ? victim : null;
        lastHurtMobStamp = tickCount;
    }
    public int getNoActionTime() {
        return noActionTime;
    }
    public void setNoActionTime(int noActionTime) {
        this.noActionTime = noActionTime;
    }
    public boolean shouldDiscardFriction() {
        return discardFriction;
    }
    public void setDiscardFriction(boolean discardFriction) {
        this.discardFriction = discardFriction;
    }
    protected boolean doesEmitEquipEvent(EquipmentSlot slot) {
        return true;
    }
    public void onEquipItem(EquipmentSlot slot, ItemStack oldStack, ItemStack stack) {
        return;
    }
    protected Holder<SoundEvent> getEquipSound(EquipmentSlot slot, ItemStack stack, Equippable equippable) {
        return equippable.equipSound();
    }
    public void remove(Entity.RemovalReason reason) {
        if (live() != null) {
            live().remove();
        }
    }
    public void onRemoval(Entity.RemovalReason reason) {

    }
    protected void triggerOnDeathMobEffects(ServerLevel level, Entity.RemovalReason reason) {
        return;
    }
    protected void addAdditionalSaveData(ValueOutput output) {
        output.putFloat("Health", getHealth());
        output.putShort("HurtTime", (short) hurtTime);
        output.putShort("DeathTime", (short) deathTime);
    }
    public ItemEntity drop(ItemStack itemStack, boolean thrownFromHand, Prediction prediction) {
        if (itemStack.isEmpty() || live() == null) {
            return null;
        }
        return spawnAtLocation(level(), itemStack, getEyeHeight() - 0.3F);
    }
    protected void readAdditionalSaveData(ValueInput input) {
        setHealth(input.getFloatOr("Health", getHealth()));
        hurtTime = input.getShortOr("HurtTime", (short) 0);
        deathTime = input.getShortOr("DeathTime", (short) 0);
    }
    public void updateDataBeforeSync() {
        return;
    }
    protected void tickEffects() {
        return;
    }
    protected void updateInvisibilityStatus() {
        setInvisible(hasEffect(net.minecraft.world.effect.MobEffects.INVISIBILITY));
    }
    public double getVisibilityPercent(ServerLevel serverLevel, Entity targetingEntity) {
        double visibility = 1.0;
        if (isDiscrete()) {
            visibility *= 0.8;
        }
        if (isInvisible()) {
            float coverage = Math.max(getArmorCoverPercentage(), 0.1F);
            visibility *= 0.7 * coverage;
        }
        return visibility;
    }
    public boolean canAttack(LivingEntity target) {
        return target.canBeSeenAsEnemy();
    }
    public boolean canBeSeenAsEnemy() {
        return !isInvulnerable() && canBeSeenByAnyone();
    }
    public boolean canBeSeenByAnyone() {
        return !isSpectator() && isAlive();
    }
    public static boolean areAllEffectsAmbient(Collection<MobEffectInstance> effects) {
        for (MobEffectInstance effect : effects) {
            if (effect.isVisible() && !effect.isAmbient()) {
                return false;
            }
        }
        return true;
    }
    protected void removeEffectParticles() {
        return;
    }
    public boolean removeAllEffects() {
        org.bukkit.entity.LivingEntity living = living();
        if (living == null || living.getActivePotionEffects().isEmpty()) {
            return false;
        }
        for (org.bukkit.potion.PotionEffect active : living.getActivePotionEffects()) {
            living.removePotionEffect(active.getType());
        }
        return true;
    }
    public Collection<MobEffectInstance> getActiveEffects() {
        return getActiveEffectsMap().values();
    }
    public Map<Holder<MobEffect>, MobEffectInstance> getActiveEffectsMap() {
        Map<Holder<MobEffect>, MobEffectInstance> out = new java.util.LinkedHashMap<>();
        org.bukkit.entity.LivingEntity living = living();
        if (living != null) {
            for (org.bukkit.potion.PotionEffect active : living.getActivePotionEffects()) {
                Holder<MobEffect> holder = effectOf(active.getType());
                if (holder != null) {
                    out.put(holder, instanceOf(holder, active));
                }
            }
        }
        return out;
    }
    public boolean hasEffect(Holder<MobEffect> effect) {
        org.bukkit.entity.LivingEntity living = living();
        org.bukkit.potion.PotionEffectType type = typeOf(effect);
        return living != null && type != null && living.hasPotionEffect(type);
    }
    public MobEffectInstance getEffect(Holder<MobEffect> effect) {
        org.bukkit.entity.LivingEntity living = living();
        org.bukkit.potion.PotionEffectType type = typeOf(effect);
        org.bukkit.potion.PotionEffect active = living == null || type == null ? null : living.getPotionEffect(type);
        return active == null ? null : instanceOf(effect, active);
    }
    public float getEffectBlendFactor(Holder<MobEffect> effect, float partialTicks) {
        return hasEffect(effect) ? 1.0F : 0.0F;
    }
    public boolean addEffect(MobEffectInstance newEffect) {
        return addEffect(newEffect, null);
    }
    public boolean addEffect(MobEffectInstance newEffect, Entity source) {
        org.bukkit.entity.LivingEntity living = living();
        org.bukkit.potion.PotionEffectType type = typeOf(newEffect.getEffect());
        if (living == null || type == null) {
            return false;
        }
        return living.addPotionEffect(new org.bukkit.potion.PotionEffect(type, newEffect.getDuration(), newEffect.getAmplifier(),
            newEffect.isAmbient(), newEffect.isVisible(), newEffect.showIcon()));
    }
    public boolean canBeAffected(MobEffectInstance newEffect) {
        return typeOf(newEffect.getEffect()) != null;
    }
    public void forceAddEffect(MobEffectInstance newEffect, Entity source) {
        addEffect(newEffect, source);
    }
    public boolean isInvertedHealAndHarm() {
        return false;
    }
    public MobEffectInstance removeEffectNoUpdate(Holder<MobEffect> effect) {
        MobEffectInstance previous = getEffect(effect);
        removeEffect(effect);
        return previous;
    }
    public boolean removeEffect(Holder<MobEffect> effect) {
        org.bukkit.entity.LivingEntity living = living();
        org.bukkit.potion.PotionEffectType type = typeOf(effect);
        if (living == null || type == null || !living.hasPotionEffect(type)) {
            return false;
        }
        living.removePotionEffect(type);
        return true;
    }
    protected void onEffectAdded(MobEffectInstance effect, Entity source) {
        return;
    }
    public void sendEffectToPassengers(MobEffectInstance effect) {
        for (Entity passenger : getPassengers()) {
            if (passenger instanceof LivingEntity rider) {
                rider.addEffect(new MobEffectInstance(effect));
            }
        }
    }
    protected void onEffectUpdated(MobEffectInstance effect, boolean doRefreshAttributes, Entity source) {
        return;
    }
    protected void onEffectsRemoved(Collection<MobEffectInstance> effects) {
        return;
    }
    protected void onAttributeUpdated(Holder<Attribute> attribute) {
        return;
    }
    public void heal(float heal) {
        float now = getHealth();
        if (now > 0.0F) {
            setHealth(now + heal);
        }
    }
    public float getHealth() {
        org.bukkit.entity.LivingEntity living = living();
        if (living != null) {
            return (float) living.getHealth();
        }
        CompoundTag tag = nbt();
        return tag == null ? 0.0F : tag.getFloat("Health").orElse(getMaxHealth());
    }
    public void setHealth(float health) {
        org.bukkit.entity.LivingEntity living = living();
        if (living != null) {
            living.setHealth(Math.max(0.0, Math.min(health, getMaxHealth())));
        }
    }
    public boolean isDeadOrDying() {
        return getHealth() <= 0.0F;
    }
    public boolean wasHurtRecently() {
        return hurtTime > 0;
    }
    public boolean hurtServer(ServerLevel level, DamageSource source, float damage) {
        org.bukkit.entity.LivingEntity living = living();
        if (living == null || isDeadOrDying() || isInvulnerableTo(level, source) || damage <= 0.0F) {
            return false;
        }
        lastDamageSourceRef = source;
        lastDamageStamp = living.getWorld().getGameTime();
        net.minecraft.world.entity.Entity attacker = source.getEntity();
        org.bukkit.entity.Entity bukkitAttacker = attacker == null ? null : attacker.live();
        if (bukkitAttacker != null) {
            living.damage(damage, bukkitAttacker);
        } else {
            living.damage(damage);
        }
        return true;
    }
    public void dealDefaultKnockback(DamageSource source, float damage, boolean blocked) {
        Vec3 from = source.getSourcePosition();
        if (from != null) {
            knockback(0.4F, from.x - getX(), from.z - getZ(), source, damage, false);
        } else {
            knockback(0.4F, random.nextDouble() - 0.5, random.nextDouble() - 0.5, source, damage, false);
        }
    }
    public float applyItemBlocking(ServerLevel level, DamageSource source, float damage) {
        return 0.0F;
    }
    protected void resolveMobResponsibleForDamage(DamageSource source) {
        if (source.getEntity() instanceof LivingEntity attacker && !source.is(net.minecraft.tags.DamageTypeTags.NO_ANGER)) {
            setLastHurtByMob(attacker);
        }
    }
    protected Player resolvePlayerResponsibleForDamage(DamageSource source) {
        Entity cause = source.getEntity();
        return cause instanceof Player player ? player : null;
    }
    protected void blockUsingItem(ServerLevel level, LivingEntity attacker, DamageSource source, float damage, boolean fullyBlocked) {
        return;
    }
    protected void blockedByItem(LivingEntity defender, DamageSource source, float damage, boolean fullyBlocked) {
        return;
    }
    public DamageSource getLastDamageSource() {
        return getLastDamageSource(40);
    }
    public DamageSource getLastDamageSource(int damageSourceTimeout) {
        return live() == null || live().getWorld().getGameTime() - lastDamageStamp > damageSourceTimeout ? null : lastDamageSourceRef;
    }
    protected void playHurtSound(DamageSource source) {
        makeSound(getHurtSound(source));
    }
    public void makeSound(SoundEvent sound) {
        if (sound != null) {
            playSound(sound, getSoundVolume(), getVoicePitch());
        }
    }
    public void die(DamageSource source) {
        setHealth(0.0F);
    }
    protected void handleKillingBlow() {
        return;
    }
    protected void createWitherRose(LivingEntity killer) {
        return;
    }
    protected void dropAllDeathLoot(ServerLevel level, DamageSource source) {
        dropEquipment(level);
        dropCustomDeathLoot(level, source, resolvePlayerResponsibleForDamage(source) != null);
        if (shouldDropLoot(level)) {
            dropFromLootTable(level, source, resolvePlayerResponsibleForDamage(source) != null);
        }
        dropExperience(level, source.getEntity());
    }
    protected void dropEquipment(ServerLevel level) {
        return;
    }
    protected void dropExperience(ServerLevel level, Entity killer) {
        if (!skipDropExperience && shouldDropExperience()) {
            int amount = getExperienceReward(level, killer);
            if (amount > 0) {
                net.minecraft.world.entity.ExperienceOrb.award(level, position(), amount);
            }
        }
    }
    protected void dropCustomDeathLoot(ServerLevel level, DamageSource source, boolean killedByPlayer) {
        return;
    }
    public long getLootTableSeed() {
        return 0L;
    }
    protected float getKnockback(Entity target, DamageSource damageSource) {
        return (float) getAttributeValue(Attributes.ATTACK_KNOCKBACK);
    }
    protected void dropFromLootTable(ServerLevel level, DamageSource source, boolean playerKilled) {
        dropFromLootTable(level, source, playerKilled, defaultLootKey());
    }
    public void dropFromLootTable(ServerLevel level, DamageSource source, boolean playerKilled, ResourceKey<LootTable> lootTable) {
        dropFromLootTable(level, source, playerKilled, lootTable, stack -> spawnAtLocation(level, stack));
    }
    public void dropFromLootTable(ServerLevel level, DamageSource source, boolean playerKilled, ResourceKey<LootTable> lootTable, Consumer<ItemStack> itemStackConsumer) {
        if (lootTable == null) {
            return;
        }
        dropFromLootTable(level, lootTable, params -> {
            params.withParameter(net.minecraft.world.level.storage.loot.parameters.LootContextParams.THIS_ENTITY, this)
                .withParameter(net.minecraft.world.level.storage.loot.parameters.LootContextParams.ORIGIN, position())
                .withParameter(net.minecraft.world.level.storage.loot.parameters.LootContextParams.DAMAGE_SOURCE, source)
                .withOptionalParameter(net.minecraft.world.level.storage.loot.parameters.LootContextParams.ATTACKING_ENTITY, source.getEntity())
                .withOptionalParameter(net.minecraft.world.level.storage.loot.parameters.LootContextParams.DIRECT_ATTACKING_ENTITY, source.getDirectEntity());
            Player credit = resolvePlayerResponsibleForDamage(source);
            if (playerKilled && credit != null) {
                params.withParameter(net.minecraft.world.level.storage.loot.parameters.LootContextParams.LAST_DAMAGE_PLAYER, credit);
            }
            return lootParams(params);
        }, (lootLevel, stack) -> itemStackConsumer.accept(stack));
    }
    public boolean dropFromEntityInteractLootTable(ServerLevel level, ResourceKey<LootTable> key, Entity interactingEntity, ItemInstance tool, BiConsumer<ServerLevel, ItemStack> consumer) {
        return dropFromLootTable(level, key, params -> lootParams(params.withParameter(
            net.minecraft.world.level.storage.loot.parameters.LootContextParams.THIS_ENTITY, this)
            .withParameter(net.minecraft.world.level.storage.loot.parameters.LootContextParams.INTERACTING_ENTITY, interactingEntity)
            .withParameter(net.minecraft.world.level.storage.loot.parameters.LootContextParams.ORIGIN, position())), consumer);
    }
    public boolean dropFromGiftLootTable(ServerLevel level, ResourceKey<LootTable> key, BiConsumer<ServerLevel, ItemStack> consumer) {
        return dropFromLootTable(level, key, params -> lootParams(params.withParameter(
            net.minecraft.world.level.storage.loot.parameters.LootContextParams.THIS_ENTITY, this)
            .withParameter(net.minecraft.world.level.storage.loot.parameters.LootContextParams.ORIGIN, position())), consumer);
    }
    protected void dropFromShearingLootTable(ServerLevel level, ResourceKey<LootTable> key, ItemInstance tool, BiConsumer<ServerLevel, ItemStack> consumer) {
        dropFromLootTable(level, key, params -> lootParams(params.withParameter(
            net.minecraft.world.level.storage.loot.parameters.LootContextParams.THIS_ENTITY, this)
            .withParameter(net.minecraft.world.level.storage.loot.parameters.LootContextParams.ORIGIN, position())), consumer);
    }
    protected boolean dropFromLootTable(ServerLevel level, ResourceKey<LootTable> key, Function<LootParams.Builder, LootParams> paramsBuilder, BiConsumer<ServerLevel, ItemStack> consumer) {
        net.minecraft.world.level.storage.loot.LootParams params = paramsBuilder.apply(new net.minecraft.world.level.storage.loot.LootParams.Builder(level));
        net.minecraft.world.level.storage.loot.LootTable.of(key.identifier()).getRandomItems(params, stack -> consumer.accept(level, stack));
        return true;
    }
    public void knockback(double power, double xd, double zd, DamageSource source, float damage, boolean comesFromEffect) {
        if (living() != null) {
            living().knockback(power, xd, zd);
        }
    }
    public void knockback(double power, double xd, double zd, DamageSource source, float damage) {
        knockback(power, xd, zd, source, damage, false);
    }
    public void indicateDamage(double xd, double zd) {
        return;
    }
    protected SoundEvent getHurtSound(DamageSource source) {
        return net.minecraft.sounds.SoundEvents.byId("entity.generic.hurt");
    }
    protected SoundEvent getDeathSound() {
        return net.minecraft.sounds.SoundEvents.byId("entity.generic.death");
    }
    public void skipDropExperience() {
        skipDropExperience = true;
    }
    public boolean wasExperienceConsumed() {
        return skipDropExperience;
    }
    public float getHurtDir() {
        return 0.0F;
    }
    protected AABB getHitbox() {
        return getBoundingBox();
    }
    public Map<Enchantment, Set<EnchantmentLocationBasedEffect>> activeLocationDependentEnchantments(EquipmentSlot slot) {
        return java.util.Map.of();
    }
    public void postPiercingAttack() {
        return;
    }
    public Fallsounds getFallSounds() {
        return new Fallsounds(net.minecraft.sounds.SoundEvents.byId("entity.generic.small_fall"), net.minecraft.sounds.SoundEvents.byId("entity.generic.big_fall"));
    }
    public Optional<BlockPos> getLastClimbablePos() {
        return onClimbable() ? Optional.of(blockPosition()) : Optional.empty();
    }
    public boolean onClimbable() {
        org.bukkit.entity.LivingEntity living = living();
        return living != null && living.isClimbing();
    }
    public boolean isAlive() {
        return !isRemoved() && getHealth() > 0.0F;
    }
    public boolean isLookingAtMe(LivingEntity target, double coneSize, boolean adjustForDistance, boolean seeThroughTransparentBlocks, double... gazeHeights) {
        Vec3 view = target.getViewVector(1.0F).normalize();
        for (double y : gazeHeights) {
            Vec3 delta = new Vec3(getX() - target.getX(), y - target.getEyeY(), getZ() - target.getZ());
            double distance = delta.length();
            delta = delta.normalize();
            double threshold = adjustForDistance ? 1.0 - coneSize / distance : 1.0 - coneSize;
            if (view.dot(delta) > threshold && target.hasLineOfSight(this)) {
                return true;
            }
        }
        return false;
    }
    public int getMaxFallDistance() {
        return 3;
    }
    protected int getComfortableFallDistance(float allowedDamage) {
        return net.minecraft.util.Mth.floor(allowedDamage + 3.0F);
    }
    public boolean causeFallDamage(double fallDistance, float damageModifier, DamageSource damageSource) {
        if (fallDistance <= 0.0 || live() == null) {
            return false;
        }
        int damage = calculateFallDamage(fallDistance, damageModifier);
        if (damage > 0) {
            hurtServer(level(), damageSource, damage);
            return true;
        }
        return false;
    }
    public void setIgnoreFallDamageFromCurrentImpulse(boolean ignoreFallDamage, Vec3 newImpulseImpactPos) {
        currentImpulseImpactPos = ignoreFallDamage ? newImpulseImpactPos : null;
    }
    public void applyPostImpulseGraceTime(int ticks) {
        return;
    }
    public boolean isIgnoringFallDamageFromCurrentImpulse() {
        return false;
    }
    public void tryResetCurrentImpulseContext() {
        if (onGround()) {
            resetCurrentImpulseContext();
        }
    }
    public boolean isInPostImpulseGraceTime() {
        return false;
    }
    public void resetCurrentImpulseContext() {
        currentImpulseImpactPos = null;
    }
    protected int calculateFallDamage(double fallDistance, float damageModifier) {
        return net.minecraft.util.Mth.ceil((fallDistance - getAttributeValue(net.minecraft.world.entity.ai.attributes.Attributes.SAFE_FALL_DISTANCE)) * damageModifier
            * getAttributeValue(net.minecraft.world.entity.ai.attributes.Attributes.FALL_DAMAGE_MULTIPLIER));
    }
    protected void playBlockFallSound() {
        return;
    }
    public void animateHurt(float yaw) {
        hurtDuration = 10;
        hurtTime = hurtDuration;
    }
    public int getArmorValue() {
        return Mth.floor(getAttributeValue(Attributes.ARMOR));
    }
    protected void hurtArmor(DamageSource damageSource, float damage) {
        return;
    }
    protected void hurtHelmet(DamageSource damageSource, float damage) {
        return;
    }
    protected void doHurtEquipment(DamageSource damageSource, float damage, EquipmentSlot... slots) {
        return;
    }
    protected float getDamageAfterArmorAbsorb(DamageSource damageSource, float damage) {
        if (damageSource.is(net.minecraft.tags.DamageTypeTags.BYPASSES_ARMOR)) {
            return damage;
        }
        float armor = (float) getArmorValue();
        float toughness = (float) getAttributeValue(Attributes.ARMOR_TOUGHNESS);
        float reduction = Mth.clamp(armor - damage / (2.0F + toughness / 4.0F), armor * 0.2F, 20.0F);
        return damage * (1.0F - reduction / 25.0F);
    }
    protected float getDamageAfterMagicAbsorb(DamageSource damageSource, float damage) {
        if (damageSource.is(net.minecraft.tags.DamageTypeTags.BYPASSES_EFFECTS)) {
            return damage;
        }
        MobEffectInstance resistance = getEffect(net.minecraft.world.effect.MobEffects.RESISTANCE);
        if (resistance != null && !damageSource.is(net.minecraft.tags.DamageTypeTags.BYPASSES_RESISTANCE)) {
            int scale = 25 - (resistance.getAmplifier() + 1) * 5;
            return Math.max(damage * scale / 25.0F, 0.0F);
        }
        return damage;
    }
    protected void actuallyHurt(ServerLevel level, DamageSource source, float dmg) {
        float afterArmor = getDamageAfterMagicAbsorb(source, getDamageAfterArmorAbsorb(source, dmg));
        float absorbed = Math.max(afterArmor - getAbsorptionAmount(), 0.0F);
        setAbsorptionAmount(getAbsorptionAmount() - (afterArmor - absorbed));
        if (absorbed > 0.0F) {
            setHealth(getHealth() - absorbed);
        }
    }
    public CombatTracker getCombatTracker() {
        return new CombatTracker(this);
    }
    public LivingEntity getKillCredit() {
        return getLastHurtByMob();
    }
    public float getMaxHealth() {
        return (float) getAttributeValue(Attributes.MAX_HEALTH);
    }
    public float getMaxAbsorption() {
        return (float) getAttributeValue(Attributes.MAX_ABSORPTION);
    }
    public int getArrowCount() {
        org.bukkit.entity.LivingEntity living = living();
        return living == null ? 0 : living.getArrowsInBody();
    }
    public void setArrowCount(int count) {
        org.bukkit.entity.LivingEntity living = living();
        if (living != null) {
            living.setArrowsInBody(count);
        }
    }
    public int getStingerCount() {
        org.bukkit.entity.LivingEntity living = living();
        return living == null ? 0 : living.getBeeStingersInBody();
    }
    public void setStingerCount(int count) {
        org.bukkit.entity.LivingEntity living = living();
        if (living != null) {
            living.setBeeStingersInBody(count);
        }
    }
    public void swingAndResetAttackStrength(InteractionHand hand, SwingAnimation animation, boolean sendToSwingingEntity) {
        swing(hand, animation, sendToSwingingEntity);
    }
    public boolean swing(InteractionHand hand, SwingAnimation animation, boolean sendToSwingingEntity) {
        if (living() != null) {
            living().swingHand(hand == InteractionHand.MAIN_HAND ? org.bukkit.inventory.EquipmentSlot.HAND : org.bukkit.inventory.EquipmentSlot.OFF_HAND);
        }
        return true;
    }
    public void handleDamageEvent(DamageSource source) {
        hurtDuration = 10;
        hurtTime = hurtDuration;
        makeSound(getHurtSound(source));
    }
    public void handleEntityEvent(byte id) {
        switch (id) {
            case 2, 33, 36, 37, 44, 57 -> hurtTime = hurtDuration = 10;
            case 3 -> deathTime = 1;
            default -> super.handleEntityEvent(id);
        }
    }
    public float getTicksSinceLastKineticHitFeedback(float partial) {
        return 0.0F;
    }
    public void makePoofParticles() {
        if (live() != null) {
            live().getWorld().spawnParticle(org.bukkit.Particle.POOF, live().getLocation().add(0.0, getBbHeight() / 2.0, 0.0), 20, getBbWidth() / 2.0, getBbHeight() / 2.0, getBbWidth() / 2.0, 0.02);
        }
    }
    protected void onBelowWorld() {
        super.onBelowWorld();
    }
    protected double getEntityBounciness() {
        return 0.0;
    }
    public AttributeInstance getAttribute(Holder<Attribute> attribute) {
        return getAttributes().getInstance(attribute);
    }
    public double getAttributeValue(Holder<Attribute> attribute) {
        return getAttributes().getValue(attribute);
    }
    public double getAttributeBaseValue(Holder<Attribute> attribute) {
        return getAttributes().getBaseValue(attribute);
    }
    public AttributeMap getAttributes() {
        if (attributeMap == null) {
            attributeMap = new AttributeMap(net.minecraft.world.entity.ai.attributes.DefaultAttributes.getSupplier((EntityType<? extends LivingEntity>) getType()));
        }
        return attributeMap;
    }
    public ItemStack getMainHandItem() {
        return getItemBySlot(EquipmentSlot.MAINHAND);
    }
    public ItemStack getOffhandItem() {
        return getItemBySlot(EquipmentSlot.OFFHAND);
    }
    public ItemStack getItemHeldByArm(HumanoidArm arm) {
        return getItemBySlot(arm == getMainArm() ? EquipmentSlot.MAINHAND : EquipmentSlot.OFFHAND);
    }
    public ItemStack getWeaponItem() {
        return getMainHandItem();
    }
    public AttackRange getAttackRangeWith(ItemStack weaponItem) {
        return null;
    }
    public ItemStack getActiveItem() {
        return living() == null ? ItemStack.EMPTY : org.bukkit.craftbukkit.inventory.CraftItemStack.asNMSCopy(living().getActiveItem());
    }
    public boolean isHolding(Item item) {
        return isHolding(stack -> stack.is(item));
    }
    public boolean isHolding(Predicate<ItemStack> itemPredicate) {
        return itemPredicate.test(getMainHandItem()) || itemPredicate.test(getOffhandItem());
    }
    public ItemStack getItemInHand(InteractionHand hand) {
        return getItemBySlot(hand == InteractionHand.MAIN_HAND ? EquipmentSlot.MAINHAND : EquipmentSlot.OFFHAND);
    }
    public void setItemInHand(InteractionHand hand, ItemStack itemStack) {
        setItemSlot(hand == InteractionHand.MAIN_HAND ? EquipmentSlot.MAINHAND : EquipmentSlot.OFFHAND, itemStack);
    }
    public boolean hasItemInSlot(EquipmentSlot slot) {
        return !getItemBySlot(slot).isEmpty();
    }
    public boolean canUseSlot(EquipmentSlot slot) {
        return true;
    }
    public ItemStack getItemBySlot(EquipmentSlot slot) {
        org.bukkit.entity.LivingEntity living = living();
        org.bukkit.inventory.EntityEquipment equipment = living == null ? null : living.getEquipment();
        if (equipment == null) {
            return ItemStack.EMPTY;
        }
        return org.bukkit.craftbukkit.inventory.CraftItemStack.asNMSCopy(equipment.getItem(bukkitSlot(slot)));
    }
    public void setItemSlot(EquipmentSlot slot, ItemStack itemStack) {
        org.bukkit.entity.LivingEntity living = living();
        org.bukkit.inventory.EntityEquipment equipment = living == null ? null : living.getEquipment();
        if (equipment != null) {
            equipment.setItem(bukkitSlot(slot), org.bukkit.craftbukkit.inventory.CraftItemStack.asCraftMirror(itemStack));
        }
    }
    public float getArmorCoverPercentage() {
        int covered = 0;
        for (EquipmentSlot slot : new EquipmentSlot[] {EquipmentSlot.HEAD, EquipmentSlot.CHEST, EquipmentSlot.LEGS, EquipmentSlot.FEET}) {
            if (!getItemBySlot(slot).isEmpty()) {
                covered++;
            }
        }
        return covered / 4.0F;
    }
    public void setSprinting(boolean isSprinting) {
        setSharedFlag(3, isSprinting);
    }
    protected float getSoundVolume() {
        return 1.0F;
    }
    public float getVoicePitch() {
        return isBaby() ? (random.nextFloat() - random.nextFloat()) * 0.2F + 1.5F : (random.nextFloat() - random.nextFloat()) * 0.2F + 1.0F;
    }
    protected boolean isImmobile() {
        return isDeadOrDying();
    }
    public void push(Entity entity) {
        super.push(entity);
    }
    public boolean shouldShowName() {
        return false;
    }
    protected float getJumpPower() {
        return getJumpPower(1.0F);
    }
    protected float getJumpPower(float multiplier) {
        return (float) getAttributeValue(net.minecraft.world.entity.ai.attributes.Attributes.JUMP_STRENGTH) * multiplier * getBlockJumpFactor() + getJumpBoostPower();
    }
    public float getJumpBoostPower() {
        return hasEffect(net.minecraft.world.effect.MobEffects.JUMP_BOOST) ? 0.1F * (getEffect(net.minecraft.world.effect.MobEffects.JUMP_BOOST).getAmplifier() + 1.0F) : 0.0F;
    }
    public void jumpFromGround() {
        Vec3 motion = getDeltaMovement();
        setDeltaMovement(motion.x, Math.max(getJumpPower(), motion.y), motion.z);
        if (isSprinting()) {
            float yaw = getYRot() * ((float) Math.PI / 180.0F);
            setDeltaMovement(getDeltaMovement().add(-net.minecraft.util.Mth.sin(yaw) * 0.2F, 0.0, net.minecraft.util.Mth.cos(yaw) * 0.2F));
        }
    }
    protected void goDownInWater() {
        setDeltaMovement(getDeltaMovement().add(0.0, -0.04F, 0.0));
    }
    protected void jumpInLiquid(TagKey<Fluid> type) {
        setDeltaMovement(getDeltaMovement().add(0.0, 0.04F, 0.0));
    }
    protected float getWaterSlowDown() {
        return 0.8F;
    }
    public boolean canStandOnFluid(FluidState fluid) {
        return false;
    }
    protected double getDefaultGravity() {
        return 0.08;
    }
    protected double getEffectiveGravity() {
        double gravity = getGravity();
        boolean falling = getDeltaMovement().y <= 0.0;
        return falling && hasEffect(net.minecraft.world.effect.MobEffects.SLOW_FALLING) ? Math.min(gravity, 0.01) : gravity;
    }
    public void travel(Vec3 input) {
        if (isNoGravity() && !isInWater() && !isInLava()) {
            travelFlying(input, getSpeed());
            return;
        }
        FluidState fluid = level().getFluidState(blockPosition());
        if (shouldTravelInFluid(fluid)) {
            travelInFluid(input);
            return;
        }
        float friction = onGround() ? 0.91F * 0.6F : 0.91F;
        moveRelative(onGround() ? getSpeed() * (0.21600002F / (0.6F * 0.6F * 0.6F)) : 0.02F, input);
        move(MoverType.SELF, getDeltaMovement());
        Vec3 motion = getDeltaMovement();
        double fall = isNoGravity() ? 0.0 : getEffectiveGravity();
        setDeltaMovement(motion.x * friction, (motion.y - fall) * 0.98F, motion.z * friction);
    }
    public VoxelShape getLiquidCollisionShape() {
        return net.minecraft.world.phys.shapes.Shapes.empty();
    }
    protected boolean shouldTravelInFluid(FluidState fluidState) {
        return (isInWater() || isInLava()) && isAffectedByFluids() && !canStandOnFluid(fluidState);
    }
    protected void travelFlying(Vec3 input, float speed) {
        moveRelative(speed, input);
        move(MoverType.SELF, getDeltaMovement());
        setDeltaMovement(getDeltaMovement().scale(0.91F));
    }
    protected void travelFlying(Vec3 input, float waterSpeed, float lavaSpeed, float airSpeed) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.entity.LivingEntity#travelFlying");
    }
    protected float getAirDrag() {
        return 0.98F;
    }
    protected void travelInFluid(Vec3 input) {
        boolean lava = isInLava();
        double oldY = getY();
        double fall = isNoGravity() ? 0.0 : getEffectiveGravity() / 16.0;
        moveRelative(0.02F, input);
        move(MoverType.SELF, getDeltaMovement());
        Vec3 motion = getDeltaMovement().scale(lava ? 0.5F : getWaterSlowDown());
        setDeltaMovement(motion.x, motion.y - fall, motion.z);
    }
    protected void travelInWater(Vec3 input, double baseGravity, boolean isFalling, double oldY) {
        moveRelative(0.02F, input);
        move(MoverType.SELF, getDeltaMovement());
        setDeltaMovement(getFluidFallingAdjustedMovement(baseGravity, isFalling, getDeltaMovement().scale(getWaterSlowDown())));
    }
    protected boolean isInShallowFluid(TagKey<Fluid> fluidTag) {
        return isInFluidDeeperThan(0.0, fluidTag) && !isInFluidDeeperThan(0.4, fluidTag);
    }
    public boolean isInFluidDeeperThan(double depth, TagKey<Fluid> fluidTag) {
        return getFluidHeight(fluidTag) > depth;
    }
    public void stopFallFlying() {
        if (live() != null) {
            living().setGliding(false);
        }
    }
    protected void tickRidden(Player controller, Vec3 riddenInput) {
        return;
    }
    protected Vec3 getRiddenInput(Player controller, Vec3 selfInput) {
        return selfInput;
    }
    protected float getRiddenSpeed(Player controller) {
        return getSpeed();
    }
    public void calculateEntityAnimation(boolean useY) {
        double dx = getX() - xo;
        double dz = getZ() - zo;
        double dy = useY ? getY() - yo : 0.0;
        updateWalkAnimation((float) Math.sqrt(dx * dx + dy * dy + dz * dz) * 4.0F);
    }
    protected void updateWalkAnimation(float distance) {
        return;
    }
    public Vec3 getFluidFallingAdjustedMovement(double baseGravity, boolean isFalling, Vec3 movement) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.entity.LivingEntity#getFluidFallingAdjustedMovement");
    }
    protected float getFlyingSpeed() {
        return getControllingPassenger() instanceof net.minecraft.world.entity.player.Player ? getSpeed() * 0.1F : 0.02F;
    }
    public float getSpeed() {
        return speed;
    }
    public void setSpeed(float speed) {
        this.speed = speed;
    }
    public boolean doHurtTarget(ServerLevel level, Entity target) {
        if (target == null || target.live() == null || live() == null) {
            return false;
        }
        double damage = getAttributeValue(Attributes.ATTACK_DAMAGE);
        if (target.live() instanceof org.bukkit.entity.Damageable victim) {
            victim.damage(damage, live());
            return true;
        }
        return false;
    }
    public void causeExtraKnockback(Entity target, float knockback, Vec3 oldMovement, DamageSource damageSource, float damage, boolean comesFromEffect) {
        if (knockback > 0.0F && target instanceof LivingEntity victim) {
            victim.knockback(knockback * 0.5F, Mth.sin(getYRot() * ((float) Math.PI / 180.0F)), -Mth.cos(getYRot() * ((float) Math.PI / 180.0F)), damageSource, damage, comesFromEffect);
            setDeltaMovement(getDeltaMovement().multiply(0.6, 1.0, 0.6));
        }
    }
    protected void playAttackSound() {
        return;
    }
    public void tick() {
        baseTick();
    }
    public boolean wasRecentlyStabbed(Entity target, int allowedTime) {
        if (stabbedTimes == null) {
            return false;
        }
        Long stamp = stabbedTimes.get(target);
        return stamp != null && tickCount - stamp <= allowedTime;
    }
    public void rememberStabbedEntity(Entity target) {
        if (stabbedTimes == null) {
            stabbedTimes = new java.util.WeakHashMap<>();
        }
        stabbedTimes.put(target, (long) tickCount);
    }
    public int stabbedEntities(Predicate<Entity> filter) {
        if (stabbedTimes == null) {
            return 0;
        }
        int count = 0;
        for (Entity entity : stabbedTimes.keySet()) {
            if (filter.test(entity)) {
                count++;
            }
        }
        return count;
    }
    public boolean stabAttack(EquipmentSlot weaponSlot, Entity target, float baseDamage, boolean dealsDamage, boolean dealsKnockback, boolean dismounts) {
        if (!dealsDamage) {
            return false;
        }
        boolean hit = doHurtTarget(level(), target);
        if (hit) {
            rememberStabbedEntity(target);
        }
        return hit;
    }
    public void onAttack() {
        return;
    }
    protected Map<EquipmentSlot, ItemStack> collectEquipmentChanges(Map<EquipmentSlot, ItemStack> lastEquipmentItems) {
        Map<EquipmentSlot, ItemStack> changed = new java.util.EnumMap<>(EquipmentSlot.class);
        for (EquipmentSlot slot : EquipmentSlot.values()) {
            ItemStack now = getItemBySlot(slot);
            ItemStack before = lastEquipmentItems.get(slot);
            if (before == null || equipmentHasChanged(before, now)) {
                changed.put(slot, now);
            }
        }
        return changed;
    }
    public boolean equipmentHasChanged(ItemStack previous, ItemStack current) {
        return !ItemStack.matches(previous, current);
    }
    protected void tickHeadTurn(float yBodyRotT) {
        float diff = Mth.wrapDegrees(yBodyRotT - yBodyRot);
        yBodyRot += diff * 0.3F;
        float head = Mth.clamp(Mth.wrapDegrees(getYRot() - yBodyRot), -getMaxHeadRotationRelativeToBody(), getMaxHeadRotationRelativeToBody());
        yBodyRot = getYRot() - head;
    }
    protected float getMaxHeadRotationRelativeToBody() {
        return 50.0F;
    }
    public void aiStep() {
        return;
    }
    protected void applyInput() {
        return;
    }
    public boolean isSensitiveToWater() {
        return false;
    }
    public boolean isJumping() {
        return jumping;
    }
    protected void updateFallFlying() {
        return;
    }
    protected boolean canGlide() {
        return false;
    }
    protected void serverAiStep() {
        return;
    }
    protected void pushEntities() {
        if (live() == null) {
            return;
        }
        for (Entity near : level().getEntities(this, getBoundingBox(), Entity::isPushable)) {
            doPush(near);
        }
    }
    protected void checkAutoSpinAttack(AABB old, AABB current) {
        return;
    }
    protected void doPush(Entity entity) {
        entity.push(this);
    }
    protected void doAutoAttackOnTouch(LivingEntity entity) {
        return;
    }
    public boolean isAutoSpinAttack() {
        return false;
    }
    public void stopRiding() {
        if (living() != null) {
            living().leaveVehicle();
        }
    }
    public void rideTick() {
        setDeltaMovement(Vec3.ZERO);
        baseTick();
    }
    protected InterpolationHandler createInterpolationHandler() {
        return null;
    }
    public void lerpHeadTo(float yRot, int steps) {
        setYHeadRot(yRot);
    }
    public void setJumping(boolean jump) {
        jumping = jump;
    }
    public void onItemPickup(ItemEntity entity) {
        return;
    }
    public void take(Entity entity, int orgCount) {
        return;
    }
    public boolean hasLineOfSight(Entity target) {
        org.bukkit.entity.LivingEntity living = living();
        org.bukkit.entity.Entity other = target.live();
        return living != null && other != null && living.hasLineOfSight(other);
    }
    public boolean hasLineOfSight(Entity target, ClipContext.Block blockCollidingContext, ClipContext.Fluid fluidCollidingContext, double eyeHeight) {
        return living() != null && target.live() != null && living().hasLineOfSight(target.live());
    }
    public float getViewYRot(float a) {
        return getYRot();
    }
    public SwingDescription getCurrentSwing() {
        return null;
    }
    public float getSwingAnimation(float partialTicks) {
        return 0.0F;
    }
    public boolean isSwinging() {
        return false;
    }
    public boolean isPickable() {
        return !isRemoved();
    }
    public boolean isPushable() {
        return isAlive() && !isSpectator() && !onClimbable();
    }
    public float getYHeadRot() {
        org.bukkit.entity.LivingEntity living = living();
        return living != null ? living.getEyeLocation().getYaw() : getYRot();
    }
    public void setYHeadRot(float yHeadRot) {

    }
    public void setYBodyRot(float yBodyRot) {
        yBodyRot = yBodyRot;
    }
    public Vec3 getRelativePortalPosition(Direction.Axis axis, BlockUtil.FoundRectangle portalArea) {
        return Vec3.ZERO;
    }
    public static Vec3 resetForwardDirectionOfRelativePortalPosition(Vec3 offsets) {
        return new Vec3(offsets.x, offsets.y, 0.0);
    }
    public float getAbsorptionAmount() {
        org.bukkit.entity.LivingEntity living = living();
        return living == null ? 0.0F : (float) living.getAbsorptionAmount();
    }
    public void setAbsorptionAmount(float absorptionAmount) {
        org.bukkit.entity.LivingEntity living = living();
        if (living != null) {
            living.setAbsorptionAmount(Math.max(0.0F, Math.min(absorptionAmount, getMaxAbsorption())));
        }
    }
    protected void internalSetAbsorptionAmount(float absorptionAmount) {
        setAbsorptionAmount(absorptionAmount);
    }
    public void onEnterCombat() {
        return;
    }
    public void onLeaveCombat() {
        return;
    }
    protected void updateEffectVisibility() {
        updateInvisibilityStatus();
    }
    public HumanoidArm getMainArm() {
        return HumanoidArm.RIGHT;
    }
    public boolean isUsingItem() {
        org.bukkit.entity.LivingEntity living = living();
        return living != null && living.isHandRaised();
    }
    public InteractionHand getUsedItemHand() {
        return InteractionHand.MAIN_HAND;
    }
    public ItemEntity createItemStackToDrop(ItemStack itemStack, boolean randomly, boolean thrownFromHand) {
        if (itemStack.isEmpty()) {
            return null;
        }
        return spawnAtLocation(level(), itemStack, getEyeHeight() - 0.3F);
    }
    protected void updateUsingItem(ItemStack useItem) {
        return;
    }
    protected void setLivingEntityFlag(int flag, boolean value) {
        return;
    }
    public void startUsingItem(InteractionHand hand) {
        if (living() != null) {
            living().startUsingItem(hand == InteractionHand.MAIN_HAND ? org.bukkit.inventory.EquipmentSlot.HAND : org.bukkit.inventory.EquipmentSlot.OFF_HAND);
        }
    }
    public void onSyncedDataUpdated(EntityDataAccessor<?> accessor) {
        return;
    }
    public void lookAt(EntityAnchorArgument.Anchor anchor, Vec3 pos) {
        Vec3 from = anchor.apply(this);
        double dx = pos.x - from.x;
        double dy = pos.y - from.y;
        double dz = pos.z - from.z;
        double horizontal = Math.sqrt(dx * dx + dz * dz);
        setXRot(Mth.wrapDegrees((float) -(Mth.atan2(dy, horizontal) * 180.0F / (float) Math.PI)));
        setYRot(Mth.wrapDegrees((float) (Mth.atan2(dz, dx) * 180.0F / (float) Math.PI) - 90.0F));
        setYHeadRot(getYRot());
    }
    public float getPreciseBodyRotation(float partial) {
        return getYRot();
    }
    public void spawnItemParticles(ItemStack itemStack, int count) {
        if (live() != null && !itemStack.isEmpty()) {
            live().getWorld().spawnParticle(org.bukkit.Particle.ITEM, live().getLocation().add(0.0, getEyeHeight(), 0.0), count, 0.1, 0.1, 0.1, 0.05,
                org.bukkit.craftbukkit.inventory.CraftItemStack.asCraftMirror(itemStack));
        }
    }
    protected void completeUsingItem() {
        if (living() != null) {
            living().completeUsingActiveItem();
        }
    }
    public void handleExtraItemsCreatedOnUse(ItemStack extraCreatedRemainder) {
        if (!extraCreatedRemainder.isEmpty() && living() != null) {
            org.bukkit.entity.LivingEntity holder = living();
            if (holder instanceof org.bukkit.entity.HumanEntity human) {
                human.getInventory().addItem(org.bukkit.craftbukkit.inventory.CraftItemStack.asCraftMirror(extraCreatedRemainder)).values()
                    .forEach(left -> holder.getWorld().dropItem(holder.getLocation(), left));
            } else {
                holder.getWorld().dropItem(holder.getLocation(), org.bukkit.craftbukkit.inventory.CraftItemStack.asCraftMirror(extraCreatedRemainder));
            }
        }
    }
    public ItemStack getUseItem() {
        return getActiveItem();
    }
    public int getUseItemRemainingTicks() {
        return live() instanceof org.bukkit.entity.LivingEntity api ? api.getItemUseRemainingTime() : 0;
    }
    public int getTicksUsingItem() {
        return live() instanceof org.bukkit.entity.LivingEntity api ? api.getActiveItemUsedTime() : 0;
    }
    public float getTicksUsingItem(float partialTicks) {
        return getTicksUsingItem() + partialTicks;
    }
    public void releaseUsingItem() {
        stopUsingItem();
    }
    public void stopUsingItem() {
        if (living() != null) {
            living().clearActiveItem();
        }
    }
    public boolean isBlocking() {
        org.bukkit.entity.LivingEntity living = living();
        return living instanceof org.bukkit.entity.HumanEntity human && human.isBlocking();
    }
    public ItemStack getItemBlockingWith() {
        return ItemStack.EMPTY;
    }
    public boolean isSuppressingSlidingDownLadder() {
        return isShiftKeyDown();
    }
    public boolean isFallFlying() {
        org.bukkit.entity.LivingEntity living = living();
        return living != null && living.isGliding();
    }
    public boolean isVisuallySwimming() {
        return super.isVisuallySwimming();
    }
    public int getFallFlyingTicks() {
        return 0;
    }
    public boolean randomTeleport(double xx, double yy, double zz, boolean showParticles, TagKey<Block> avoidanceTag) {
        return randomTeleport(xx, yy, zz, showParticles, state -> state.is(avoidanceTag));
    }
    public boolean randomTeleport(double xx, double yy, double zz, boolean showParticles, Predicate<BlockState> isInvalidPosition) {
        if (live() == null) {
            return false;
        }
        BlockPos.MutableBlockPos pos = new BlockPos.MutableBlockPos(xx, yy, zz);
        while (pos.getY() > level().getWorld().getMinHeight() && !level().getBlockState(pos).isSolid()) {
            pos.move(Direction.DOWN);
        }
        BlockState ground = level().getBlockState(pos);
        if (!ground.isSolid() || isInvalidPosition.test(ground)) {
            return false;
        }
        double targetY = pos.getY() + 1.0;
        if (!canRandomlyTeleportTo(xx, targetY, zz)) {
            return false;
        }
        org.bukkit.Location from = live().getLocation();
        boolean moved = live().teleport(new org.bukkit.Location(live().getWorld(), xx, targetY, zz, from.getYaw(), from.getPitch()));
        if (moved && showParticles) {
            live().getWorld().spawnParticle(org.bukkit.Particle.PORTAL, from, 32, 0.5, 1.0, 0.5, 0.1);
        }
        return moved;
    }
    public boolean canRandomlyTeleportTo(double x, double y, double z) {
        AABB moved = getBoundingBox().move(x - getX(), y - getY(), z - getZ());
        return level().noCollision(this, moved) && !level().containsAnyLiquid(moved);
    }
    public boolean isAffectedByPotions() {
        return true;
    }
    public boolean attackable() {
        return true;
    }
    public void setRecordPlayingNearby(BlockPos jukebox, boolean isPlaying) {
        return;
    }
    public boolean canPickUpLoot() {
        org.bukkit.entity.LivingEntity living = living();
        return living != null && living.getCanPickupItems();
    }
    public EntityDimensions getDimensions(Pose pose) {
        return EntityDimensions.scalable(getBbWidth(), getBbHeight());
    }
    protected EntityDimensions getDefaultDimensions(Pose pose) {
        return EntityDimensions.scalable(getBbWidth(), getBbHeight());
    }
    public ImmutableList<Pose> getDismountPoses() {
        return ImmutableList.of(Pose.STANDING, Pose.CROUCHING, Pose.SWIMMING);
    }
    public AABB getLocalBoundsForPose(Pose pose) {
        EntityDimensions size = getDimensions(pose);
        return new AABB(-size.width() / 2.0F, 0.0, -size.width() / 2.0F, size.width() / 2.0F, size.height(), size.width() / 2.0F);
    }
    protected boolean wouldNotSuffocateAtTargetPose(Pose pose) {
        AABB bounds = getLocalBoundsForPose(pose).move(position());
        return level().noCollision(this, bounds);
    }
    public boolean canUsePortal(boolean ignorePassenger) {
        return !isPassenger() && (ignorePassenger || !isVehicle());
    }
    public Optional<BlockPos> getSleepingPos() {
        return Optional.ofNullable(sleepingPos);
    }
    public void setSleepingPos(BlockPos bedPosition) {
        sleepingPos = bedPosition;
    }
    public void clearSleepingPos() {
        sleepingPos = null;
    }
    public boolean isSleeping() {
        return sleepingPos != null;
    }
    public boolean startSleeping(BlockPos bedPosition) {
        if (live() instanceof org.bukkit.entity.HumanEntity human) {
            return human.sleep(new org.bukkit.Location(human.getWorld(), bedPosition.getX(), bedPosition.getY(), bedPosition.getZ()), true);
        }
        sleepingPos = bedPosition;
        return true;
    }
    public void stopSleeping() {
        if (live() instanceof org.bukkit.entity.HumanEntity human) {
            human.wakeup(true);
        }
        sleepingPos = null;
    }
    public Direction getBedOrientation() {
        return null;
    }
    public boolean isInWall() {
        return false;
    }
    public ItemStack getProjectile(ItemStack heldWeapon) {
        return ItemStack.EMPTY;
    }
    public void onEquippedItemBroken(ItemStack brokenItem, EquipmentSlot inSlot) {
        return;
    }
    public boolean canEquipWithDispenser(ItemStack itemStack) {
        return itemStack.has(net.minecraft.core.component.DataComponents.EQUIPPABLE) && canDispenserEquipIntoSlot(getEquipmentSlotForItem(itemStack));
    }
    protected boolean canDispenserEquipIntoSlot(EquipmentSlot slot) {
        return canUseSlot(slot);
    }
    public EquipmentSlot getEquipmentSlotForItem(ItemStack itemStack) {
        net.minecraft.world.item.equipment.Equippable equippable = itemStack.get(net.minecraft.core.component.DataComponents.EQUIPPABLE);
        return equippable != null ? equippable.slot() : EquipmentSlot.MAINHAND;
    }
    public boolean isEquippableInSlot(ItemStack itemStack, EquipmentSlot slot) {
        net.minecraft.world.item.equipment.Equippable equippable = itemStack.get(net.minecraft.core.component.DataComponents.EQUIPPABLE);
        return equippable == null ? slot == EquipmentSlot.MAINHAND || slot == EquipmentSlot.OFFHAND : equippable.slot() == slot;
    }
    public SlotAccess getSlot(int slot) {
        for (EquipmentSlot equipment : EquipmentSlot.VALUES) {
            int base = switch (equipment.getType()) {
                case HAND -> 98;
                case HUMANOID_ARMOR -> 100;
                case ANIMAL_ARMOR -> 105;
                case SADDLE -> 106;
            };
            if (equipment.getIndex(base) == slot) {
                return SlotAccess.forEquipmentSlot(this, equipment);
            }
        }
        return super.getSlot(slot);
    }
    public boolean canFreeze() {
        return true;
    }
    public boolean isCurrentlyGlowing() {
        return getSharedFlag(6);
    }
    public float getVisualRotationYInDegrees() {
        return getYRot();
    }
    public void recreateFromPacket(ClientboundAddEntityPacket packet) {
        return;
    }
    public float getSecondsToDisableBlocking() {
        return 0.0F;
    }
    public float maxUpStep() {
        return (float) getAttributeValue(Attributes.STEP_HEIGHT);
    }
    public Vec3 getPassengerRidingPosition(Entity passenger) {
        return position().add(0.0, getBbHeight() * 0.75, 0.0);
    }
    protected void lerpHeadRotationStep(int lerpHeadSteps, double targetYHeadRot) {
        return;
    }
    public void igniteForTicks(int numberOfTicks) {
        if (living() != null) {
            living().setFireTicks(numberOfTicks);
        }
    }
    public boolean hasInfiniteMaterials() {
        return false;
    }
    public boolean isInvulnerableTo(ServerLevel level, DamageSource source) {
        return isInvulnerable();
    }
    public static boolean canGlideUsing(ItemStack itemStack, EquipmentSlot slot) {
        return itemStack.has(net.minecraft.core.component.DataComponents.GLIDER);
    }
    public int getLastHurtByPlayerMemoryTime() {
        return 0;
    }
    public boolean isTransmittingWaypoint() {
        return false;
    }
    public Optional<WaypointTransmitter.Connection> makeWaypointConnectionWith(ServerPlayer player) {
        return Optional.empty();
    }
    public Waypoint.Icon waypointIcon() {
        return new Waypoint.Icon();
    }
    public DamageSource createDamageSource() {
        return damageSources().mobAttack(this);
    }
    public static record Fallsounds(SoundEvent small, SoundEvent big) {
    }
    public static class SwingState {
        private InteractionHand hand;
        private int duration;
        private int tickCount;
        public boolean startIfAble(InteractionHand hand, SwingAnimation animation, int durationTicks) {
            if (isSwinging() && tickCount < duration / 2) {
                return false;
            }
            this.hand = hand;
            this.duration = Math.max(durationTicks, 1);
            this.tickCount = 0;
            return true;
        }
        public void tick() {
            if (hand != null && ++tickCount >= duration) {
                hand = null;
            }
        }
        public float getAnimation(float partialTicks) {
            return hand == null ? 0.0F : Math.min((tickCount + partialTicks) / duration, 1.0F);
        }
        public boolean isSwinging() {
            return hand != null;
        }
    }
    public static record SwingDescription(InteractionHand hand, SwingAnimation animation, int durationTicks) {
    }

    private java.util.Map<Entity, Long> stabbedTimes;
    private DamageSource lastDamageSourceRef;
    private long lastDamageStamp = Long.MIN_VALUE / 2;
    private LivingEntity lastHurtByMobRef;
    private int lastHurtByMobStamp;
    private LivingEntity lastHurtMobRef;
    private int lastHurtMobStamp;
    private Player lastHurtByPlayerRef;

    private net.minecraft.resources.ResourceKey<LootTable> defaultLootKey() {
        net.minecraft.resources.Identifier id = EntityType.getKey(getType());
        return (net.minecraft.resources.ResourceKey<LootTable>) (net.minecraft.resources.ResourceKey<?>) net.minecraft.resources.ResourceKey.create(
            (net.minecraft.resources.ResourceKey) net.minecraft.core.registries.Registries.LOOT_TABLE,
            net.minecraft.resources.Identifier.fromNamespaceAndPath(id.getNamespace(), "entities/" + id.getPath()));
    }

    private LootParams lootParams(LootParams.Builder builder) {
        return new LootParams(builder.getLevel(), builder.contextMap(), java.util.Map.of(), 0.0F);
    }

}
