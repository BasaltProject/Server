// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.server.level;

import com.mojang.authlib.GameProfile;
import net.minecraft.server.MinecraftServer;
import net.minecraft.server.network.ServerGamePacketListenerImpl;
import org.basaltmc.bukkit.Unsupported;
import com.mojang.datafixers.util.Either;
import com.mojang.serialization.Codec;
import com.mojang.serialization.MapCodec;
import java.util.Collection;
import java.util.List;
import java.util.Optional;
import java.util.OptionalInt;
import java.util.Set;
import net.minecraft.commands.CommandSource;
import net.minecraft.commands.CommandSourceStack;
import net.minecraft.commands.arguments.EntityAnchorArgument;
import net.minecraft.core.BlockPos;
import net.minecraft.core.Holder;
import net.minecraft.core.SectionPos;
import net.minecraft.nbt.CompoundTag;
import net.minecraft.network.chat.ChatType;
import net.minecraft.network.chat.Component;
import net.minecraft.network.chat.OutgoingChatMessage;
import net.minecraft.network.chat.RemoteChatSession;
import net.minecraft.network.protocol.game.CommonPlayerSpawnInfo;
import net.minecraft.network.protocol.status.ServerStatus;
import net.minecraft.resources.Identifier;
import net.minecraft.resources.ResourceKey;
import net.minecraft.server.PlayerAdvancements;
import net.minecraft.server.dialog.Dialog;
import net.minecraft.server.network.TextFilter;
import net.minecraft.server.permissions.PermissionSet;
import net.minecraft.stats.ServerRecipeBook;
import net.minecraft.stats.ServerStatsCounter;
import net.minecraft.stats.Stat;
import net.minecraft.util.Prediction;
import net.minecraft.util.Unit;
import net.minecraft.util.debug.DebugSubscription;
import net.minecraft.world.Container;
import net.minecraft.world.InteractionHand;
import net.minecraft.world.MenuProvider;
import net.minecraft.world.attribute.BedRule;
import net.minecraft.world.damagesource.DamageSource;
import net.minecraft.world.effect.MobEffectInstance;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.entity.EquipmentSlot;
import net.minecraft.world.entity.Relative;
import net.minecraft.world.entity.ai.attributes.Attribute;
import net.minecraft.world.entity.animal.equine.AbstractHorse;
import net.minecraft.world.entity.animal.nautilus.AbstractNautilus;
import net.minecraft.world.entity.item.ItemEntity;
import net.minecraft.world.entity.monster.warden.WardenSpawnTracker;
import net.minecraft.world.entity.player.ChatVisiblity;
import net.minecraft.world.entity.player.Input;
import net.minecraft.world.entity.player.Player;
import net.minecraft.world.entity.projectile.throwableitemprojectile.ThrownEnderpearl;
import net.minecraft.world.item.ItemCooldowns;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.item.component.SwingAnimation;
import net.minecraft.world.item.crafting.Recipe;
import net.minecraft.world.item.crafting.RecipeHolder;
import net.minecraft.world.item.trading.MerchantOffers;
import net.minecraft.world.level.ChunkPos;
import net.minecraft.world.level.GameType;
import net.minecraft.world.level.Level;
import net.minecraft.world.level.block.AbstractBedBlock;
import net.minecraft.world.level.block.entity.CommandBlockEntity;
import net.minecraft.world.level.block.entity.SignBlockEntity;
import net.minecraft.world.level.block.entity.SignTextSlot;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.portal.TeleportTransition;
import net.minecraft.world.level.storage.LevelData;
import net.minecraft.world.level.storage.ValueInput;
import net.minecraft.world.level.storage.ValueOutput;
import net.minecraft.world.phys.Vec2;
import net.minecraft.world.phys.Vec3;

public class ServerPlayer extends Player {
    private org.bukkit.entity.Player player;
    private ServerStatsCounter stats;
    private ServerRecipeBook recipeBook;
    private SectionPos lastSectionPos;
    private ChunkTrackingView chunkTrackingView;
    private net.minecraft.world.entity.monster.warden.WardenSpawnTracker wardenSpawnTracker;
    private net.minecraft.world.entity.player.Input lastClientInput = new net.minecraft.world.entity.player.Input(false, false, false, false, false, false, false);
    public void bindPlayer(org.bukkit.entity.Player player){this.player=player;bindBukkitEntity(player);}
    public org.bukkit.entity.Player bukkitPlayer(){return player;}
    @Override public int getId(){return player==null?super.getId():player.getEntityId();}
    @Override public java.util.UUID getUUID(){return player==null?super.getUUID():player.getUniqueId();}
    @Override public org.bukkit.craftbukkit.entity.CraftEntity getBukkitEntity(){return player==null?super.getBukkitEntity():(org.bukkit.craftbukkit.entity.CraftEntity)player;}

    public ServerGamePacketListenerImpl connection;
    private ServerLevel level;
    private final List<Identifier> postEffects = new java.util.ArrayList<>();
    private boolean postEffectsDirty;
    private ClientInformation clientInformation;

