// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.player;

import net.minecraft.world.entity.ai.attributes.Attributes;
import net.minecraft.util.Mth;
import net.minecraft.world.entity.Avatar;
import net.minecraft.world.entity.ContainerUser;
import com.google.common.collect.ImmutableList;
import com.mojang.authlib.GameProfile;
import com.mojang.datafixers.util.Either;
import java.util.Collection;
import java.util.List;
import java.util.Optional;
import java.util.OptionalInt;
import net.minecraft.core.BlockPos;
import net.minecraft.core.Direction;
import net.minecraft.core.GlobalPos;
import net.minecraft.core.Holder;
import net.minecraft.nbt.CompoundTag;
import net.minecraft.network.chat.Component;
import net.minecraft.network.syncher.SynchedEntityData;
import net.minecraft.resources.Identifier;
import net.minecraft.server.dialog.Dialog;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.server.permissions.PermissionSet;
import net.minecraft.server.players.NameAndId;
import net.minecraft.sounds.SoundEvent;
import net.minecraft.sounds.SoundSource;
import net.minecraft.stats.Stat;
import net.minecraft.util.Unit;
import net.minecraft.world.Container;
import net.minecraft.world.InteractionHand;
import net.minecraft.world.InteractionResult;
import net.minecraft.world.MenuProvider;
import net.minecraft.world.attribute.BedRule;
import net.minecraft.world.damagesource.DamageSource;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.entity.EntityEquipment;
import net.minecraft.world.entity.EquipmentSlot;
import net.minecraft.world.entity.LivingEntity;
import net.minecraft.world.entity.MoveSimulationType;
import net.minecraft.world.entity.MoverType;
import net.minecraft.world.entity.Pose;
import net.minecraft.world.entity.SlotAccess;
import net.minecraft.world.entity.ai.attributes.AttributeSupplier;
import net.minecraft.world.entity.animal.equine.AbstractHorse;
import net.minecraft.world.entity.animal.nautilus.AbstractNautilus;
import net.minecraft.world.entity.animal.parrot.Parrot;
import net.minecraft.world.entity.projectile.FishingHook;
import net.minecraft.world.entity.vehicle.minecart.MinecartCommandBlock;
import net.minecraft.world.food.FoodData;
import net.minecraft.world.inventory.AbstractContainerMenu;
import net.minecraft.world.inventory.ClickAction;
import net.minecraft.world.inventory.InventoryMenu;
import net.minecraft.world.inventory.PlayerEnderChestContainer;
import net.minecraft.world.item.ItemCooldowns;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.item.component.ResolvableProfile;
import net.minecraft.world.item.crafting.RecipeHolder;
import net.minecraft.world.item.trading.MerchantOffers;
import net.minecraft.world.level.GameType;
import net.minecraft.world.level.Level;
import net.minecraft.world.level.block.AbstractBedBlock;
import net.minecraft.world.level.block.entity.CommandBlockEntity;
import net.minecraft.world.level.block.entity.ContainerOpenersCounter;
import net.minecraft.world.level.block.entity.JigsawBlockEntity;
import net.minecraft.world.level.block.entity.SignBlockEntity;
import net.minecraft.world.level.block.entity.SignTextSlot;
import net.minecraft.world.level.block.entity.StructureBlockEntity;
import net.minecraft.world.level.block.entity.TestBlockEntity;
import net.minecraft.world.level.block.entity.TestInstanceBlockEntity;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.storage.ValueInput;
import net.minecraft.world.level.storage.ValueOutput;
import net.minecraft.world.phys.AABB;
import net.minecraft.world.phys.Vec3;
public abstract class Player extends Avatar implements ContainerUser {
    private org.bukkit.entity.Player bukkit() {
        return live() instanceof org.bukkit.entity.Player player ? player : null;
    }

