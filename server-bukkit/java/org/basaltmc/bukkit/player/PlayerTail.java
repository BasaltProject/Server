package org.basaltmc.bukkit.player;

import java.util.Collection;
import java.util.HashSet;
import java.util.Map;
import java.util.Set;
import java.util.UUID;
import java.util.concurrent.ConcurrentHashMap;
import net.kyori.adventure.key.Key;
import net.kyori.adventure.util.TriState;
import org.basaltmc.bukkit.EntityNatives;
import org.basaltmc.bukkit.Items;
import org.basaltmc.bukkit.misc.AdventureBars;
import org.bukkit.Color;
import org.bukkit.Location;
import org.bukkit.block.Block;
import org.bukkit.block.TileState;
import org.bukkit.entity.Entity;
import org.bukkit.entity.Firework;
import org.bukkit.entity.LivingEntity;
import org.bukkit.entity.Player;
import org.bukkit.entity.Pose;
import org.bukkit.entity.memory.MemoryKey;
import org.bukkit.event.entity.EntityRemoveEvent;
import org.bukkit.inventory.EquipmentSlot;
import org.bukkit.inventory.ItemStack;

/** The players' state the server keeps for them alone: item use, fixed pose, shoulders, waypoint and the like. */
public abstract class PlayerTail extends PlayerActions {
    private static final long kUnset = -1;

    private final Map<MemoryKey<?>, Object> memories = new ConcurrentHashMap<>();
    private final Set<UUID> collidableExemptions = ConcurrentHashMap.newKeySet();
    private volatile TriState frictionState = TriState.NOT_SET;
    private volatile Boolean jumpOverride;
    private volatile int jumpOverrideInput;

    private UUID key() {
        return getUniqueId();
    }

    private static int slotIndex(EquipmentSlot hand) {
        return hand == EquipmentSlot.OFF_HAND ? 1 : 0;
    }

    @Override
    public void startUsingItem(EquipmentSlot hand) {
        if (hand != EquipmentSlot.HAND && hand != EquipmentSlot.OFF_HAND) {
            throw new IllegalArgumentException("Only a hand can use an item");
        }
        PlayerCalls.call(key(), "use_start", hand == EquipmentSlot.OFF_HAND);
    }

    @Override
    public void completeUsingActiveItem() {
        PlayerCalls.call(key(), "use_complete");
    }

    @Override
    public void clearActiveItem() {
        PlayerCalls.call(key(), "use_clear");
    }

    @Override
    public int getActiveItemRemainingTime() {
        return (int) Math.max(PlayerCalls.integer(key(), "use_remaining"), 0L);
    }

    @Override
    public void setActiveItemRemainingTime(int ticks) {
        PlayerCalls.set(key(), "use_remaining", (long) ticks);
    }

    @Override
    public int getActiveItemUsedTime() {
        return (int) Math.max(PlayerCalls.integer(key(), "use_ticks"), 0L);
    }

    @Override
    public int getItemInUseTicks() {
        return getActiveItemRemainingTime();
    }

    @Override
    public void setItemInUseTicks(int ticks) {
        setActiveItemRemainingTime(ticks);
    }

    @Override
    public TriState getVisualFire() {
        long value = PlayerCalls.integer(key(), "visual_fire");
        return value < 0 ? TriState.NOT_SET : value == 0 ? TriState.FALSE : TriState.TRUE;
    }

    @Override
    public void setVisualFire(TriState fire) {
        PlayerCalls.set(key(), "visual_fire", fire == TriState.NOT_SET ? kUnset : fire == TriState.FALSE ? 0L : 1L);
    }

    @Override
    public void setVisualFire(boolean fire) {
        setVisualFire(fire ? TriState.TRUE : TriState.FALSE);
    }

    @Override
    public boolean isVisualFire() {
        return getVisualFire() == TriState.TRUE;
    }

    @Override
    public void setPose(Pose pose, boolean fixed) {
        if (fixed) {
            PlayerCalls.set(key(), "fixed_pose", (long) pose.ordinal());
        } else {
            unsetFixedPose();
        }
    }

    @Override
    public boolean hasFixedPose() {
        return PlayerCalls.integer(key(), "fixed_pose") >= 0;
    }

    @Override
    public void unsetFixedPose() {
        PlayerCalls.set(key(), "fixed_pose", kUnset);
    }

    @Override
    public Pose getPose() {
        long ordinal = PlayerCalls.integer(key(), "pose");
        Pose[] poses = Pose.values();
        return ordinal < 0 || ordinal >= poses.length ? Pose.STANDING : poses[(int) ordinal];
    }