    public ServerPlayer(ServerGamePacketListenerImpl connection) {
        this.connection = connection;
        connection.player = this;
    }

    public ServerPlayer(MinecraftServer server, ServerLevel level, GameProfile profile, ClientInformation information) {
        this.level = level;
    }

    public ServerLevel level() {
        if (level == null) {
            throw Unsupported.hit("net.minecraft.server.level.ServerPlayer.level");
        }
        return level;
    }
    public static final double BLOCK_INTERACTION_DISTANCE_VERIFICATION_BUFFER = 1.0;
    public static final double ENTITY_INTERACTION_DISTANCE_VERIFICATION_BUFFER = 3.0;
    public static final int ENDER_PEARL_TICKET_RADIUS = 2;
    public static final String ENDER_PEARLS_TAG = "ender_pearls";
    public static final String ENDER_PEARL_DIMENSION_TAG = "ender_pearl_dimension";
    public static final String TAG_DIMENSION = "Dimension";
    public final ServerPlayerGameMode gameMode = null;
    public boolean seenCredits;
    public final Object object = null;
    public boolean wonGame;
    public BlockPos adjustSpawnLocation(ServerLevel level, BlockPos spawnSuggestion) {
        return spawnSuggestion;
    }
    protected void readAdditionalSaveData(ValueInput input) {
    }
    protected void addAdditionalSaveData(ValueOutput output) {
    }
    public void loadAndSpawnParentVehicle(ValueInput playerInput) {
    }
    public void loadAndSpawnEnderPearls(ValueInput playerInput) {
    }
    public void setExperiencePoints(int amount) {
        player.setExp(amount / (float) player.getExpToLevel());
    }
    public void setExperienceLevels(int amount) {
        player.setLevel(amount);
    }
    public void giveExperienceLevels(int amount) {
        player.giveExpLevels(amount);
    }
    public void onEnchantmentPerformed(ItemStack itemStack, int enchantmentCost) {
    }
    public void initInventoryMenu() {
    }
    public void onEnterCombat() {
    }
    public void onLeaveCombat() {
    }
    public void onInsideBlock(BlockState state) {
    }
    protected ItemCooldowns createItemCooldowns() {
        return new net.minecraft.world.item.ServerItemCooldowns(this);
    }
    public void tick() {
    }
    public void doTick() {
    }
    protected void tickRegeneration() {
    }
    public void handleShoulderEntities() {
    }
    public boolean setEntityOnShoulder(CompoundTag entityTag) {
        if (getShoulderEntityLeft().isEmpty()) {
            setShoulderEntityLeft(entityTag);
            return true;
        }
        if (getShoulderEntityRight().isEmpty()) {
            setShoulderEntityRight(entityTag);
            return true;
        }
        return false;
    }
    protected void removeEntitiesOnShoulder() {
        player.releaseLeftShoulderEntity();
        player.releaseRightShoulderEntity();
    }
    public void resetFallDistance() {
        player.setFallDistance(0.0F);
    }
    public void trackStartFallingPosition() {
    }
    public void trackEnteredOrExitedLavaOnVehicle() {
    }
    public void die(DamageSource source) {
        player.setHealth(0.0);
    }
    public void awardKillScore(Entity victim, DamageSource killingBlow) {
        if (victim == this) {
            return;
        }
        super.awardKillScore(victim, killingBlow);
        net.minecraft.world.scores.Scoreboard scoreboard = level().getScoreboard();
        scoreboard.forAllObjectives(net.minecraft.world.scores.criteria.ObjectiveCriteria.KILL_COUNT_ALL, this, net.minecraft.world.scores.ScoreAccess::increment);
        if (victim instanceof Player) {
            awardStat(net.minecraft.stats.Stats.PLAYER_KILLS);
            scoreboard.forAllObjectives(net.minecraft.world.scores.criteria.ObjectiveCriteria.KILL_COUNT_PLAYERS, this, net.minecraft.world.scores.ScoreAccess::increment);
        } else {
            awardStat(net.minecraft.stats.Stats.MOB_KILLS);
        }
    }
    public boolean hurtServer(ServerLevel level, DamageSource source, float damage) {
        player.damage(damage);
        return true;
    }
    public boolean canHarmPlayer(Player target) {
        return player.getWorld().getPVP();
    }
    public TeleportTransition findRespawnPositionAndUseSpawnBlock(boolean consumeSpawnBlock, TeleportTransition.PostTeleportTransition postTeleportTransition) {
        org.bukkit.Location spawn = player.getRespawnLocation();
        if (spawn == null || spawn.getWorld() == null) {
            return TeleportTransition.createDefault(this, postTeleportTransition);
        }
        ServerLevel target = ((org.bukkit.craftbukkit.CraftWorld) spawn.getWorld()).getHandle();
        return new TeleportTransition(target, new Vec3(spawn.getX(), spawn.getY(), spawn.getZ()), Vec3.ZERO, spawn.getYaw(), spawn.getPitch(), postTeleportTransition);
    }
    public boolean isReceivingWaypoints() {
        return true;
    }
    protected void onAttributeUpdated(Holder<Attribute> attribute) {
    }
    public void sendPostEffects() {
        postEffectsDirty = false;
        connection.send(new net.minecraft.network.protocol.common.ClientboundPostEffectsPacket(postEffects));
    }
    public boolean addPostEffect(Identifier postEffect) {
        if (postEffects.contains(postEffect)) {
            return false;
        }
        postEffects.add(postEffect);
        postEffectsDirty = true;
        return true;
    }
    public boolean clearPostEffects() {
        if (postEffects.isEmpty()) {
            return false;
        }
        postEffects.clear();
        postEffectsDirty = true;
        return true;
    }
    public List<Identifier> getPostEffects() {
        return postEffects;
    }
    public boolean removePostEffect(Identifier postEffect) {
        if (postEffects.remove(postEffect)) {
            postEffectsDirty = true;
            return true;
        }
        return false;
    }
    public void showEndCredits() {
        player.showWinScreen();
    }
    public ServerPlayer teleport(TeleportTransition transition) {
        org.bukkit.Location at = player.getLocation();
        net.minecraft.world.phys.Vec3 position = transition.position();
        java.util.Set<Relative> relatives = transition.relatives();
        org.bukkit.World world = transition.newLevel() == null ? at.getWorld() : transition.newLevel().getWorld();
        float yaw = relatives.contains(Relative.Y_ROT) ? at.getYaw() + transition.yRot() : transition.yRot();
        float pitch = relatives.contains(Relative.X_ROT) ? at.getPitch() + transition.xRot() : transition.xRot();
        player.teleport(new org.bukkit.Location(world, position.x, position.y, position.z, yaw, pitch));
        net.minecraft.world.phys.Vec3 motion = transition.deltaMovement();
        player.setVelocity(new org.bukkit.util.Vector(motion.x, motion.y, motion.z));
        return this;
    }
    public void forceSetRotation(float yRot, boolean relativeY, float xRot, boolean relativeX) {
        org.bukkit.Location at = player.getLocation();
        at.setYaw(relativeY ? at.getYaw() + yRot : yRot);
        at.setPitch(relativeX ? at.getPitch() + xRot : xRot);
        player.teleport(at);
    }
    public boolean broadcastToPlayer(ServerPlayer player) {
        return true;
    }
    public void take(Entity entity, int orgCount) {
        connection.send(new net.minecraft.network.protocol.game.ClientboundTakeItemEntityPacket(entity.getId(), getId(), orgCount));
    }
    public Either<Player.BedSleepingProblem, Unit> startSleepInBed(AbstractBedBlock bedBlock, BlockState bedBlockState, BedRule rule, BlockPos pos) {
        return super.startSleepInBed(bedBlock, bedBlockState, rule, pos);
    }
    public boolean startSleeping(BlockPos bedPosition) {
        player.sleep(new org.bukkit.Location(player.getWorld(), bedPosition.getX(), bedPosition.getY(), bedPosition.getZ()), true);
        return true;
    }
    public void stopSleepInBed(boolean forcefulWakeUp, boolean updateLevelList) {
        player.wakeup(forcefulWakeUp);
    }
    public boolean isInvulnerableTo(ServerLevel level, DamageSource source) {
        return player.isInvulnerable();
    }
    protected void onChangedBlock(ServerLevel level, BlockPos pos) {
    }
    protected void checkFallDamage(double ya, boolean onGround, BlockState onState, BlockPos pos) {
    }
    public void onExplosionHit(Entity explosionCausedBy) {
    }
    protected void pushEntities() {
    }


