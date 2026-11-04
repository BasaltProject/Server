// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity;
import java.util.Map;
import java.util.Optional;
import java.util.Set;
import java.util.function.Predicate;
import net.minecraft.world.entity.ai.attributes.Attributes;
import net.minecraft.core.BlockPos;
import net.minecraft.core.Holder;
import net.minecraft.core.Vec3i;
import net.minecraft.network.syncher.SynchedEntityData;
import net.minecraft.resources.Identifier;
import net.minecraft.resources.ResourceKey;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.sounds.SoundEvent;
import net.minecraft.tags.TagKey;
import net.minecraft.util.RandomSource;
import net.minecraft.util.debug.DebugValueSource;
import net.minecraft.world.Container;
import net.minecraft.world.DifficultyInstance;
import net.minecraft.world.InteractionHand;
import net.minecraft.world.InteractionResult;
import net.minecraft.world.damagesource.DamageSource;
import net.minecraft.world.entity.ai.attributes.Attribute;
import net.minecraft.world.entity.ai.attributes.AttributeSupplier;
import net.minecraft.world.entity.ai.control.BodyRotationControl;
import net.minecraft.world.entity.ai.control.JumpControl;
import net.minecraft.world.entity.ai.control.LookControl;
import net.minecraft.world.entity.ai.control.MoveControl;
import net.minecraft.world.entity.ai.goal.Goal;
import net.minecraft.world.entity.ai.goal.GoalSelector;
import net.minecraft.world.entity.ai.navigation.PathNavigation;
import net.minecraft.world.entity.ai.sensing.Sensing;
import net.minecraft.world.entity.item.ItemEntity;
import net.minecraft.world.entity.player.Player;
import net.minecraft.world.item.Item;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.item.component.SwingAnimation;
import net.minecraft.world.level.Level;
import net.minecraft.world.level.LevelAccessor;
import net.minecraft.world.level.LevelReader;
import net.minecraft.world.level.ServerLevelAccessor;
import net.minecraft.world.level.material.Fluid;
import net.minecraft.world.level.pathfinder.PathType;
import net.minecraft.world.level.storage.ValueInput;
import net.minecraft.world.level.storage.ValueOutput;
import net.minecraft.world.level.storage.loot.LootTable;
import net.minecraft.world.phys.AABB;
import net.minecraft.world.phys.Vec3;
public abstract class Mob extends LivingEntity implements Targeting, EquipmentUser, Leashable {
    protected static final int PICKUP_REACH = 1;
    public static final float MAX_WEARING_ARMOR_CHANCE = 0.15F;
    public static final float WEARING_ARMOR_UPGRADE_MATERIAL_CHANCE = 0.1087F;
    public static final float WEARING_ARMOR_UPGRADE_MATERIAL_ATTEMPTS = 3.0F;
    public static final float MAX_PICKUP_LOOT_CHANCE = 0.55F;
    public static final float MAX_ENCHANTED_ARMOR_CHANCE = 0.5F;
    public static final float MAX_ENCHANTED_WEAPON_CHANCE = 0.25F;
    public static final int UPDATE_GOAL_SELECTOR_EVERY_N_TICKS = 2;
    protected static final Identifier RANDOM_SPAWN_BONUS_ID = Identifier.withDefaultNamespace("random_spawn_bonus");
    public static final String TAG_DROP_CHANCES = "drop_chances";
    public static final String TAG_LEFT_HANDED = "LeftHanded";
    public static final String TAG_CAN_PICK_UP_LOOT = "CanPickUpLoot";
    public static final String TAG_NO_AI = "NoAI";
    public static final String TAG_PERSISTENCE_REQUIRED = "PersistenceRequired";
    public int ambientSoundTime;
    protected int xpReward;
    protected LookControl lookControl;
    protected MoveControl moveControl;
    protected JumpControl jumpControl;
    protected PathNavigation navigation;
    protected final GoalSelector goalSelector = new GoalSelector(this);
    protected final GoalSelector targetSelector = new GoalSelector(this);
    private final net.minecraft.world.entity.ai.sensing.Sensing sensing = new net.minecraft.world.entity.ai.sensing.Sensing(this);
    private final java.util.Map<PathType, Float> pathfindingMalus = new java.util.EnumMap<>(PathType.class);
    public float zza;
    public float yya;
    public float xxa;
    private boolean persistenceRequired;
    private BlockPos homePos = BlockPos.ZERO;
    private int homeRadius = -1;