    @Override
    public void setNoPhysics(boolean noPhysics) {
        PlayerCalls.set(key(), "no_physics", noPhysics);
    }

    @Override
    public boolean hasNoPhysics() {
        return PlayerCalls.flag(key(), "no_physics");
    }

    @Override
    public void lockFreezeTicks(boolean locked) {
        PlayerCalls.set(key(), "freeze_locked", locked);
    }

    @Override
    public boolean isFreezeTickingLocked() {
        return PlayerCalls.flag(key(), "freeze_locked");
    }

    @Override
    public void setWaypointStyle(Key style) {
        PlayerCalls.set(key(), "waypoint_style", style == null ? "minecraft:default" : style.asString());
    }

    @Override
    public Key getWaypointStyle() {
        String text = PlayerCalls.text(key(), "waypoint_style");
        return Key.key(text == null || text.isEmpty() ? "minecraft:default" : text);
    }

    @Override
    public void setWaypointColor(Color color) {
        PlayerCalls.set(key(), "waypoint_color", color == null ? kUnset : (long) color.asRGB());
    }

    @Override
    public Color getWaypointColor() {
        long rgb = PlayerCalls.integer(key(), "waypoint_color");
        return rgb < 0 ? null : Color.fromRGB((int) (rgb & 0xFFFFFF));
    }

    @Override
    public void startRiptideAttack(int duration, float damage, ItemStack item) {
        if (duration <= 0) {
            throw new IllegalArgumentException("Duration must be greater than 0");
        }
        if (damage < 0.0F) {
            throw new IllegalArgumentException("Damage must not be negative");
        }
        PlayerCalls.call(key(), "riptide", (long) duration, (double) damage, Items.toSnbt(item));
    }

    @Override
    public void setRiptiding(boolean riptiding) {
        PlayerCalls.call(key(), "set_riptiding", riptiding);
    }

    @Override
    public Firework fireworkBoost(ItemStack booster) {
        if (booster == null || booster.getType() != org.bukkit.Material.FIREWORK_ROCKET) {
            throw new IllegalArgumentException("Item must be a firework rocket");
        }
        Object made = PlayerCalls.call(key(), "firework_boost", Items.toSnbt(booster));
        Entity rocket = made instanceof Number id && id.intValue() >= 0 ? getWorld().getEntities().stream()
            .filter(entity -> entity.getEntityId() == id.intValue()).findFirst().orElse(null) : null;
        return rocket instanceof Firework firework ? firework : null;
    }

    @Override
    public boolean sleep(Location location, boolean force) {
        if (location == null || location.getWorld() == null) {
            throw new IllegalArgumentException("Location needs to be in a world");
        }
        if (!location.getWorld().equals(getWorld())) {
            throw new IllegalArgumentException("Cannot sleep across worlds");
        }
        org.basaltmc.bukkit.Blocks.flushAll();
        return PlayerCalls.done(key(), "sleep", (long) location.getBlockX(), (long) location.getBlockY(),
            (long) location.getBlockZ(), force);
    }

    @Override
    public void sendBlockUpdate(Location location, TileState tile) {
        if (location == null || tile == null) {
            throw new IllegalArgumentException("Location and tile state cannot be null");
        }
        Map<String, Object> data = org.basaltmc.bukkit.tile.TileStates.dataOf(tile);
        if (data == null) {
            throw new IllegalArgumentException("Not a tile state of the server: " + tile);
        }
        String snbt = EntityNatives.nbtToSnbt(org.basaltmc.bukkit.item.Nbt.writeNamed(data));
        PlayerCalls.call(key(), "block_entity", (long) location.getBlockX(), (long) location.getBlockY(),
            (long) location.getBlockZ(), tile.getType().getKey().toString(), snbt == null ? "" : snbt);
    }

    @Override
    public void loadData() {
        PlayerCalls.call(key(), "load_data");
    }

    @Override
    public void setMaximumAir(int ticks) {
        PlayerCalls.set(key(), "maximum_air", (long) ticks);
    }

    @Override
    public int getMaximumAir() {
        return (int) PlayerCalls.integer(key(), "maximum_air");
    }

    @Override
    public void setMaximumNoDamageTicks(int ticks) {
        PlayerCalls.set(key(), "max_no_damage_ticks", (long) ticks);
    }

    @Override
    public int getMaximumNoDamageTicks() {
        return (int) PlayerCalls.integer(key(), "max_no_damage_ticks");
    }

