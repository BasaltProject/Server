package org.basaltmc.bukkit.player;

import com.destroystokyo.paper.block.TargetBlockInfo;
import com.destroystokyo.paper.entity.TargetEntityInfo;
import java.util.HashSet;
import java.util.Map;
import java.util.Set;
import java.util.UUID;
import net.md_5.bungee.api.chat.BaseComponent;
import org.bukkit.Bukkit;
import org.bukkit.Chunk;
import org.bukkit.Input;
import org.bukkit.Location;
import org.bukkit.Sound;
import org.bukkit.block.Block;
import org.bukkit.block.BlockFace;
import org.bukkit.damage.DamageSource;
import org.bukkit.entity.Item;
import org.bukkit.entity.LivingEntity;
import org.bukkit.entity.Player;
import org.bukkit.inventory.EquipmentSlot;
import org.bukkit.inventory.ItemStack;
import org.bukkit.potion.PotionEffect;
import org.bukkit.potion.PotionEffectType;
import org.bukkit.util.RayTraceResult;

public abstract class PlayerLiving extends PlayerActions {
    private static final double kGravity = 0.08;
    private static final float kAirDrag = 0.98F;
    private static final int kWardenWarningCooldown = 200;

    private UUID key() {
        return getUniqueId();
    }

    @Override
    public void sendActionBar(String message) {
        sendActionBar(net.kyori.adventure.text.serializer.legacy.LegacyComponentSerializer.legacySection()
            .deserialize(message == null ? "" : message));
    }

    @Override
    public void sendActionBar(BaseComponent... message) {
        sendActionBar(PlayerChat.component(message));
    }

    @Override
    public void showTitle(BaseComponent title) {
        showTitle(new BaseComponent[] {title}, null, 10, 70, 20);
    }

    @Override
    public void showTitle(BaseComponent[] title) {
        showTitle(title, null, 10, 70, 20);
    }

    @Override
    public void setRotation(float yaw, float pitch) {
        Location here = getLocation();
        here.setYaw(yaw);
        here.setPitch(pitch);
        teleport(here);
    }

    @Override
    public void sendPotionEffectChange(LivingEntity entity, PotionEffect effect) {
        PlayerCalls.call(key(), "send_effect", (long) entity.getEntityId(), effect.getType().getKey().toString(),
            (long) effect.getAmplifier(), (long) effect.getDuration(), effect.isAmbient(), effect.hasParticles(),
            effect.hasIcon());
    }

    @Override
    public void sendPotionEffectChangeRemove(LivingEntity entity, PotionEffectType type) {
        PlayerCalls.call(key(), "send_effect_removed", (long) entity.getEntityId(), type.getKey().toString());
    }

    @Override
    public boolean isChunkSent(long chunkKey) {
        return getSentChunkKeys().contains(chunkKey);
    }

    @Override
    public Set<Long> getSentChunkKeys() {
        Set<Long> keys = new HashSet<>();
        if (PlayerCalls.call(key(), "sent_chunks") instanceof String[] flat) {
            for (int at = 0; at + 1 < flat.length; at += 2) {
                keys.add(Chunk.getChunkKey(Integer.parseInt(flat[at]), Integer.parseInt(flat[at + 1])));
            }
        }
        return keys;
    }

    @Override
    public Set<Chunk> getSentChunks() {
        Set<Chunk> chunks = new HashSet<>();
        if (PlayerCalls.call(key(), "sent_chunks") instanceof String[] flat) {
            for (int at = 0; at + 1 < flat.length; at += 2) {
                chunks.add(getWorld().getChunkAt(Integer.parseInt(flat[at]), Integer.parseInt(flat[at + 1])));
            }
        }
        return chunks;
    }

    private Set<Player> trackers(int entityId) {
        Set<Player> viewers = new HashSet<>();
        if (PlayerCalls.call(key(), "tracked_by", (long) entityId) instanceof String[] uuids) {
            for (String uuid : uuids) {
                Player viewer = Bukkit.getPlayer(UUID.fromString(uuid));
                if (viewer != null) {
                    viewers.add(viewer);
                }
            }
        }
        return viewers;
    }

    @Override
    public Set<Player> getTrackedBy() {
        return trackers(getEntityId());
    }

    @Override
    public boolean isTrackedBy(Player viewer) {
        return getTrackedBy().contains(viewer);
    }

    @Override
    public Set<Player> getTrackedPlayers() {
        Set<Player> tracked = new HashSet<>();
        for (Player other : Bukkit.getOnlinePlayers()) {
            if (!other.equals(this) && trackers(other.getEntityId()).contains(this)) {
                tracked.add(other);
            }
        }
        return tracked;
    }

    @Override
    public java.time.Duration getIdleDuration() {
        return java.time.Duration.ofMillis(PlayerCalls.integer(key(), "idle_ms"));
    }

    @Override
    public void resetIdleDuration() {
        PlayerCalls.call(key(), "reset_idle");
    }

    @Override
    public int getWardenWarningLevel() {
        return (int) PlayerCalls.integer(key(), "warden_warning_level");
    }