    public org.bukkit.entity.Entity live() {
        return super.live();
    }

    private org.bukkit.entity.Mob mob() {
        return live() instanceof org.bukkit.entity.Mob mob ? mob : null;
    }

    protected Mob(EntityType<? extends Mob> type, Level level) {
        super(type, level);
        if (!org.basaltmc.bukkit.facade.Species.viewing()) {
            registerGoals();
        }
    }

    protected void onSpawned() {
        goalSelector.syncNative();
        targetSelector.syncNative();
    }
    protected void registerGoals() {
        return;
    }
    public static AttributeSupplier.Builder createMobAttributes() {
        return LivingEntity.createLivingAttributes().add(Attributes.FOLLOW_RANGE, 16.0).add(Attributes.TEMPT_RANGE, 10.0);
    }
    protected PathNavigation createNavigation(Level level) {
        return new net.minecraft.world.entity.ai.navigation.GroundPathNavigation(this, level);
    }
    protected boolean shouldPassengersInheritMalus() {
        return false;
    }
    public float getPathfindingMalus(PathType pathType) {
        Float override = pathfindingMalus.get(pathType);
        return override != null ? override : pathType.getMalus();
    }
    public void setPathfindingMalus(PathType pathType, float cost) {
        pathfindingMalus.put(pathType, cost);
    }
    public void onPathfindingStart() {
        return;
    }
    public void onPathfindingDone() {
        return;
    }
    protected BodyRotationControl createBodyControl() {
        return new BodyRotationControl(this);
    }
    public LookControl getLookControl() {
        if (lookControl == null) {
            lookControl = new LookControl(this);
        }
        return lookControl;
    }
    public MoveControl getMoveControl() {
        if (moveControl == null) {
            moveControl = new MoveControl(this);
        }
        return moveControl;
    }
    public JumpControl getJumpControl() {
        if (jumpControl == null) {
            jumpControl = new JumpControl(this);
        }
        return jumpControl;
    }
    public PathNavigation getNavigation() {
        if (navigation == null) {
            navigation = createNavigation(level());
        }
        return navigation;
    }
    public LivingEntity getControllingPassenger() {
        return getFirstPassenger() instanceof LivingEntity rider && !isNoAi() ? rider : null;
    }
    public Sensing getSensing() {
        return sensing;
    }
    public LivingEntity getTarget() {
        return getTargetUnchecked();
    }
    public LivingEntity getTargetUnchecked() {
        org.bukkit.entity.Mob mob = mob();
        org.bukkit.entity.LivingEntity target = mob == null ? null : mob.getTarget();
        return target == null ? null : (LivingEntity) wrap(target);
    }
    protected LivingEntity asValidTarget(LivingEntity target) {
        return target;
    }
    protected final LivingEntity getTargetFromBrain() {
        return brain == null ? null : getBrain().getMemory(net.minecraft.world.entity.ai.memory.MemoryModuleType.ATTACK_TARGET).orElse(null);
    }
    public void setTarget(LivingEntity target) {
        org.bukkit.entity.Mob mob = mob();
        if (mob != null) {
            mob.setTarget(target == null || !(target.live() instanceof org.bukkit.entity.LivingEntity living) ? null : living);
        }
    }
    public boolean canAttack(LivingEntity target) {
        return target.canBeSeenAsEnemy();
    }
    public boolean canUseNonMeleeWeapon(ItemStack item) {
        return false;
    }
    public void ate() {
        return;
    }
    protected void defineSynchedData(SynchedEntityData.Builder entityData) {

    }
    public int getAmbientSoundInterval() {
        return 80;
    }
    public void playAmbientSound() {
        SoundEvent ambient = getAmbientSound();
        if (ambient != null) {
            playSound(ambient, getSoundVolume(), getVoicePitch());
        }
    }
    public void baseTick() {
        super.baseTick();
    }
    protected void playHurtSound(DamageSource source) {
        super.playHurtSound(source);
    }
    protected int getBaseExperienceReward(ServerLevel level) {
        return xpReward;
    }
    public void spawnAnim() {
        return;
    }
    public void handleEntityEvent(byte id) {

    }
    public void tick() {

    }
    protected void updateControlFlags() {

    }
    protected void tickHeadTurn(float yBodyRotT) {

    }
    protected SoundEvent getAmbientSound() {
        return null;
    }
    protected void addAdditionalSaveData(ValueOutput output) {

    }
    protected void readAdditionalSaveData(ValueInput input) {

    }
    protected void dropFromLootTable(ServerLevel level, DamageSource source, boolean playerKilled) {

    }
    public final Optional<ResourceKey<LootTable>> getLootTable() {
        return Optional.empty();
    }
    public long getLootTableSeed() {
        return 0L;
    }
    public void setZza(float zza) {
        this.zza = zza;
    }
    public void setYya(float yya) {
        this.yya = yya;
    }
    public void setXxa(float xxa) {
        this.xxa = xxa;
    }
    public void setSpeed(float speed) {
        super.setSpeed(speed);
        setZza(speed);
    }
    public void stopInPlace() {
        setZza(0.0F);
    }
    public void aiStep() {

    }
    protected EquipmentSlot sunProtectionSlot() {
        return EquipmentSlot.HEAD;
    }
    protected Vec3i getPickupReach() {
        return new Vec3i(1, 0, 1);
    }
    protected void pickUpItem(ServerLevel level, ItemEntity entity) {

    }
    public ItemStack equipItemIfPossible(ServerLevel level, ItemStack itemStack) {
        return itemStack;
    }
    protected void setItemSlotAndDropWhenKilled(EquipmentSlot slot, ItemStack itemStack) {
        setItemSlot(slot, itemStack);
        setGuaranteedDrop(slot);
    }
    protected boolean canShearEquipment(Player player) {
        return false;
    }
    protected boolean attemptToShearEquipment(Player player, InteractionHand hand, ItemStack heldItem) {
        return false;
    }
    protected void shearItem(Player player, InteractionHand hand, ItemStack heldItem, EquipmentSlot slot, ItemStack itemStackToShear) {

    }
    public void setGuaranteedDrop(EquipmentSlot slot) {
        setDropChance(slot, 2.0F);
    }
    protected boolean canReplaceCurrentItem(ItemStack newItemStack, ItemStack currentItemStack, EquipmentSlot slot) {
        return !newItemStack.isEmpty() && currentItemStack.isEmpty();
    }
    public boolean canReplaceEqualItem(ItemStack newItemStack, ItemStack currentItemStack) {
        return newItemStack.getCount() > currentItemStack.getCount();
    }
    public boolean canHoldItem(ItemStack itemStack) {
        return true;
    }
    public boolean wantsToPickUp(ServerLevel level, ItemStack itemStack) {
        return canHoldItem(itemStack);
    }
    public TagKey<Item> getPreferredWeaponType() {
        return null;
    }
    public boolean removeWhenFarAway(double distSqr) {
        return true;
    }
    public boolean requiresCustomPersistence() {
        return false;
    }
    public void checkDespawn() {

    }
    protected final void serverAiStep() {

    }
    protected void customServerAiStep(ServerLevel level) {

    }
    public int getMaxHeadXRot() {
        return 40;
    }
    public int getMaxHeadYRot() {
        return 75;
    }
    public void clampHeadRotationToBody() {

    }
    public int getHeadRotSpeed() {
        return 10;
    }
    public void lookAt(Entity entity, float yMax, float xMax) {
        double dx = entity.getX() - getX();
        double dz = entity.getZ() - getZ();
        double dy = entity instanceof LivingEntity target ? target.getEyeY() - getEyeY() : (entity.getBoundingBox().minY + entity.getBoundingBox().maxY) / 2.0 - getEyeY();
        double horizontal = Math.sqrt(dx * dx + dz * dz);
        float wantedYaw = (float) (Math.atan2(dz, dx) * 180.0 / Math.PI) - 90.0F;
        float wantedPitch = (float) -(Math.atan2(dy, horizontal) * 180.0 / Math.PI);
        setXRot(turnTowards(getXRot(), wantedPitch, xMax));
        setYRot(turnTowards(getYRot(), wantedYaw, yMax));
    }
    public static boolean checkMobSpawnRules(EntityType<? extends Mob> type, LevelAccessor level, EntitySpawnReason spawnReason, BlockPos pos, RandomSource random) {
        return true;
    }
    public boolean checkSpawnRules(LevelAccessor level, EntitySpawnReason spawnReason) {
        return true;
    }
    public boolean checkSpawnObstruction(LevelReader level) {
        return true;
    }
    public int getMaxSpawnClusterSize() {
        return 4;
    }
    public boolean isMaxGroupSizeReached(int groupSize) {
        return false;
    }
    public int getMaxFallDistance() {
        if (getTarget() == null) {
            return getComfortableFallDistance(0.0F);
        }
        int distance = (int) (getHealth() - getMaxHealth() * 0.33F) + 3;
        return Math.max(distance, 0) + getComfortableFallDistance(0.0F);
    }
    public ItemStack getBodyArmorItem() {
        return getItemBySlot(EquipmentSlot.BODY);
    }
    public boolean isSaddled() {
        return !getItemBySlot(EquipmentSlot.SADDLE).isEmpty();
    }
    public boolean isWearingBodyArmor() {
        return !getItemBySlot(EquipmentSlot.BODY).isEmpty();
    }
    public Container createEquipmentSlotContainer(EquipmentSlot slot) {
        Mob owner = this;
        return new net.minecraft.world.SimpleContainer(1) {
            public ItemStack getItem(int index) {
                return owner.getItemBySlot(slot);
            }

            public void setItem(int index, ItemStack stack) {
                owner.setItemSlot(slot, stack);
            }
        };
    }
    protected void dropCustomDeathLoot(ServerLevel level, DamageSource source, boolean killedByPlayer) {

    }
    public DropChances getDropChances() {
        return new DropChances(java.util.Map.of());
    }
    public void dropPreservedEquipment(ServerLevel level) {

    }
    public Set<EquipmentSlot> dropPreservedEquipment(ServerLevel level, Predicate<ItemStack> shouldDrop) {
        return java.util.Set.of();
    }
    public void equip(EquipmentTable equipment) {
        equip(equipment.lootTable(), equipment.slotDropChances());
    }
    public void equip(ResourceKey<LootTable> lootTable, Map<EquipmentSlot, Float> dropChances) {
        if (lootTable == null || !(level() instanceof ServerLevel serverLevel)) {
            return;
        }
        net.minecraft.world.level.storage.loot.LootParams.Builder builder = new net.minecraft.world.level.storage.loot.LootParams.Builder(serverLevel)
            .withParameter(net.minecraft.world.level.storage.loot.parameters.LootContextParams.THIS_ENTITY, this)
            .withParameter(net.minecraft.world.level.storage.loot.parameters.LootContextParams.ORIGIN, position());
        net.minecraft.world.level.storage.loot.LootParams params = new net.minecraft.world.level.storage.loot.LootParams(serverLevel, builder.contextMap(), Map.of(), 0.0F);
        for (ItemStack stack : net.minecraft.world.level.storage.loot.LootTable.of(lootTable.identifier()).getRandomItems(params)) {
            EquipmentSlot slot = getEquipmentSlotForItem(stack);
            if (!hasItemInSlot(slot)) {
                setItemSlot(slot, stack);
            }
        }
    }
    protected void populateDefaultEquipmentSlots(RandomSource random, DifficultyInstance difficulty) {

    }
    public static Item getEquipmentForSlot(EquipmentSlot slot, int type) {
        String suffix = switch (slot) {
            case HEAD -> "helmet";
            case CHEST -> "chestplate";
            case LEGS -> "leggings";
            case FEET -> "boots";
            default -> null;
        };
        String[] material = {"leather", "golden", "chainmail", "iron", "diamond"};
        if (suffix == null || type < 0 || type >= material.length) {
            return null;
        }
        return net.minecraft.core.registries.BuiltInRegistries.ITEM.getOptional(net.minecraft.resources.Identifier.withDefaultNamespace(material[type] + "_" + suffix)).orElse(null);
    }
    protected void populateDefaultEquipmentEnchantments(ServerLevelAccessor level, RandomSource random, DifficultyInstance localDifficulty) {

    }
    protected void enchantSpawnedWeapon(ServerLevelAccessor level, RandomSource random, DifficultyInstance difficulty) {

    }
    protected void enchantSpawnedArmor(ServerLevelAccessor level, RandomSource random, EquipmentSlot slot, DifficultyInstance difficulty) {

    }
    public SpawnGroupData finalizeSpawn(ServerLevelAccessor level, DifficultyInstance difficulty, EntitySpawnReason spawnReason, SpawnGroupData groupData) {
        return groupData;
    }
    public void setPersistenceRequired() {
        persistenceRequired = true;
    }
    public void setDropChance(EquipmentSlot slot, float percent) {
        if (live() instanceof org.bukkit.entity.Mob api && api.getEquipment() != null) {
            api.getEquipment().setDropChance(bukkitSlot(slot), percent);
        }
    }
    public boolean canPickUpLoot() {
        return super.canPickUpLoot();
    }
    public void setCanPickUpLoot(boolean canPickUpLoot) {
        org.bukkit.entity.LivingEntity living = live() instanceof org.bukkit.entity.LivingEntity l ? l : null;
        if (living != null) {
            living.setCanPickupItems(canPickUpLoot);
        }
    }
    protected boolean canDispenserEquipIntoSlot(EquipmentSlot slot) {
        return slot != EquipmentSlot.BODY && slot != EquipmentSlot.SADDLE;
    }
    public boolean isPersistenceRequired() {
        return persistenceRequired;
    }
    public InteractionResult interact(Player player, InteractionHand hand, Vec3 location) {
        return isAlive() ? mobInteract(player, hand) : InteractionResult.PASS;
    }
    protected void onOffspringSpawnedFromEgg(Player spawner, Mob offspring) {

    }
    protected InteractionResult mobInteract(Player player, InteractionHand hand) {
        return InteractionResult.PASS;
    }
    protected void usePlayerItem(Player player, InteractionHand hand, ItemStack itemStack) {
        if (!player.hasInfiniteMaterials()) {
            itemStack.shrink(1);
        }
    }
    public boolean isWithinHome() {
        return isWithinHome(blockPosition());
    }
    public boolean isWithinHome(BlockPos pos) {
        return homeRadius == -1 || homePos.distSqr(pos) < (double) (homeRadius * homeRadius);
    }
    public boolean isWithinHome(Vec3 pos) {
        return homeRadius == -1 || homePos.distToCenterSqr(pos) < (double) (homeRadius * homeRadius);
    }
    public void setHomeTo(BlockPos newCenter, int radius) {
        homePos = newCenter;
        homeRadius = radius;
    }
    public BlockPos getHomePosition() {
        return homePos;
    }
    public int getHomeRadius() {
        return homeRadius;
    }
    public void clearHome() {
        homeRadius = -1;
    }
    public boolean hasHome() {
        return homeRadius != -1;
    }
    public <T extends Mob> T convertTo(EntityType<T> entityType, ConversionParams params, EntitySpawnReason spawnReason, ConversionParams.AfterConversion<T> afterConversion) {
        if (isRemoved() || level() == null) {
            return null;
        }
        @SuppressWarnings("unchecked") T converted = (T) entityType.create(level(), spawnReason);
        if (converted == null) {
            return null;
        }
        converted.snapTo(position(), getYRot(), getXRot());
        converted.setBaby(isBaby());
        if (hasCustomName()) {
            converted.setCustomName(getCustomName());
            converted.setCustomNameVisible(isCustomNameVisible());
        }
        if (afterConversion != null) {
            afterConversion.finalizeConversion(converted);
        }
        level().addFreshEntity(converted);
        discard();
        return converted;
    }
    public <T extends Mob> T convertTo(EntityType<T> entityType, ConversionParams params, ConversionParams.AfterConversion<T> afterConversion) {
        return convertTo(entityType, params, EntitySpawnReason.CONVERSION, afterConversion);
    }
    public Leashable. LeashData getLeashData() {
        return leashData;
    }
    public void setLeashData(Leashable. LeashData leashData) {
        this.leashData = leashData;
    }
    public void onLeashRemoved() {
        return;
    }
    public void leashTooFarBehaviour() {
        if (live() instanceof org.bukkit.entity.LivingEntity living && living.isLeashed()) {
            living.setLeashHolder(null);
        }
    }
    public boolean canBeLeashed() {
        return true;
    }
    public boolean startRiding(Entity entity, boolean force, boolean sendEventAndTriggers) {
        return entity.live() != null && live() != null && entity.live().addPassenger(live());
    }
    public boolean isEffectiveAi() {
        return !isNoAi();
    }
    public void setNoAi(boolean flag) {
        org.bukkit.entity.LivingEntity living = live() instanceof org.bukkit.entity.LivingEntity l ? l : null;
        if (living != null) {
            living.setAI(!flag);
        }
    }
    public void setLeftHanded(boolean flag) {
        org.bukkit.entity.Mob mob = mob();
        if (mob != null) {
            mob.setLeftHanded(flag);
        }
    }
    public void setAggressive(boolean flag) {
        org.bukkit.entity.Mob mob = mob();
        if (mob != null) {
            mob.setAggressive(flag);
        }
    }
    public boolean isNoAi() {
        org.bukkit.entity.LivingEntity living = live() instanceof org.bukkit.entity.LivingEntity l ? l : null;
        return living != null && !living.hasAI();
    }
    public boolean isLeftHanded() {
        org.bukkit.entity.Mob mob = mob();
        return mob != null && mob.isLeftHanded();
    }
    public boolean isAggressive() {
        org.bukkit.entity.Mob mob = mob();
        return mob != null && mob.isAggressive();
    }
    public void setBaby(boolean baby) {
        if (live() instanceof org.bukkit.entity.Ageable api) {
            if (baby) {
                api.setBaby();
            } else {
                api.setAdult();
            }
        }
    }
    public HumanoidArm getMainArm() {
        return isLeftHanded() ? HumanoidArm.LEFT : HumanoidArm.RIGHT;
    }
    public boolean isWithinMeleeAttackRange(LivingEntity target) {
        return getAttackBoundingBox(Math.sqrt(2.04F) - 0.6F).intersects(target.getBoundingBox());
    }
    protected AABB getAttackBoundingBox(double horizontalExpansion) {
        AABB box = getBoundingBox();
        return box.inflate(horizontalExpansion, 0.0, horizontalExpansion);
    }
    public boolean doHurtTarget(ServerLevel level, Entity target) {
        if (!(live() instanceof org.bukkit.entity.LivingEntity attacker)
            || !(target.live() instanceof org.bukkit.entity.Damageable victim)) {
            return false;
        }
        org.bukkit.attribute.AttributeInstance damage = attacker.getAttribute(org.bukkit.attribute.Attribute.ATTACK_DAMAGE);
        victim.damage(damage == null ? 1.0 : damage.getValue(), attacker);
        return true;
    }
    protected void jumpInLiquid(TagKey<Fluid> type) {
        setDeltaMovement(getDeltaMovement().add(0.0, 0.04F, 0.0));
    }
    public void removeFreeWill() {
        goalSelector.removeAllGoals(goal -> true);
    }
    public GoalSelector getGoalSelector() {
        return goalSelector;
    }
    public void removeAllGoals(Predicate<Goal> predicate) {
        goalSelector.removeAllGoals(predicate);
    }
    protected void removeAfterChangingDimensions() {
        return;
    }
    public ItemStack getPickResult() {
        return ItemStack.EMPTY;
    }
    protected void onAttributeUpdated(Holder<Attribute> attribute) {

    }
    public void registerDebugValues(ServerLevel level, DebugValueSource.Registration registration) {

    }
    public float chargeSpeedModifier() {
        return 1.0F;
    }
    public void swingForAttack(InteractionHand hand) {
        if (live() instanceof org.bukkit.entity.LivingEntity living) {
            if (hand == InteractionHand.OFF_HAND) {
                living.swingOffHand();
            } else {
                living.swingMainHand();
            }
        }
    }
    public void swing(InteractionHand hand, SwingAnimation animation) {
        swing(hand, animation, true);
    }
    private static float turnTowards(float from, float to, float limit) {
        float diff = net.minecraft.util.Mth.wrapDegrees(to - from);
        return from + net.minecraft.util.Mth.clamp(diff, -limit, limit);
    }

    private Leashable.LeashData leashData;

}