    @Override
    public void setSaturatedRegenRate(int ticks) {
        PlayerCalls.set(key(), "saturated_regen_ticks", (long) ticks);
    }

    @Override
    public int getSaturatedRegenRate() {
        return (int) PlayerCalls.integer(key(), "saturated_regen_ticks");
    }

    @Override
    public void setUnsaturatedRegenRate(int ticks) {
        PlayerCalls.set(key(), "unsaturated_regen_ticks", (long) ticks);
    }

    @Override
    public int getUnsaturatedRegenRate() {
        return (int) PlayerCalls.integer(key(), "unsaturated_regen_ticks");
    }

    @Override
    public void setStarvationRate(int ticks) {
        PlayerCalls.set(key(), "starvation_ticks", (long) ticks);
    }

    @Override
    public int getStarvationRate() {
        return (int) PlayerCalls.integer(key(), "starvation_ticks");
    }

    @Override
    public int getNoActionTicks() {
        return (int) PlayerCalls.integer(key(), "no_action_ticks");
    }

    @Override
    public void setNoActionTicks(int ticks) {
        PlayerCalls.set(key(), "no_action_ticks", (long) ticks);
    }

    @Override
    public float getBodyYaw() {
        return (float) PlayerCalls.real(key(), "body_yaw");
    }

    @Override
    public void setBodyYaw(float yaw) {
        PlayerCalls.set(key(), "body_yaw", (double) yaw);
    }

    @Override
    public float getHurtDirection() {
        return (float) PlayerCalls.real(key(), "hurt_direction");
    }

    @Override
    public void setHurtDirection(float direction) {
        PlayerCalls.set(key(), "hurt_direction", (double) direction);
    }

    @Override
    public void setKiller(Player killer) {
        PlayerCalls.set(key(), "killer", killer == null ? "" : killer.getUniqueId().toString());
    }

    @Override
    public void setJumping(boolean jumping) {
        jumpOverrideInput = (int) PlayerCalls.integer(key(), "input_flags");
        jumpOverride = jumping;
    }

    @Override
    public boolean isJumping() {
        Boolean held = jumpOverride;
        if (held != null && jumpOverrideInput == (int) PlayerCalls.integer(key(), "input_flags")) {
            return held;
        }
        jumpOverride = null;
        return getCurrentInput().isJump();
    }

    @Override
    public void registerAttribute(org.bukkit.attribute.Attribute attribute) {
        if (getAttribute(attribute) == null) {
            throw org.basaltmc.bukkit.Unsupported.hit("Attributable#registerAttribute of " + attribute.getKey());
        }
    }

    @Override
    public <T> T getMemory(MemoryKey<T> memoryKey) {
        return memoryKey.getMemoryClass().cast(memories.get(memoryKey));
    }

    @Override
    public <T> void setMemory(MemoryKey<T> memoryKey, T value) {
        if (value == null) {
            memories.remove(memoryKey);
        } else {
            memories.put(memoryKey, value);
        }
    }

    @Override
    public Set<UUID> getCollidableExemptions() {
        return collidableExemptions;
    }

    @Override
    public TriState getFrictionState() {
        return frictionState;
    }

    @Override
    public void setFrictionState(TriState state) {
        frictionState = state == null ? TriState.NOT_SET : state;
    }

    @Override
    public EntityRemoveEvent.Cause getRemoveEventCause() {
        if (isValid()) {
            throw new IllegalStateException("Entity has not been removed yet");
        }
        return EntityRemoveEvent.Cause.PLAYER_QUIT;
    }

    @Override
    public void resetFlyingTicks() {}

    @Override
    public float getForwardsMovement() {
        org.bukkit.Input input = getCurrentInput();
        return input.isForward() == input.isBackward() ? 0.0F : input.isForward() ? 1.0F : -1.0F;
    }

    @Override
    public float getSidewaysMovement() {
        org.bukkit.Input input = getCurrentInput();
        return input.isLeft() == input.isRight() ? 0.0F : input.isLeft() ? 1.0F : -1.0F;
    }

    @Override
    public float getUpwardsMovement() {
        return 0.0F;
    }

    @Override
    public Iterable<? extends net.kyori.adventure.bossbar.BossBar> activeBossBars() {
        return AdventureBars.shownTo(key());
    }

    @Override
    public void broadcastSlotBreak(EquipmentSlot slot) {
        broadcastSlotBreak(slot, getWorld().getPlayers());
    }