    @SuppressWarnings({"unchecked", "rawtypes"})
    private void changeStat(Stat<?> stat, int count, boolean reset) {
        String type = net.minecraft.core.registries.BuiltInRegistries.STAT_TYPE.getKey((net.minecraft.stats.StatType) stat.getType()).getPath();
        Object key = ((net.minecraft.core.Registry) stat.getType().getRegistry()).getKey(stat.getValue());
        String name = key == null ? String.valueOf(stat.getValue()) : ((net.minecraft.resources.Identifier) key).getPath();
        try {
            if (type.equals("custom")) {
                org.bukkit.Statistic statistic = org.bukkit.Statistic.valueOf(name.toUpperCase(java.util.Locale.ROOT));
                apply(statistic, null, null, count, reset);
                return;
            }
            org.bukkit.Statistic statistic = switch (type) {
                case "mined" -> org.bukkit.Statistic.MINE_BLOCK;
                case "broken" -> org.bukkit.Statistic.BREAK_ITEM;
                case "crafted" -> org.bukkit.Statistic.CRAFT_ITEM;
                case "used" -> org.bukkit.Statistic.USE_ITEM;
                case "picked_up" -> org.bukkit.Statistic.PICKUP;
                case "dropped" -> org.bukkit.Statistic.DROP;
                case "killed" -> org.bukkit.Statistic.KILL_ENTITY;
                case "killed_by" -> org.bukkit.Statistic.ENTITY_KILLED_BY;
                default -> null;
            };
            if (statistic == null) {
                return;
            }
            if (type.equals("killed") || type.equals("killed_by")) {
                apply(statistic, null, org.bukkit.entity.EntityType.fromName(name), count, reset);
            } else {
                apply(statistic, org.bukkit.Material.matchMaterial(name), null, count, reset);
            }
        } catch (IllegalArgumentException unknown) {
        }
    }