    @Override
    public void setWardenWarningLevel(int level) {
        PlayerCalls.set(key(), "warden_warning_level", (long) level);
    }

    @Override
    public int getWardenWarningCooldown() {
        return (int) PlayerCalls.integer(key(), "warden_warning_cooldown");
    }

    @Override
    public void setWardenWarningCooldown(int ticks) {
        PlayerCalls.set(key(), "warden_warning_cooldown", (long) ticks);
    }

    @Override
    public int getWardenTimeSinceLastWarning() {
        return (int) PlayerCalls.integer(key(), "warden_time_since_warning");
    }

    @Override
    public void setWardenTimeSinceLastWarning(int ticks) {
        PlayerCalls.set(key(), "warden_time_since_warning", (long) ticks);
    }

    @Override
    public void increaseWardenWarningLevel() {
        setWardenWarningLevel(getWardenWarningLevel() + 1);
        setWardenWarningCooldown(kWardenWarningCooldown);
        setWardenTimeSinceLastWarning(0);
    }

    @Override
    public void setPortalCooldown(int ticks) {
        PlayerCalls.set(key(), "portal_cooldown", (long) ticks);
    }

    @Override
    public int getPortalCooldown() {
        return (int) PlayerCalls.integer(key(), "portal_cooldown");
    }

    @Override
    public Input getCurrentInput() {
        int flags = (int) PlayerCalls.integer(key(), "input_flags");
        return new Input() {
            @Override
            public boolean isForward() {
                return (flags & 0x01) != 0;
            }

            @Override
            public boolean isBackward() {
                return (flags & 0x02) != 0;
            }

            @Override
            public boolean isLeft() {
                return (flags & 0x04) != 0;
            }

            @Override
            public boolean isRight() {
                return (flags & 0x08) != 0;
            }

            @Override
            public boolean isJump() {
                return (flags & 0x10) != 0;
            }

            @Override
            public boolean isSneak() {
                return (flags & 0x20) != 0;
            }

            @Override
            public boolean isSprint() {
                return (flags & 0x40) != 0;
            }
        };
    }

    @Override
    public boolean isJumping() {
        return getCurrentInput().isJump();
    }

    @Override
    public boolean isHandRaised() {
        return PlayerCalls.flag(key(), "using_item");
    }

    @Override
    public boolean hasActiveItem() {
        return isHandRaised();
    }

    @Override
    public boolean isBlocking() {
        return PlayerCalls.flag(key(), "blocking");
    }

    @Override
    public EquipmentSlot getActiveItemHand() {
        return PlayerCalls.flag(key(), "using_offhand") ? EquipmentSlot.OFF_HAND : EquipmentSlot.HAND;
    }

    @Override
    public ItemStack getActiveItem() {
        if (!hasActiveItem()) {
            return new ItemStack(org.bukkit.Material.AIR);
        }
        return getInventory().getItem(getActiveItemHand());
    }

    @Override
    public ItemStack getItemInUse() {
        return hasActiveItem() ? getActiveItem() : null;
    }

    @Override
    public ItemStack getWeapon() {
        return getInventory().getItemInMainHand();
    }

    @Override
    public void openBook(ItemStack book) {
        if (book == null || book.getType() != org.bukkit.Material.WRITTEN_BOOK
            && book.getType() != org.bukkit.Material.WRITABLE_BOOK) {
            throw new IllegalArgumentException("Book must be Material.WRITTEN_BOOK");
        }
        PlayerCalls.call(key(), "open_book", org.basaltmc.bukkit.Items.toBytes(book));
    }

    @Override
    public void playPickupItemAnimation(Item item, int quantity) {
        PlayerCalls.call(key(), "pickup_animation", (long) item.getEntityId(), (long) quantity);
    }

    private RayTraceResult blocks(int maxDistance, TargetBlockInfo.FluidMode fluids) {
        return getWorld().rayTraceBlocks(getEyeLocation(), getEyeLocation().getDirection(), maxDistance, fluids.bukkit,
            false);
    }

    @Override
    public Block getTargetBlock(int maxDistance, TargetBlockInfo.FluidMode fluids) {
        RayTraceResult hit = blocks(maxDistance, fluids);
        return hit == null ? null : hit.getHitBlock();
    }

    @Override
    public BlockFace getTargetBlockFace(int maxDistance, TargetBlockInfo.FluidMode fluids) {
        RayTraceResult hit = blocks(maxDistance, fluids);
        return hit == null ? null : hit.getHitBlockFace();
    }

    @Override
    public TargetBlockInfo getTargetBlockInfo(int maxDistance, TargetBlockInfo.FluidMode fluids) {
        RayTraceResult hit = blocks(maxDistance, fluids);
        return hit == null || hit.getHitBlock() == null ? null
            : new TargetBlockInfo(hit.getHitBlock(), hit.getHitBlockFace());
    }