    @Override
    public void broadcastSlotBreak(EquipmentSlot slot, Collection<Player> players) {
        long event = switch (slot) {
            case HAND -> 47L;
            case OFF_HAND -> 48L;
            case HEAD -> 49L;
            case CHEST -> 50L;
            case LEGS -> 51L;
            case FEET -> 52L;
            case BODY -> 65L;
            default -> throw new IllegalArgumentException("No break animation for " + slot);
        };
        for (Player viewer : new HashSet<>(players)) {
            PlayerCalls.call(viewer.getUniqueId(), "entity_event", (long) getEntityId(), event);
        }
    }

    @Override
    public boolean canCriticalAttack(Entity target) {
        return getFallDistance() > 0.0F && !isOnGround() && !isClimbing() && !isInWater()
            && !hasPotionEffect(org.bukkit.potion.PotionEffectType.BLINDNESS) && !isInsideVehicle()
            && target instanceof LivingEntity && !isSprinting();
    }

    @Override
    public org.bukkit.inventory.InventoryView openMerchant(org.bukkit.inventory.Merchant merchant, boolean force) {
        if (merchant == null) {
            throw new IllegalArgumentException("merchant cannot be null");
        }
        if (!force && merchant.isTrading()) {
            return null;
        }
        boolean opened;
        if (merchant instanceof org.bukkit.entity.AbstractVillager villager) {
            opened = PlayerCalls.done(key(), "open_trades", (long) villager.getEntityId());
        } else {
            net.kyori.adventure.text.Component title = merchant instanceof org.basaltmc.bukkit.inv.BasaltMerchant own
                ? own.title() : net.kyori.adventure.text.Component.empty();
            opened = PlayerCalls.done(key(), "open_merchant", org.basaltmc.bukkit.misc.Json.of(title),
                offersOf(merchant));
        }
        return opened ? getOpenInventory() : null;
    }

    private static String offersOf(org.bukkit.inventory.Merchant merchant) {
        StringBuilder out = new StringBuilder("{offers:[");
        boolean first = true;
        for (org.bukkit.inventory.MerchantRecipe recipe : merchant.getRecipes()) {
            java.util.List<ItemStack> ingredients = recipe.getIngredients();
            if (ingredients.isEmpty()) {
                continue;
            }
            out.append(first ? "" : ",").append("{buy:").append(Items.toSnbt(ingredients.get(0)));
            if (ingredients.size() > 1 && !ingredients.get(1).getType().isAir()) {
                out.append(",buy2:").append(Items.toSnbt(ingredients.get(1)));
            }
            out.append(",sell:").append(Items.toSnbt(recipe.getResult())).append(",uses:").append(recipe.getUses())
                .append(",max_uses:").append(recipe.getMaxUses()).append(",xp:").append(recipe.getVillagerExperience())
                .append(",price_multiplier:").append(recipe.getPriceMultiplier()).append("f,demand:")
                .append(recipe.getDemand()).append(",special_price:").append(recipe.getSpecialPrice()).append('}');
            first = false;
        }
        return out.append("]}").toString();
    }

    @Override
    public void setPlayerProfile(com.destroystokyo.paper.profile.PlayerProfile profile) {
        if (profile == null) {
            throw new IllegalArgumentException("The profile cannot be null");
        }
        java.util.List<Object> flat = new java.util.ArrayList<>();
        for (com.destroystokyo.paper.profile.ProfileProperty property : profile.getProperties()) {
            flat.add(property.getName());
            flat.add(property.getValue());
            flat.add(property.getSignature() == null ? "" : property.getSignature());
        }
        PlayerCalls.call(key(), "set_profile", flat.toArray());
    }

    private volatile io.papermc.paper.world.damagesource.CombatTracker combat;

    @Override
    public io.papermc.paper.world.damagesource.CombatTracker getCombatTracker() {
        io.papermc.paper.world.damagesource.CombatTracker held = combat;
        if (held == null) {
            held = combat = new BasaltCombatTracker(this);
        }
        return held;
    }

    @Override
    public int getShieldBlockingDelay() {
        return (int) PlayerCalls.integer(key(), "shield_delay");
    }

    @Override
    public void setShieldBlockingDelay(int delay) {
        PlayerCalls.set(key(), "shield_delay", (long) delay);
    }

    @Override
    public void closeDialog() {
        PlayerCalls.call(key(), "clear_dialog");
    }