    private void apply(org.bukkit.Statistic statistic, org.bukkit.Material material, org.bukkit.entity.EntityType entity, int count, boolean reset) {
        if (reset) {
            if (material != null) {
                player.setStatistic(statistic, material, 0);
            } else if (entity != null) {
                player.setStatistic(statistic, entity, 0);
            } else {
                player.setStatistic(statistic, 0);
            }
        } else if (material != null) {
            player.incrementStatistic(statistic, material, count);
        } else if (entity != null) {
            player.incrementStatistic(statistic, entity, count);
        } else {
            player.incrementStatistic(statistic, count);
        }
    }

    private PlayerAdvancements advancements;
    private int menuCounter;

    private OptionalInt showContainer(net.minecraft.world.Container container, Component title) {
        int size = Math.max(9, Math.min(54, ((container.getContainerSize() + 8) / 9) * 9));
        org.bukkit.inventory.Inventory view = title == null
            ? org.bukkit.Bukkit.createInventory(null, size)
            : org.bukkit.Bukkit.createInventory(null, size, org.basaltmc.bukkit.facade.ChatJson.toAdventure(title));
        for (int slot = 0; slot < Math.min(size, container.getContainerSize()); slot++) {
            view.setItem(slot, org.bukkit.craftbukkit.inventory.CraftItemStack.asBukkitMirror(container.getItem(slot)));
        }
        player.openInventory(view);
        menuCounter = menuCounter % 100 + 1;
        return OptionalInt.of(menuCounter);
    }
    public void openTextEdit(SignBlockEntity sign, SignTextSlot slot) {
        org.bukkit.block.BlockState state = player.getWorld().getBlockAt(sign.getBlockPos().getX(), sign.getBlockPos().getY(), sign.getBlockPos().getZ()).getState();
        if (state instanceof org.bukkit.block.Sign editable) {
            player.openSign(editable, String.valueOf(slot).equals("BACK") ? org.bukkit.block.sign.Side.BACK : org.bukkit.block.sign.Side.FRONT);
        }
    }
    public void openDialog(Holder<Dialog> dialog) {
        connection.send(new net.minecraft.network.protocol.common.ClientboundShowDialogPacket(dialog));
    }
    public OptionalInt openMenu(MenuProvider provider) {
        if (provider instanceof net.minecraft.world.level.block.entity.BlockEntity entity) {
            org.bukkit.block.BlockState state = player.getWorld().getBlockAt(entity.getBlockPos().getX(), entity.getBlockPos().getY(), entity.getBlockPos().getZ()).getState();
            if (state instanceof org.bukkit.block.Container container) {
                player.openInventory(container.getInventory());
                menuCounter = menuCounter % 100 + 1;
                return OptionalInt.of(menuCounter);
            }
        }
        net.minecraft.world.inventory.AbstractContainerMenu menu = provider.createMenu(menuCounter % 100 + 1, getInventory(), this);
        if (menu == null) {
            return OptionalInt.empty();
        }
        net.minecraft.world.SimpleContainer shown = new net.minecraft.world.SimpleContainer(Math.max(0, menu.slots.size() - 36));
        for (int slot = 0; slot < shown.getContainerSize(); slot++) {
            shown.setItem(slot, menu.getSlot(slot).getItem());
        }
        return showContainer(shown, provider.getDisplayName());
    }
    public void sendMerchantOffers(int containerId, MerchantOffers offers, int merchantLevel, int merchantXp, boolean showProgressBar, boolean canRestock) {
        java.util.List<org.bukkit.inventory.MerchantRecipe> recipes = new java.util.ArrayList<>();
        for (net.minecraft.world.item.trading.MerchantOffer offer : offers) {
            org.bukkit.inventory.MerchantRecipe recipe = new org.bukkit.inventory.MerchantRecipe(
                org.bukkit.craftbukkit.inventory.CraftItemStack.asBukkitMirror(offer.getResult()), offer.getUses(), offer.getMaxUses(),
                offer.shouldRewardExp(), offer.getXp(), offer.getPriceMultiplier());
            recipe.addIngredient(org.bukkit.craftbukkit.inventory.CraftItemStack.asBukkitMirror(offer.getBaseCostA()));
            if (!offer.getCostB().isEmpty()) {
                recipe.addIngredient(org.bukkit.craftbukkit.inventory.CraftItemStack.asBukkitMirror(offer.getCostB()));
            }
            recipes.add(recipe);
        }
        org.bukkit.inventory.Merchant merchant = org.bukkit.Bukkit.createMerchant();
        merchant.setRecipes(recipes);
        player.openMerchant(merchant, true);
    }
    public void openHorseInventory(AbstractHorse horse, Container container) {
        showContainer(container, null);
    }
    public void openNautilusInventory(AbstractNautilus nautilus, Container container) {
        showContainer(container, null);
    }
    public void openItemGui(ItemStack itemStack, InteractionHand hand) {
        player.openBook(org.bukkit.craftbukkit.inventory.CraftItemStack.asBukkitMirror(itemStack));
    }
    public void openCommandBlock(CommandBlockEntity commandBlock) {
        connection.send(net.minecraft.network.protocol.game.ClientboundBlockEntityDataPacket.create(commandBlock));
    }
    public void closeContainer() {
        player.closeInventory();
    }
    public void doCloseContainer() {
        player.closeInventory();
    }
    public void rideTick() {
    }
    public void checkMovementStatistics(double dx, double dy, double dz) {
    }
    public void awardStat(Stat<?> stat, int count) {
        changeStat(stat, count, false);
    }
    public void resetStat(Stat<?> stat) {
        changeStat(stat, 0, true);
    }
    public int awardRecipes(Collection<RecipeHolder<?>> recipes) {
        java.util.List<org.bukkit.NamespacedKey> keys = new java.util.ArrayList<>();
        for (net.minecraft.world.item.crafting.RecipeHolder<?> each : recipes) {
            keys.add(org.bukkit.NamespacedKey.fromString(each.id().identifier().toString()));
        }
        return player.discoverRecipes(keys);
    }
    public void triggerRecipeCrafted(RecipeHolder<?> recipe, List<ItemStack> itemStacks) {
    }
    public void awardRecipesByKey(List<ResourceKey<Recipe<?>>> recipeIds) {
        java.util.List<org.bukkit.NamespacedKey> keys = new java.util.ArrayList<>();
        for (ResourceKey<Recipe<?>> each : recipeIds) {
            keys.add(org.bukkit.NamespacedKey.fromString(each.identifier().toString()));
        }
        player.discoverRecipes(keys);
    }
    public int resetRecipes(Collection<RecipeHolder<?>> recipes) {
        java.util.List<org.bukkit.NamespacedKey> keys = new java.util.ArrayList<>();
        for (net.minecraft.world.item.crafting.RecipeHolder<?> each : recipes) {
            keys.add(org.bukkit.NamespacedKey.fromString(each.id().identifier().toString()));
        }
        return player.undiscoverRecipes(keys);
    }
    public void jumpFromGround() {
        org.bukkit.util.Vector velocity = player.getVelocity();
        player.setVelocity(velocity.setY(Math.max(velocity.getY(), 0.42)));
    }
    public void giveExperiencePoints(int i) {
        player.giveExp(i);
    }
    public void disconnect() {
        player.kickPlayer("Disconnected");
    }
    public boolean hasDisconnected() {
        return !player.isOnline();
    }
    public void resetSentInfo() {
    }
    protected void completeUsingItem() {
        player.completeUsingActiveItem();
    }
    public void lookAt(EntityAnchorArgument.Anchor anchor, Vec3 pos) {
        player.lookAt(pos.x, pos.y, pos.z, anchor == EntityAnchorArgument.Anchor.EYES ? io.papermc.paper.entity.LookAnchor.EYES : io.papermc.paper.entity.LookAnchor.FEET);
    }
    public void lookAt(EntityAnchorArgument.Anchor fromAnchor, Entity entity, EntityAnchorArgument.Anchor toAnchor) {
        org.bukkit.Location at = ((org.bukkit.entity.Entity) entity.getBukkitEntity()).getLocation();
        lookAt(fromAnchor, new Vec3(at.getX(), at.getY(), at.getZ()));
    }
    public void restoreFrom(ServerPlayer oldPlayer, boolean restoreAll) {

    }
    protected void onEffectAdded(MobEffectInstance effect, Entity source) {
    }
    protected void onEffectUpdated(MobEffectInstance effect, boolean doRefreshAttributes, Entity source) {
    }
    protected void onEffectsRemoved(Collection<MobEffectInstance> effects) {
    }
    public void teleportTo(double x, double y, double z) {
        player.teleport(new org.bukkit.Location(player.getWorld(), x, y, z, player.getLocation().getYaw(), player.getLocation().getPitch()));
    }
    public void teleportRelative(double dx, double dy, double dz) {
        org.bukkit.Location at = player.getLocation();
        teleportTo(at.getX() + dx, at.getY() + dy, at.getZ() + dz);
    }
    public boolean teleportTo(ServerLevel level, double x, double y, double z, Set<Relative> relatives, float newYRot, float newXRot, boolean resetCamera) {
        org.bukkit.Location at = player.getLocation();
        float yaw = relatives.contains(Relative.Y_ROT) ? at.getYaw() + newYRot : newYRot;
        float pitch = relatives.contains(Relative.X_ROT) ? at.getPitch() + newXRot : newXRot;
        return player.teleport(new org.bukkit.Location(level.getWorld(), relatives.contains(Relative.X) ? at.getX() + x : x, relatives.contains(Relative.Y) ? at.getY() + y : y, relatives.contains(Relative.Z) ? at.getZ() + z : z, yaw, pitch));
    }
    public void snapTo(double x, double y, double z) {
        teleportTo(x, y, z);
    }
    public void crit(Entity entity) {
    }
    public void magicCrit(Entity entity) {
    }
    public void onUpdateAbilities() {
    }
    public boolean setGameMode(GameType mode) {
        player.setGameMode(org.bukkit.GameMode.getByValue(mode.getId()));
        return true;
    }
    public GameType gameMode() {
        return GameType.byId(player.getGameMode().getValue());
    }
    public CommandSource commandSource() {
        return org.basaltmc.bukkit.facade.BukkitCommandBridge.sourceOf(player);
    }
    public CommandSourceStack createCommandSourceStack() {
        return org.basaltmc.bukkit.facade.BukkitCommandBridge.stackOf(player, net.minecraft.server.MinecraftServer.getServer());
    }
    public void sendSystemMessage(Component message) {
        connection.send(new net.minecraft.network.protocol.game.ClientboundSystemChatPacket(message, false));
    }
    public void sendOverlayMessage(Component message) {
        connection.send(new net.minecraft.network.protocol.game.ClientboundSystemChatPacket(message, true));
    }
    public void sendBuildLimitMessage(boolean isTooHigh, int limit) {
        connection.send(new net.minecraft.network.protocol.game.ClientboundSystemChatPacket(isTooHigh ? Component.translatable("build.tooHigh", limit - 1) : Component.translatable("build.tooLow"), true));
    }
    public void sendSpawnProtectionMessage(BlockPos pos) {
        sendOverlayMessage(Component.translatable("build.spawn_protection", pos.toShortString()).withStyle(net.minecraft.ChatFormatting.RED));
    }
    public void sendSystemMessage(Component message, boolean overlay) {
        connection.send(new net.minecraft.network.protocol.game.ClientboundSystemChatPacket(message, overlay));
    }
    public void sendChatMessage(OutgoingChatMessage message, boolean filtered, ChatType.Bound chatType) {
        connection.send(new net.minecraft.network.protocol.game.ClientboundDisguisedChatPacket(message.content(), chatType));
    }
    public String getIpAddress() {
        return player.getAddress() == null ? "" : player.getAddress().getAddress().getHostAddress();
    }
    public void updateOptions(ClientInformation information) {
        this.clientInformation = information;
    }
    public ClientInformation clientInformation() {
        return clientInformation != null ? clientInformation : ClientInformation.createDefault();
    }
    public boolean canChatInColor() {
        return Boolean.TRUE.equals(org.basaltmc.bukkit.PlayerNatives.get(player.getUniqueId().toString(), "chat_colors"));
    }
    public ChatVisiblity getChatVisibility() {
        return net.minecraft.world.entity.player.ChatVisiblity.FULL;
    }
    public int requestedViewDistance() {
        Object value = org.basaltmc.bukkit.PlayerNatives.get(player.getUniqueId().toString(), "client_view_distance");
        return value instanceof Number number ? number.intValue() : player.getClientViewDistance();
    }
    public void sendServerStatus(ServerStatus status) {
        connection.send(new net.minecraft.network.protocol.game.ClientboundServerDataPacket(status.description(), status.favicon().map(ServerStatus.Favicon::iconBytes)));
    }
    public PermissionSet permissions() {
        return org.basaltmc.bukkit.facade.BukkitCommandBridge.permissionsOf(player);
    }
    public void resetLastActionTime() {
        org.basaltmc.bukkit.PlayerNatives.call(player.getUniqueId().toString(), "reset_idle", new Object[0]);
    }
    public ServerStatsCounter getStats() {
        if (stats == null) {
            stats = new ServerStatsCounter(this);
        }
        return stats;
    }
    public ServerRecipeBook getRecipeBook() {
        if (recipeBook == null) {
            recipeBook = new ServerRecipeBook(this);
        }
        return recipeBook;
    }
    protected void updateInvisibilityStatus() {
    }
    public Entity getCamera() {
        return this;
    }
    public void setCamera(Entity newCamera) {
        player.setSpectatorTarget(newCamera == null || newCamera == this ? null : (org.bukkit.entity.Entity) newCamera.getBukkitEntity());
    }
    protected void processPortalCooldown() {
    }
    public long getLastActionTime() {
        Object idle = org.basaltmc.bukkit.PlayerNatives.get(player.getUniqueId().toString(), "idle_ms");
        return System.currentTimeMillis() - (idle instanceof Number number ? number.longValue() : 0L);
    }
    public Component getTabListDisplayName() {
        return org.basaltmc.bukkit.facade.ChatJson.fromAdventure(player.playerListName());
    }
    public int getTabListOrder() {
        return player.getPlayerListOrder();
    }
    public boolean isChangingDimension() {
        return false;
    }
    public void hasChangedDimension() {
    }
    public PlayerAdvancements getAdvancements() {
        if (advancements == null) {
            advancements = new PlayerAdvancements(null, null, null, null, this);
        }
        return advancements;
    }
    public RespawnConfig getRespawnConfig() {
        org.bukkit.Location at = player.getRespawnLocation();
        if (at == null) {
            return null;
        }
        ServerLevel handle = ((org.bukkit.craftbukkit.CraftWorld) at.getWorld()).getHandle();
        net.minecraft.core.BlockPos pos = new net.minecraft.core.BlockPos(at.getBlockX(), at.getBlockY(), at.getBlockZ());
        return new RespawnConfig(new net.minecraft.world.level.storage.LevelData.RespawnData(
            new net.minecraft.core.GlobalPos(handle.dimension(), pos), at.getYaw(), at.getPitch()), false);
    }
    public void copyRespawnPosition(ServerPlayer player) {
        setRespawnPosition(player.getRespawnConfig(), false);
    }
    public void setRespawnPosition(RespawnConfig respawnConfig, boolean showMessage) {
        if (respawnConfig == null) {
            player.setRespawnLocation(null);
            return;
        }
        net.minecraft.core.GlobalPos target = respawnConfig.respawnData().globalPos();
        for (org.bukkit.World each : org.bukkit.Bukkit.getWorlds()) {
            if (((org.bukkit.craftbukkit.CraftWorld) each).getHandle().dimension().equals(target.dimension())) {
                net.minecraft.core.BlockPos pos = target.pos();
                player.setRespawnLocation(new org.bukkit.Location(each, pos.getX(), pos.getY(), pos.getZ(),
                    respawnConfig.respawnData().yaw(), respawnConfig.respawnData().pitch()), respawnConfig.forced());
                return;
            }
        }
    }
    public SectionPos getLastSectionPos() {
        return lastSectionPos == null ? SectionPos.of(player.getLocation().getBlockX() >> 4, player.getLocation().getBlockY() >> 4, player.getLocation().getBlockZ() >> 4) : lastSectionPos;
    }
    public void setLastSectionPos(SectionPos lastSectionPos) {
        this.lastSectionPos = lastSectionPos;
    }
    public ChunkTrackingView getChunkTrackingView() {
        return chunkTrackingView == null ? ChunkTrackingView.EMPTY : chunkTrackingView;
    }
    public void setChunkTrackingView(ChunkTrackingView chunkTrackingView) {
        this.chunkTrackingView = chunkTrackingView;
    }
    public ItemEntity drop(ItemStack itemStack, boolean thrownFromHand, Prediction prediction) {
        ItemEntity entity = super.drop(itemStack, thrownFromHand, prediction);
        if (thrownFromHand && entity != null && !entity.getItem().isEmpty()) {
            awardStat(net.minecraft.stats.Stats.ITEM_DROPPED.get(entity.getItem().getItem()), itemStack.getCount());
            awardStat(net.minecraft.stats.Stats.DROP);
        }
        return entity;
    }
    public TextFilter getTextFilter() {
        return TextFilter.DUMMY;
    }
    public void setServerLevel(ServerLevel level) {
        org.bukkit.Location at = player.getLocation();
        at.setWorld(level.getWorld());
        player.teleport(at);
    }
    public boolean isTextFilteringEnabled() {
        return false;
    }
    public boolean shouldFilterMessageTo(ServerPlayer serverPlayer) {
        return false;
    }
    public boolean mayInteract(ServerLevel level, BlockPos pos) {
        return !MinecraftServer.getServer().isUnderSpawnProtection(level, pos, this);
    }
    protected void updateUsingItem(ItemStack useItem) {
    }
    public void drop(boolean all) {
        player.dropItem(all);
    }
    public void handleExtraItemsCreatedOnUse(ItemStack extraItems) {
    }
    public boolean allowsListing() {
        return !Boolean.FALSE.equals(org.basaltmc.bukkit.PlayerNatives.get(player.getUniqueId().toString(), "listed"));
    }
    public WardenSpawnTracker getWardenSpawnTracker() {
        if (wardenSpawnTracker == null) {
            wardenSpawnTracker = new net.minecraft.world.entity.monster.warden.WardenSpawnTracker();
        }
        return wardenSpawnTracker;
    }
    public void setSpawnExtraParticlesOnFall(boolean toggle) {
    }
    public void onItemPickup(ItemEntity entity) {
    }
    public void setChatSession(RemoteChatSession chatSession) {
    }
    public RemoteChatSession getChatSession() {
        return null;
    }
    public void indicateDamage(double xd, double zd) {
    }
    public boolean startRiding(Entity entityToRide, boolean force, boolean sendEventAndTriggers) {
        return ((org.bukkit.entity.Entity) entityToRide.getBukkitEntity()).addPassenger(player);
    }
    public void removeVehicle() {
        player.leaveVehicle();
    }
    public CommonPlayerSpawnInfo createCommonSpawnInfo(ServerLevel level) {
        return new CommonPlayerSpawnInfo(level.dimensionTypeRegistration(), level.dimension(),
            net.minecraft.world.level.biome.BiomeManager.obfuscateSeed(level.getSeed()), gameMode(),
            java.util.Optional.empty(), level.isDebug(), level.isFlat(), getLastDeathLocation(), getPortalCooldown(),
            level.getSeaLevel());
    }
    public void setRaidOmenPosition(BlockPos raidOmenPosition) {
    }
    public void clearRaidOmenPosition() {
    }
    public BlockPos getRaidOmenPosition() {
        return null;
    }
    public Vec3 getKnownMovement() {
        org.bukkit.util.Vector velocity = player.getVelocity();
        return new Vec3(velocity.getX(), velocity.getY(), velocity.getZ());
    }
    public Vec3 getKnownSpeed() {
        return getKnownMovement();
    }
    public void setKnownMovement(Vec3 lastKnownClientMovement) {
    }
    protected float getEnchantedDamage(Entity entity, float dmg, DamageSource damageSource) {
        return dmg;
    }
    public void onEquippedItemBroken(ItemStack brokenItem, EquipmentSlot inSlot) {
    }
    public Input getLastClientInput() {
        return lastClientInput;
    }
    public void setLastClientInput(Input lastClientInput) {
        this.lastClientInput = lastClientInput;
    }
    public Vec3 getLastClientMoveIntent() {
        float sideways = (lastClientInput.left() ? 1.0F : 0.0F) - (lastClientInput.right() ? 1.0F : 0.0F);
        float forward = (lastClientInput.forward() ? 1.0F : 0.0F) - (lastClientInput.backward() ? 1.0F : 0.0F);
        return new Vec3(sideways, 0.0, forward);
    }
    public void registerEnderPearl(ThrownEnderpearl enderPearl) {
    }
    public void deregisterEnderPearl(ThrownEnderpearl enderPearl) {
    }
    public Set<ThrownEnderpearl> getEnderPearls() {
        return java.util.Set.of();
    }
    private CompoundTag shoulder(int side) {
        if (!(org.basaltmc.bukkit.player.PlayerCalls.call(player.getUniqueId(), "shoulder", (long) side) instanceof String held) || held.isEmpty()) {
            return new CompoundTag();
        }
        byte[] saved = org.basaltmc.bukkit.EntityNatives.snbtToNbt(held);
        CompoundTag tag = saved == null ? null : org.basaltmc.bukkit.facade.NbtCodec.fromBytes(saved);
        return tag == null ? new CompoundTag() : tag;
    }
    private void putOnShoulder(int side, CompoundTag tag) {
        String snbt = tag == null || tag.isEmpty() ? "" : org.basaltmc.bukkit.EntityNatives.nbtToSnbt(org.basaltmc.bukkit.facade.NbtCodec.toBytes(tag));
        org.basaltmc.bukkit.player.PlayerCalls.call(player.getUniqueId(), "set_shoulder", (long) side, snbt == null ? "" : snbt);
    }
    public CompoundTag getShoulderEntityLeft() {
        return shoulder(0);
    }
    protected void setShoulderEntityLeft(CompoundTag tag) {
        putOnShoulder(0, tag);
    }
    public CompoundTag getShoulderEntityRight() {
        return shoulder(1);
    }
    protected void setShoulderEntityRight(CompoundTag tag) {
        putOnShoulder(1, tag);
    }
    public long registerAndUpdateEnderPearlTicket(ThrownEnderpearl enderpearl) {
        return 0L;
    }
    public static long placeEnderPearlTicket(ServerLevel level, ChunkPos chunk) {
        return 0L;
    }
    public void requestDebugSubscriptions(Set<DebugSubscription<?>> subscriptions) {
    }
    public Set<DebugSubscription<?>> debugSubscriptions() {
        return java.util.Set.of();
    }
    public void swingAndResetAttackStrength(InteractionHand hand, SwingAnimation animation, boolean sendToSwingingEntity) {
        if (hand == InteractionHand.OFF_HAND) {
            player.swingOffHand();
        } else {
            player.swingMainHand();
        }
    }
    static record RespawnPosAngle(Vec3 position, float yaw, float pitch) {
        public static RespawnPosAngle of(Vec3 position, BlockPos lookAtBlockPos, float pitch) {
        Vec3 look = Vec3.atBottomCenterOf(lookAtBlockPos).subtract(position).normalize();
        float yaw = (float) net.minecraft.util.Mth.wrapDegrees(net.minecraft.util.Mth.atan2(look.z, look.x) * (180.0F / (float) Math.PI) - 90.0);
        return new RespawnPosAngle(position, yaw, pitch);
        }
    }
    public static record RespawnConfig(LevelData.RespawnData respawnData, boolean forced) {
        public static final Codec<RespawnConfig> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.server.level.ServerPlayer.RespawnConfig.class, "CODEC");
        public boolean isSamePosition(RespawnConfig other) {
        return other != null && respawnData().globalPos().equals(other.respawnData().globalPos());
        }
    }
    public static record SavedPosition(Optional<ResourceKey<Level>> dimension, Optional<Vec3> position, Optional<Vec2> rotation) {
        public static final MapCodec<SavedPosition> MAP_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.server.level.ServerPlayer.SavedPosition.class, "MAP_CODEC");
        public static final SavedPosition EMPTY = null;
    }
}