    @Override
    public TargetEntityInfo getTargetEntityInfo(int maxDistance, boolean ignoreBlocks) {
        RayTraceResult hit = getWorld().rayTraceEntities(getEyeLocation(), getEyeLocation().getDirection(),
            maxDistance, 0.0, entity -> !entity.equals(this));
        if (hit == null || hit.getHitEntity() == null) {
            return null;
        }
        if (!ignoreBlocks) {
            RayTraceResult wall = blocks(maxDistance, TargetBlockInfo.FluidMode.NEVER);
            if (wall != null && wall.getHitPosition().distanceSquared(getEyeLocation().toVector())
                < hit.getHitPosition().distanceSquared(getEyeLocation().toVector())) {
                return null;
            }
        }
        return new TargetEntityInfo(hit.getHitEntity(), hit.getHitPosition());
    }

    @Override
    public Sound getHurtSound() {
        return Sound.ENTITY_PLAYER_HURT;
    }

    @Override
    public Sound getHurtSound(DamageSource source) {
        String type = source.getDamageType().getKey().getKey();
        return switch (type) {
            case "on_fire" -> Sound.ENTITY_PLAYER_HURT_ON_FIRE;
            case "drown" -> Sound.ENTITY_PLAYER_HURT_DROWN;
            case "sweet_berry_bush" -> Sound.ENTITY_PLAYER_HURT_SWEET_BERRY_BUSH;
            case "freeze" -> Sound.ENTITY_PLAYER_HURT_FREEZE;
            default -> Sound.ENTITY_PLAYER_HURT;
        };
    }

    @Override
    public Sound getDeathSound() {
        return Sound.ENTITY_PLAYER_DEATH;
    }

    @Override
    public Sound getFallDamageSound(int fallHeight) {
        return fallHeight > 4 ? getFallDamageSoundBig() : getFallDamageSoundSmall();
    }

    @Override
    public Sound getFallDamageSoundSmall() {
        return Sound.ENTITY_PLAYER_SMALL_FALL;
    }

    @Override
    public Sound getFallDamageSoundBig() {
        return Sound.ENTITY_PLAYER_BIG_FALL;
    }

    @Override
    public Sound getSwimSound() {
        return Sound.ENTITY_PLAYER_SWIM;
    }

    @Override
    public Sound getSwimSplashSound() {
        return Sound.ENTITY_PLAYER_SPLASH;
    }

    @Override
    public Sound getSwimHighSpeedSplashSound() {
        return Sound.ENTITY_PLAYER_SPLASH_HIGH_SPEED;
    }

    @Override
    public Sound getDrinkingSound(ItemStack item) {
        return Sound.ENTITY_GENERIC_DRINK;
    }

    @Override
    public Sound getEatingSound(ItemStack item) {
        return Sound.ENTITY_GENERIC_EAT;
    }

    @Override
    public boolean isTransferred() {
        return PlayerCalls.flag(key(), "transferred");
    }

    @Override
    public net.kyori.adventure.sound.Sound.Source soundSource() {
        return net.kyori.adventure.sound.Sound.Source.PLAYER;
    }

    @Override
    public int getBeeStingersInBody() {
        return (int) PlayerCalls.integer(key(), "bee_stingers");
    }

    @Override
    public void setBeeStingersInBody(int count) {
        PlayerCalls.set(key(), "bee_stingers", (long) count);
    }

    @Override
    public int getBeeStingerCooldown() {
        return (int) PlayerCalls.integer(key(), "bee_stinger_cooldown");
    }

    @Override
    public void setBeeStingerCooldown(int ticks) {
        PlayerCalls.set(key(), "bee_stinger_cooldown", (long) ticks);
    }

    @Override
    public int getArrowCooldown() {
        return (int) PlayerCalls.integer(key(), "arrow_cooldown");
    }

    @Override
    public void setArrowCooldown(int ticks) {
        PlayerCalls.set(key(), "arrow_cooldown", (long) ticks);
    }

    @Override
    public boolean isRiptiding() {
        return PlayerCalls.flag(key(), "riptiding");
    }

    @Override
    public boolean isClimbing() {
        return org.bukkit.Tag.CLIMBABLE.isTagged(getLocation().getBlock().getType());
    }

    @Override
    public boolean isInPowderedSnow() {
        return getLocation().getBlock().getType() == org.bukkit.Material.POWDER_SNOW;
    }

    @Override
    public float getSoundVolume() {
        return 1.0F;
    }

    @Override
    public double getGravity() {
        return getDefaultGravity();
    }

    @Override
    public double getDefaultGravity() {
        return kGravity;
    }

    @Override
    public float getAirDrag() {
        return kAirDrag;
    }

    @Override
    public org.bukkit.event.entity.CreatureSpawnEvent.SpawnReason getEntitySpawnReason() {
        return org.bukkit.event.entity.CreatureSpawnEvent.SpawnReason.DEFAULT;
    }

    @Override
    public io.papermc.paper.entity.RemovalReason getRemovalReason() {
        if (isValid()) {
            return null;
        }
        throw PlayerCalls.unsupported("Player#getRemovalReason");
    }

    @Override
    public Map<String, Object> serialize() {
        return Map.of("name", getName());
    }
}