    @Override
    public void showDialog(net.kyori.adventure.dialog.DialogLike dialog) {
        if (!(dialog instanceof io.papermc.paper.dialog.Dialog shown)) {
            throw new IllegalArgumentException("Not a dialog: " + dialog);
        }
        PlayerCalls.call(key(), "show_dialog", shown instanceof org.basaltmc.bukkit.dialog.BasaltDialog inline
            ? inline.snbt() : shown.getKey().toString());
    }

    @Override
    public void deleteMessage(net.kyori.adventure.chat.SignedMessage.Signature signature) {
        StringBuilder hex = new StringBuilder();
        for (byte one : signature.bytes()) {
            hex.append(String.format("%02x", one));
        }
        PlayerCalls.call(key(), "delete_chat", hex.toString());
    }

    @Override
    public void openBook(net.kyori.adventure.inventory.Book book) {
        ItemStack item = new ItemStack(org.bukkit.Material.WRITTEN_BOOK);
        org.bukkit.inventory.meta.BookMeta meta = (org.bukkit.inventory.meta.BookMeta) item.getItemMeta();
        meta.title(book.title());
        meta.author(book.author());
        meta.addPages(book.pages().toArray(new net.kyori.adventure.text.Component[0]));
        item.setItemMeta(meta);
        openBook(item);
    }

    @Override
    public void removeResourcePacks(UUID id, UUID... others) {
        PlayerCalls.call(key(), "remove_resource_pack", id.toString());
        for (UUID other : others) {
            PlayerCalls.call(key(), "remove_resource_pack", other.toString());
        }
    }

    @Override
    public String getAsString() {
        return PlayerCalls.call(key(), "as_string") instanceof String text && !text.isEmpty() ? text : null;
    }

    private Entity shoulder(int side) {
        Object value = PlayerCalls.call(key(), "shoulder", (long) side);
        if (!(value instanceof String held) || held.isEmpty()) {
            return null;
        }
        byte[] saved = EntityNatives.snbtToNbt(held);
        net.minecraft.nbt.CompoundTag tag = saved == null ? null : org.basaltmc.bukkit.facade.NbtCodec.fromBytes(saved);
        if (tag == null) {
            return null;
        }
        org.bukkit.entity.EntityType type = org.basaltmc.bukkit.entity.EntityCore.typeOf(tag.getStringOr("id", ""));
        if (type == org.bukkit.entity.EntityType.UNKNOWN) {
            return null;
        }
        Location at = getLocation();
        Entity made = org.basaltmc.bukkit.entity.EntityCore.template(
            (org.basaltmc.bukkit.BasaltWorld) at.getWorld(), type, at.getX(), at.getY(), at.getZ(), 0.0F, 0.0F);
        ((org.basaltmc.bukkit.entity.Cored) made).core().seedNbt(saved);
        return made;
    }

    private void putOnShoulder(int side, Entity entity) {
        if (entity == null) {
            PlayerCalls.call(key(), "set_shoulder", (long) side, "");
            return;
        }
        if (!(entity.createSnapshot() instanceof org.basaltmc.bukkit.entity.BasaltEntitySnapshot snapshot)) {
            throw new IllegalArgumentException("Cannot put " + entity + " on a shoulder");
        }
        String snbt = EntityNatives.nbtToSnbt(snapshot.nbt());
        PlayerCalls.call(key(), "set_shoulder", (long) side, snbt == null ? "" : snbt);
        entity.remove();
    }

    private Entity release(int side) {
        Object held = PlayerCalls.call(key(), "shoulder", (long) side);
        if (!(held instanceof String snbt) || snbt.isEmpty()) {
            return null;
        }
        PlayerCalls.call(key(), "set_shoulder", (long) side, "");
        return org.basaltmc.bukkit.entity.BasaltEntitySnapshot.parse(snbt).createEntity(getLocation());
    }

    @Override
    public Entity getShoulderEntityLeft() {
        return shoulder(0);
    }

    @Override
    public void setShoulderEntityLeft(Entity entity) {
        putOnShoulder(0, entity);
    }

    @Override
    public Entity getShoulderEntityRight() {
        return shoulder(1);
    }

    @Override
    public void setShoulderEntityRight(Entity entity) {
        putOnShoulder(1, entity);
    }

    @Override
    public Entity releaseLeftShoulderEntity() {
        return release(0);
    }

    @Override
    public Entity releaseRightShoulderEntity() {
        return release(1);
    }

    @Override
    public boolean breakBlock(Block block) {
        return org.basaltmc.bukkit.player.PlayerBreaking.breakBlock(this, block);
    }
}