    protected Player() {
    }
    public static final int MAX_HEALTH = 20;
    public static final int SLEEP_DURATION = 100;
    public static final int WAKE_UP_DURATION = 10;
    public static final int ENDER_SLOT_OFFSET = 200;
    public static final int HELD_ITEM_SLOT = 499;
    public static final int CRAFTING_SLOT_OFFSET = 500;
    public static final float DEFAULT_BLOCK_INTERACTION_RANGE = 4.5F;
    public static final float DEFAULT_ENTITY_INTERACTION_RANGE = 3.0F;
    public static final float CREATIVE_ENTITY_INTERACTION_RANGE_MODIFIER_VALUE = 2.0F;
    protected PlayerEnderChestContainer enderChestInventory;
    public final InventoryMenu inventoryMenu = null;
    public AbstractContainerMenu containerMenu;
    protected FoodData foodData;
    protected int jumpTriggerTime;
    public int takeXpDelay;
    protected boolean wasUnderwater;
    public int experienceLevel;
    public int totalExperience;
    public float experienceProgress;
    protected int enchantmentSeed;
    protected final float defaultFlySpeed = 0;
    protected final List<Identifier> postEffects = null;
    public FishingHook fishing;
    protected float hurtDir;
    public Player(Level level, GameProfile gameProfile) {
        super(null, level);
    }
    protected EntityEquipment createEquipment() {
        return null;
    }
    public boolean blockActionRestricted(Level level, BlockPos pos, GameType gameType) {
        return gameType == net.minecraft.world.level.GameType.SPECTATOR || gameType == net.minecraft.world.level.GameType.ADVENTURE && !getAbilities().mayBuild;
    }
    public static AttributeSupplier.Builder createAttributes() {
        return LivingEntity.createLivingAttributes().add(Attributes.ATTACK_DAMAGE, 1.0).add(Attributes.MOVEMENT_SPEED, 0.1F)
            .add(Attributes.ATTACK_SPEED).add(Attributes.LUCK).add(Attributes.BLOCK_INTERACTION_RANGE, 4.5).add(Attributes.ENTITY_INTERACTION_RANGE, 3.0)
            .add(Attributes.BLOCK_BREAK_SPEED).add(Attributes.SUBMERGED_MINING_SPEED).add(Attributes.SNEAKING_SPEED).add(Attributes.MINING_EFFICIENCY)
            .add(Attributes.SWEEPING_DAMAGE_RATIO).add(Attributes.WAYPOINT_TRANSMIT_RANGE, 0.0).add(Attributes.WAYPOINT_RECEIVE_RANGE, 0.0);
    }
    protected void defineSynchedData(SynchedEntityData.Builder entityData) {
        return;
    }
    public void tick() {
        super.tick();
    }
    protected float getMaxHeadRotationRelativeToBody() {
        return isBlocking() ? 15.0F : 50.0F;
    }
    public boolean isSecondaryUseActive() {
        return isShiftKeyDown();
    }
    protected boolean wantsToStopRiding() {
        return isShiftKeyDown();
    }
    protected boolean isStayingOnGroundSurface() {
        return isShiftKeyDown();
    }
    protected boolean updateIsUnderwater() {
        wasUnderwater = isEyeInFluid(net.minecraft.tags.FluidTags.WATER);
        return wasUnderwater;
    }
    public void onAboveBubbleColumn(boolean dragDown, BlockPos pos) {
        handleOnAboveBubbleColumn(this, dragDown, pos);
    }
    public void onInsideBubbleColumn(boolean dragDown) {
        handleOnInsideBubbleColumn(this, dragDown);
    }
    protected ItemCooldowns createItemCooldowns() {
        return new net.minecraft.world.item.ItemCooldowns(bukkit());
    }
    protected void updatePlayerPose() {
        return;
    }
    protected boolean canPlayerFitWithinBlocksAndEntitiesWhen(Pose newPose) {
        return wouldNotSuffocateAtTargetPose(newPose);
    }
    protected SoundEvent getSwimSound() {
        return net.minecraft.sounds.SoundEvents.byId("entity.player.swim");
    }
    protected SoundEvent getSwimSplashSound() {
        return net.minecraft.sounds.SoundEvents.byId("entity.player.splash");
    }
    protected SoundEvent getSwimHighSpeedSplashSound() {
        return net.minecraft.sounds.SoundEvents.byId("entity.player.splash.high_speed");
    }
    public int getDimensionChangingDelay() {
        return 10;
    }
    public void playSound(SoundEvent sound, float volume, float pitch) {
        super.playSound(sound, volume, pitch);
    }
    public SoundSource getSoundSource() {
        return net.minecraft.sounds.SoundSource.PLAYERS;
    }
    protected int getFireImmuneTicks() {
        return 20;
    }
    public void handleEntityEvent(byte id) {
        super.handleEntityEvent(id);
    }
    protected void closeContainer() {
        doCloseContainer();
    }
    protected void doCloseContainer() {
        if (bukkit() != null) {
            bukkit().closeInventory();
        }
    }
    public void rideTick() {
        super.rideTick();
    }
    public void aiStep() {
        return;
    }
    protected void tickRegeneration() {
        return;
    }
    public void handleShoulderEntities() {
        return;
    }
    protected void removeEntitiesOnShoulder() {
        return;
    }
    public int getScore() {
        return bukkit() == null ? 0 : bukkit().getDeathScreenScore();
    }
    public void setScore(int value) {
        if (bukkit() != null) {
            bukkit().setDeathScreenScore(value);
        }
    }
    public void increaseScore(int amount) {
        setScore(getScore() + amount);
    }
    public void startAutoSpinAttack(int activationTicks, float dmg, ItemStack itemStackUsed) {
        if (bukkit() != null) {
            bukkit().startRiptideAttack(activationTicks, dmg, org.bukkit.craftbukkit.inventory.CraftItemStack.asCraftMirror(itemStackUsed));
        }
    }
    public ItemStack getWeaponItem() {
        return getMainHandItem();
    }
    public void die(DamageSource source) {
        super.die(source);
    }
    protected void dropEquipment(ServerLevel level) {
        return;
    }
    protected void destroyVanishingCursedItems() {
        return;
    }
    protected SoundEvent getHurtSound(DamageSource source) {
        return net.minecraft.sounds.SoundEvents.byId("entity.player.hurt");
    }
    protected SoundEvent getDeathSound() {
        return net.minecraft.sounds.SoundEvents.byId("entity.player.death");
    }
    public void handleCreativeModeItemDrop(ItemStack stack) {
        if (!stack.isEmpty()) {
            drop(stack, true, null);
        }
    }
    public float getDestroySpeed(BlockState state) {
        return getMainHandItem().getDestroySpeed(state) * (float) getAttributeValue(Attributes.BLOCK_BREAK_SPEED);
    }
    public boolean hasCorrectToolForDrops(BlockState state) {
        return !state.requiresCorrectToolForDrops() || getMainHandItem().isCorrectToolForDrops(state);
    }
    protected void readAdditionalSaveData(ValueInput input) {
        experienceLevel = input.getIntOr("XpLevel", 0);
        experienceProgress = input.getFloatOr("XpP", 0.0F);
        totalExperience = input.getIntOr("XpTotal", 0);
        enchantmentSeed = input.getIntOr("XpSeed", 0);
        getFoodData().readAdditionalSaveData(input);
    }
    protected void addAdditionalSaveData(ValueOutput output) {
        output.putInt("XpLevel", bukkit() == null ? experienceLevel : bukkit().getLevel());
        output.putFloat("XpP", bukkit() == null ? experienceProgress : bukkit().getExp());
        output.putInt("XpTotal", bukkit() == null ? totalExperience : bukkit().getTotalExperience());
        output.putInt("XpSeed", enchantmentSeed);
        getFoodData().addAdditionalSaveData(output);
    }
    public boolean isInvulnerableTo(ServerLevel level, DamageSource source) {
        return super.isInvulnerableTo(level, source) || bukkit() != null && bukkit().isInvulnerable();
    }
    public boolean hurtServer(ServerLevel level, DamageSource source, float damage) {
        return super.hurtServer(level, source, damage);
    }
    protected void blockUsingItem(ServerLevel level, LivingEntity attacker, DamageSource source, float damage, boolean fullyBlocked) {
        return;
    }
    public boolean canBeSeenAsEnemy() {
        return !getAbilities().invulnerable && super.canBeSeenAsEnemy();
    }
    public boolean canHarmPlayer(Player target) {
        return true;
    }
    protected void hurtArmor(DamageSource damageSource, float damage) {
        return;
    }
    protected void hurtHelmet(DamageSource damageSource, float damage) {
        return;
    }
    protected void actuallyHurt(ServerLevel level, DamageSource source, float dmg) {
        super.actuallyHurt(level, source, dmg);
    }
    public boolean isTextFilteringEnabled() {
        return false;
    }
    public void openTextEdit(SignBlockEntity sign, SignTextSlot slot) {
        return;
    }
    public void openMinecartCommandBlock(MinecartCommandBlock commandBlock) {
        return;
    }
    public void openCommandBlock(CommandBlockEntity commandBlock) {
        return;
    }
    public void openStructureBlock(StructureBlockEntity structureBlock) {
        return;
    }
    public void openTestBlock(TestBlockEntity testBlock) {
        return;
    }
    public void openTestInstanceBlock(TestInstanceBlockEntity testInstanceBlock) {
        return;
    }
    public void openJigsawBlock(JigsawBlockEntity jigsawBlock) {
        return;
    }
    public void openHorseInventory(AbstractHorse horse, Container container) {
        return;
    }
    public void openNautilusInventory(AbstractNautilus nautilus, Container container) {
        return;
    }
    public OptionalInt openMenu(MenuProvider provider) {
        return java.util.OptionalInt.empty();
    }
    public void openDialog(Holder<Dialog> dialog) {
        return;
    }
    public void sendMerchantOffers(int containerId, MerchantOffers offers, int merchantLevel, int merchantXp, boolean showProgressBar, boolean canRestock) {
        return;
    }
    public void openItemGui(ItemStack itemStack, InteractionHand hand) {
        return;
    }
    public InteractionResult interactOn(Entity entity, InteractionHand hand, Vec3 location) {
        return entity.interact(this, hand, location);
    }
    public void removeVehicle() {
        stopRiding();
    }
    protected boolean isImmobile() {
        return super.isImmobile() || isSleeping();
    }
    public boolean isAffectedByFluids() {
        return !getAbilities().flying;
    }
    protected Vec3 maybeBackOffFromEdge(Vec3 delta, MoverType moverType) {
        return delta;
    }
    public void attack(Entity entity) {
        if (bukkit() != null && entity.live() != null) {
            bukkit().attack(entity.live());
        }
    }
    public void causeExtraKnockback(Entity entity, float knockbackAmount, Vec3 oldMovement, DamageSource damageSource, float damage, boolean comesFromEffect) {
        super.causeExtraKnockback(entity, knockbackAmount, oldMovement, damageSource, damage, comesFromEffect);
    }
    public float getVoicePitch() {
        return 1.0F;
    }
    protected float getEnchantedDamage(Entity entity, float dmg, DamageSource damageSource) {
        return dmg;
    }
    protected void doAutoAttackOnTouch(LivingEntity entity) {
        return;
    }
    public void crit(Entity entity) {
        return;
    }
    public boolean stabAttack(EquipmentSlot slot, Entity target, float baseDamage, boolean dealsDamage, boolean dealsKnockback, boolean dismounts) {
        return super.stabAttack(slot, target, baseDamage, dealsDamage, dealsKnockback, dismounts);
    }
    public void magicCrit(Entity entity) {
        return;
    }
    public void remove(Entity.RemovalReason reason) {
        super.remove(reason);
    }
    public boolean isClientAuthoritative() {
        return false;
    }
    protected boolean isLocalClientAuthoritative() {
        return false;
    }
    public boolean isLocalPlayer() {
        return false;
    }
    public MoveSimulationType getMoveSimulationType() {
        return MoveSimulationType.AUTHORITATIVE_SIDE;
    }
    public boolean isEffectiveAi() {
        return true;
    }
    public GameProfile getGameProfile() {
        return new GameProfile(getUUID(), getScoreboardName());
    }
    public NameAndId nameAndId() {
        return new net.minecraft.server.players.NameAndId(getUUID(), getScoreboardName());
    }
    private Inventory inventoryView;
    public Inventory getInventory() {
        if (inventoryView == null) {
            inventoryView = new Inventory(this, (this instanceof net.minecraft.server.level.ServerPlayer sp ? sp.bukkitPlayer() : (org.bukkit.entity.HumanEntity) live()).getInventory());
        }
        return inventoryView;
    }
    public Abilities getAbilities() {
        if (abilitiesView == null) {
            abilitiesView = new Abilities();
        }
        org.bukkit.entity.Player player = bukkit();
        if (player != null) {
            abilitiesView.invulnerable = player.isInvulnerable();
            abilitiesView.flying = player.isFlying();
            abilitiesView.mayfly = player.getAllowFlight();
            abilitiesView.instabuild = player.getGameMode() == org.bukkit.GameMode.CREATIVE;
            abilitiesView.mayBuild = player.getGameMode() != org.bukkit.GameMode.ADVENTURE && player.getGameMode() != org.bukkit.GameMode.SPECTATOR;
            abilitiesView.setFlyingSpeed(player.getFlySpeed() / 2.0F);
            abilitiesView.setWalkingSpeed(player.getWalkSpeed() / 2.0F);
        }
        return abilitiesView;
    }
    public boolean hasInfiniteMaterials() {
        return isCreative();
    }
    public boolean preventsBlockDrops() {
        return isCreative();
    }
    public void updateTutorialInventoryAction(ItemStack itemCarried, ItemStack itemInSlot, ClickAction clickAction) {
        return;
    }
    public boolean hasContainerOpen() {
        org.bukkit.entity.Player player = bukkit();
        return player != null && player.getOpenInventory().getTopInventory().getType() != org.bukkit.event.inventory.InventoryType.CRAFTING;
    }
    public boolean canDropItems() {
        return true;
    }
    public Either<BedSleepingProblem, Unit> startSleepInBed(AbstractBedBlock bedBlock, BlockState bedBlockState, BedRule rule, BlockPos pos) {
        org.bukkit.entity.Player player = bukkit();
        org.bukkit.Location bed = new org.bukkit.Location(player.getWorld(), pos.getX(), pos.getY(), pos.getZ());
        return player.sleep(bed, false) ? Either.right(Unit.INSTANCE) : Either.left(new BedSleepingProblem(Component.translatable("block.minecraft.bed.no_sleep")));
    }
    public void stopSleepInBed(boolean forcefulWakeUp, boolean updateLevelList) {
        if (bukkit() != null) {
            bukkit().wakeup(updateLevelList);
        }
    }
    public void stopSleeping() {
        stopSleepInBed(true, true);
    }
    public boolean isSleepingLongEnough() {
        return isSleeping() && getSleepTimer() >= 100;
    }
    public int getSleepTimer() {
        org.bukkit.entity.Player player = bukkit();
        return player == null ? 0 : player.getSleepTicks();
    }
    public void sendSystemMessage(Component message) {
        if (bukkit() != null) {
            bukkit().sendMessage(org.basaltmc.bukkit.facade.ChatJson.toAdventure(message));
        }
    }
    public void sendOverlayMessage(Component message) {
        if (bukkit() != null) {
            bukkit().sendActionBar(org.basaltmc.bukkit.facade.ChatJson.toAdventure(message));
        }
    }
    public void awardStat(Identifier location) {
        awardStat(location, 1);
    }
    public void awardStat(Identifier location, int count) {
        awardStat(net.minecraft.stats.Stats.CUSTOM.get(location), count);
    }
    public void awardStat(Stat<?> stat) {
        awardStat(stat, 1);
    }
    public void awardStat(Stat<?> stat, int count) {
        return;
    }
    public void resetStat(Stat<?> stat) {
        return;
    }
    public int awardRecipes(Collection<RecipeHolder<?>> recipes) {
        return 0;
    }
    public void triggerRecipeCrafted(RecipeHolder<?> recipe, List<ItemStack> itemStacks) {
        return;
    }
    public void travel(Vec3 input) {
        super.travel(input);
    }
    protected boolean canGlide() {
        return super.canGlide();
    }
    public void updateSwimming() {
        return;
    }
    protected boolean freeAt(BlockPos pos) {
        return !level().getBlockState(pos).isSolid();
    }
    public float getSpeed() {
        return (float) getAttributeValue(Attributes.MOVEMENT_SPEED);
    }
    public boolean causeFallDamage(double fallDistance, float damageModifier, DamageSource damageSource) {
        return super.causeFallDamage(fallDistance, damageModifier, damageSource);
    }
    public boolean tryToStartFallFlying() {
        return false;
    }
    public void startFallFlying() {
        if (bukkit() != null) {
            bukkit().setGliding(true);
        }
    }
    protected void doWaterSplashEffect() {
        return;
    }
    protected void playStepSound(BlockPos onPos, BlockState onState) {
        return;
    }
    public LivingEntity.Fallsounds getFallSounds() {
        return new LivingEntity.Fallsounds(net.minecraft.sounds.SoundEvents.byId("entity.player.small_fall"), net.minecraft.sounds.SoundEvents.byId("entity.player.big_fall"));
    }
    public boolean killedEntity(ServerLevel level, LivingEntity entity, DamageSource source) {
        return true;
    }
    public void makeStuckInBlock(BlockState blockState, Vec3 speedMultiplier) {
        super.makeStuckInBlock(blockState, speedMultiplier);
    }
    public void giveExperiencePoints(int i) {
        org.bukkit.entity.Player player = bukkit();
        if (player != null) {
            player.giveExp(i);
        }
    }
    public int getEnchantmentSeed() {
        return enchantmentSeed;
    }
    public void onEnchantmentPerformed(ItemStack itemStack, int enchantmentCost) {
        enchantmentSeed = random.nextInt();
    }
    public void giveExperienceLevels(int amount) {
        org.bukkit.entity.Player player = bukkit();
        if (player != null) {
            player.giveExpLevels(amount);
        }
    }
    public int getXpNeededForNextLevel() {
        org.bukkit.entity.Player player = bukkit();
        int level = player == null ? 0 : player.getLevel();
        return level >= 30 ? 112 + (level - 30) * 9 : level >= 15 ? 37 + (level - 15) * 5 : 7 + level * 2;
    }
    public void causeFoodExhaustion(float amount) {
        org.bukkit.entity.Player player = bukkit();
        if (player != null && player.getGameMode() != org.bukkit.GameMode.CREATIVE && player.getGameMode() != org.bukkit.GameMode.SPECTATOR) {
            player.setExhaustion(player.getExhaustion() + amount);
        }
    }
    protected boolean hasEnoughFoodToDoExhaustiveManoeuvres() {
        return getFoodData().getFoodLevel() > 6;
    }
    public FoodData getFoodData() {
        if (foodData == null) {
            foodData = new net.minecraft.world.food.FoodData(bukkit());
        }
        return foodData;
    }
    public boolean canEat(boolean canAlwaysEat) {
        org.bukkit.entity.Player player = bukkit();
        return player != null && (canAlwaysEat || player.getFoodLevel() < 20);
    }
    public boolean isHurt() {
        return getHealth() > 0.0F && getHealth() < getMaxHealth();
    }
    public boolean mayBuild() {
        return !isSpectator();
    }
    public boolean mayUseItemAt(BlockPos pos, Direction direction, ItemStack itemStack) {
        return !isSpectator() && getAbilities().mayBuild;
    }
    protected int getBaseExperienceReward(ServerLevel level) {
        return Math.min(experienceLevel * 7, 100);
    }
    protected boolean isAlwaysExperienceDropper() {
        return true;
    }
    public boolean shouldShowName() {
        return true;
    }
    protected Entity.MovementEmission getMovementEmission() {
        return Entity.MovementEmission.ALL;
    }
    public void onUpdateAbilities() {
        org.bukkit.entity.Player player = bukkit();
        if (player != null && abilitiesView != null) {
            player.setAllowFlight(abilitiesView.mayfly);
            player.setFlying(abilitiesView.flying && abilitiesView.mayfly);
            player.setFlySpeed(abilitiesView.getFlyingSpeed() * 2.0F);
            player.setWalkSpeed(abilitiesView.getWalkingSpeed() * 2.0F);
        }
    }
    public Component getName() {
        return Component.literal(getScoreboardName());
    }
    public String getPlainTextName() {
        return getScoreboardName();
    }
    public PlayerEnderChestContainer getEnderChestInventory() {
        if (enderChestInventory == null) {
            enderChestInventory = new net.minecraft.world.inventory.PlayerEnderChestContainer(bukkit() == null ? null : bukkit().getEnderChest());
        }
        return enderChestInventory;
    }
    protected boolean doesEmitEquipEvent(EquipmentSlot slot) {
        return true;
    }
    public boolean addItem(ItemStack itemStack) {
        if (bukkit() == null) {
            return false;
        }
        return bukkit().getInventory().addItem(org.bukkit.craftbukkit.inventory.CraftItemStack.asCraftMirror(itemStack)).isEmpty();
    }
    public GameType gameMode() {
        org.bukkit.entity.Player player = bukkit();
        return player == null ? GameType.SURVIVAL : GameType.valueOf(player.getGameMode().name());
    }
    public boolean isSpectator() {
        org.bukkit.entity.Player player = bukkit();
        return player != null && player.getGameMode() == org.bukkit.GameMode.SPECTATOR;
    }
    public boolean isPickable() {
        return !isSpectator();
    }
    public boolean isSwimming() {
        return !isCreative() ? getSharedFlag(4) : getSharedFlag(4);
    }
    public boolean isCreative() {
        org.bukkit.entity.Player player = bukkit();
        return player != null && player.getGameMode() == org.bukkit.GameMode.CREATIVE;
    }
    public boolean isPushedByFluid() {
        org.bukkit.entity.Player player = bukkit();
        return player == null || !player.isFlying();
    }
    public Component getDisplayName() {
        return Component.literal(bukkit() == null ? getScoreboardName() : bukkit().getName());
    }
    public String getScoreboardName() {
        org.bukkit.entity.Player player = bukkit();
        return player == null ? "" : player.getName();
    }
    protected void internalSetAbsorptionAmount(float absorptionAmount) {
        setAbsorptionAmount(absorptionAmount);
    }
    public float getAbsorptionAmount() {
        return super.getAbsorptionAmount();
    }
    public SlotAccess getSlot(int slot) {
        return SlotAccess.of(() -> getInventory().getItem(slot), stack -> getInventory().setItem(slot, stack));
    }
    public boolean isReducedDebugInfo() {
        return false;
    }
    public void setReducedDebugInfo(boolean reducedDebugInfo) {
        return;
    }
    public void setRemainingFireTicks(int remainingTicks) {
        super.setRemainingFireTicks(remainingTicks);
    }
    protected static Optional<Parrot.Variant> extractParrotVariant(CompoundTag tag) {
        return Optional.empty();
    }
    protected static OptionalInt convertParrotVariant(Optional<Parrot.Variant> variant) {
        return java.util.OptionalInt.empty();
    }
    public void setShoulderParrotLeft(Optional<Parrot.Variant> variant) {
        return;
    }
    public Optional<Parrot.Variant> getShoulderParrotLeft() {
        return Optional.empty();
    }
    public void setShoulderParrotRight(Optional<Parrot.Variant> variant) {
        return;
    }
    public Optional<Parrot.Variant> getShoulderParrotRight() {
        return Optional.empty();
    }
    public float getCurrentItemAttackStrengthDelay() {
        return (float) (1.0 / getAttributeValue(Attributes.ATTACK_SPEED) * 20.0);
    }
    public boolean cannotAttackWithItem(ItemStack itemStack, int tolerance) {
        return false;
    }
    public float getAttackStrengthScale(float a) {
        return bukkit() == null ? 1.0F : bukkit().getAttackCooldown();
    }
    public float getItemSwapScale(float a) {
        return 1.0F;
    }
    public void resetAttackStrengthTicker() {
        resetOnlyAttackStrengthTicker();
    }
    public void onAttack() {
        resetAttackStrengthTicker();
    }
    public void resetOnlyAttackStrengthTicker() {
        if (bukkit() != null) {
            bukkit().resetCooldown();
        }
    }
    public ItemCooldowns getCooldowns() {
        if (cooldownsView == null) {
            cooldownsView = createItemCooldowns();
        }
        return cooldownsView;
    }
    protected float getBlockSpeedFactor() {
        return super.getBlockSpeedFactor();
    }
    public float getLuck() {
        return (float) getAttributeValue(Attributes.LUCK);
    }
    public boolean canUseGameMasterBlocks() {
        org.bukkit.entity.Player player = bukkit();
        return player != null && player.isOp() && isCreative();
    }
    public PermissionSet permissions() {
        org.bukkit.entity.Player player = bukkit();
        if (player == null || !player.isOp()) {
            return net.minecraft.server.permissions.LevelBasedPermissionSet.ALL;
        }
        return net.minecraft.server.permissions.LevelBasedPermissionSet.OWNER;
    }
    public ImmutableList<Pose> getDismountPoses() {
        return ImmutableList.of(Pose.STANDING, Pose.CROUCHING, Pose.SWIMMING);
    }
    public ItemStack getProjectile(ItemStack heldWeapon) {
        return ItemStack.EMPTY;
    }
    public Vec3 getRopeHoldPosition(float partialTickTime) {
        return super.getRopeHoldPosition(partialTickTime);
    }
    public boolean isAlwaysTicking() {
        return true;
    }
    public boolean isScoping() {
        return false;
    }
    public boolean shouldBeSaved() {
        return true;
    }
    public Optional<GlobalPos> getLastDeathLocation() {
        return Optional.empty();
    }
    public void setLastDeathLocation(Optional<GlobalPos> pos) {
        return;
    }
    public float getHurtDir() {
        return hurtDir;
    }
    public void animateHurt(float yaw) {
        super.animateHurt(yaw);
    }
    public boolean isMobilityRestricted() {
        return false;
    }
    public boolean canSprint() {
        return true;
    }
    protected float getFlyingSpeed() {
        return bukkit() == null ? 0.05F : bukkit().getFlySpeed() / 2.0F;
    }
    public boolean hasContainerOpen(ContainerOpenersCounter container, BlockPos blockPos) {
        return false;
    }
    public double getContainerInteractionRange() {
        return blockInteractionRange();
    }
    public double blockInteractionRange() {
        return getAttributeValue(Attributes.BLOCK_INTERACTION_RANGE);
    }
    public double entityInteractionRange() {
        return getAttributeValue(Attributes.ENTITY_INTERACTION_RANGE);
    }
    public boolean isWithinEntityInteractionRange(Entity entity, double buffer) {
        return entity.isRemoved() ? false : isWithinEntityInteractionRange(entity.getBoundingBox(), buffer);
    }
    public boolean isWithinEntityInteractionRange(AABB aabb, double buffer) {
        return aabb.distanceToSqr(getEyePosition()) < Mth.square(entityInteractionRange() + buffer);
    }
    public boolean isWithinAttackRange(ItemStack weaponItem, AABB aabb, double buffer) {
        Vec3 eye = getEyePosition();
        AABB reach = aabb.inflate(buffer);
        double dx = Math.max(Math.max(reach.minX - eye.x, 0.0), eye.x - reach.maxX);
        double dy = Math.max(Math.max(reach.minY - eye.y, 0.0), eye.y - reach.maxY);
        double dz = Math.max(Math.max(reach.minZ - eye.z, 0.0), eye.z - reach.maxZ);
        double range = getAttributeValue(Attributes.ENTITY_INTERACTION_RANGE);
        return dx * dx + dy * dy + dz * dz <= range * range;
    }
    public boolean isWithinBlockInteractionRange(BlockPos pos, double buffer) {
        return new AABB(pos).distanceToSqr(getEyePosition()) < Mth.square(blockInteractionRange() + buffer);
    }
    public boolean shouldRotateWithMinecart() {
        return false;
    }
    public boolean onClimbable() {
        return super.onClimbable();
    }
    public String debugInfo() {
        return "";
    }
    public ResolvableProfile getProfile() {
        return ResolvableProfile.createResolved(getGameProfile());
    }
    public DamageSource createDamageSource() {
        return damageSources().playerAttack(this);
    }
    public static record BedSleepingProblem(Component message) {
        public static final BedSleepingProblem TOO_FAR_AWAY = new BedSleepingProblem(net.minecraft.network.chat.Component.translatable("block.minecraft.bed.too_far_away"));
        public static final BedSleepingProblem OBSTRUCTED = new BedSleepingProblem(net.minecraft.network.chat.Component.translatable("block.minecraft.bed.obstructed"));
        public static final BedSleepingProblem OTHER_PROBLEM = new BedSleepingProblem((Component) null);
        public static final BedSleepingProblem NOT_SAFE = new BedSleepingProblem(net.minecraft.network.chat.Component.translatable("block.minecraft.bed.not_safe"));
    }

    private Abilities abilitiesView;
    private net.minecraft.world.item.ItemCooldowns cooldownsView;

}
