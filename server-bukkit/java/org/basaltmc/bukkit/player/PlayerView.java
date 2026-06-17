package org.basaltmc.bukkit.player;

import io.papermc.paper.math.Position;
import java.util.Collection;
import java.util.List;
import java.util.Map;
import java.util.UUID;
import net.kyori.adventure.text.Component;
import org.basaltmc.bukkit.BasaltBlockData;
import org.basaltmc.bukkit.BasaltServer;
import org.basaltmc.bukkit.BasaltWorld;
import org.basaltmc.bukkit.Text;
import org.bukkit.DyeColor;
import org.bukkit.GameMode;
import org.bukkit.Location;
import org.bukkit.WeatherType;
import org.bukkit.WorldBorder;
import org.bukkit.block.BlockState;
import org.bukkit.block.data.BlockData;
import org.bukkit.block.sign.Side;
import org.bukkit.entity.Entity;
import org.bukkit.entity.Player;
import org.bukkit.plugin.Plugin;

public abstract class PlayerView extends PlayerState {
    private WorldBorder border;

    private UUID key() {
        return getUniqueId();
    }

    @Override
    public void setPlayerTime(long time, boolean relative) {
        PlayerCalls.call(key(), "set_player_time", time, relative);
    }

    @Override
    public void resetPlayerTime() {
        PlayerCalls.call(key(), "reset_player_time");
    }

    @Override
    public boolean isPlayerTimeRelative() {
        return PlayerCalls.flag(key(), "player_time_relative");
    }

    @Override
    public long getPlayerTimeOffset() {
        Object held = PlayerCalls.get(key(), "player_time");
        if (!(held instanceof Number offset)) {
            return 0L;
        }
        return isPlayerTimeRelative() ? offset.longValue() : offset.longValue() - getWorld().getFullTime();
    }

    @Override
    public long getPlayerTime() {
        Object held = PlayerCalls.get(key(), "player_time");
        long world = getWorld().getFullTime();
        if (!(held instanceof Number time)) {
            return world;
        }
        return isPlayerTimeRelative() ? world + time.longValue() : time.longValue();
    }

    @Override
    public void setPlayerWeather(WeatherType type) {
        PlayerCalls.call(key(), "set_player_weather", type == WeatherType.DOWNFALL ? "downfall" : "clear");
    }

    @Override
    public void resetPlayerWeather() {
        PlayerCalls.call(key(), "set_player_weather", (Object) null);
    }

    @Override
    public WeatherType getPlayerWeather() {
        String held = PlayerCalls.text(key(), "player_weather");
        return held == null ? null : held.equals("downfall") ? WeatherType.DOWNFALL : WeatherType.CLEAR;
    }

    @Override
    public void hideEntity(Plugin plugin, Entity entity) {
        if (entity.equals(this)) {
            return;
        }
        PlayerCalls.call(key(), "hide_entity", (long) entity.getEntityId());
    }

    @Override
    public void showEntity(Plugin plugin, Entity entity) {
        PlayerCalls.call(key(), "show_entity", (long) entity.getEntityId());
    }

    @Override
    public void hidePlayer(Player player) {
        hideEntity(null, player);
    }

    @Override
    public void showPlayer(Player player) {
        showEntity(null, player);
    }

    @Override
    public boolean canSee(Entity entity) {
        return entity.equals(this) || PlayerCalls.done(key(), "can_see", (long) entity.getEntityId());
    }

    @Override
    public boolean canSee(Player player) {
        return canSee((Entity) player);
    }

    @Override
    public void setVisibleByDefault(boolean visible) {
        PlayerCalls.call(key(), "visible_by_default", (long) getEntityId(), visible);
    }

    @Override
    public boolean isVisibleByDefault() {
        return PlayerCalls.done(key(), "visible_by_default", (long) getEntityId());
    }

    @Override
    public boolean listPlayer(Player player) {
        boolean changed = !isListed(player);
        PlayerCalls.call(key(), "list_player", (long) player.getEntityId(), true);
        return changed;
    }

    @Override
    public boolean unlistPlayer(Player player) {
        boolean changed = isListed(player);
        PlayerCalls.call(key(), "list_player", (long) player.getEntityId(), false);
        return changed;
    }

    @Override
    public boolean isListed(Player player) {
        return !PlayerCalls.done(key(), "list_unlisted", (long) player.getEntityId());
    }

    @Override
    public Entity getSpectatorTarget() {
        long id = PlayerCalls.integer(key(), "spectator_target");
        return id < 0 ? null : BasaltServer.instance().entityById((BasaltWorld) getWorld(), (int) id);
    }

    @Override
    public void setSpectatorTarget(Entity entity) {
        if (getGameMode() != GameMode.SPECTATOR) {
            throw new IllegalStateException("Player must be in spectator mode");
        }
        PlayerCalls.set(key(), "spectator_target", entity == null ? -1L : (long) entity.getEntityId());
    }

    @Override
    public Location getCompassTarget() {
        Object held = PlayerCalls.get(key(), "compass_target");
        if (held instanceof String[] at && at.length == 3) {
            return new Location(getWorld(), Integer.parseInt(at[0]), Integer.parseInt(at[1]), Integer.parseInt(at[2]));
        }
        return getWorld().getSpawnLocation();
    }

    @Override
    public void setCompassTarget(Location location) {
        PlayerCalls.call(key(), "set_compass_target", (long) location.getBlockX(), (long) location.getBlockY(),
            (long) location.getBlockZ());
    }

    @Override
    public void sendBlockChange(Location location, BlockData data) {
        PlayerCalls.call(key(), "block_change", (long) location.getBlockX(), (long) location.getBlockY(),
            (long) location.getBlockZ(), (long) BasaltBlockData.stateOf(data));
    }

    @Override
    public void sendBlockChange(Location location, org.bukkit.Material material, byte data) {
        sendBlockChange(location, material.createBlockData());
    }

    @Override
    public void sendBlockChanges(Collection<BlockState> blocks) {
        for (BlockState state : blocks) {
            sendBlockChange(state.getLocation(), state.getBlockData());
        }
    }

    @Override
    public void sendMultiBlockChange(Map<? extends Position, BlockData> blocks) {
        for (Map.Entry<? extends Position, BlockData> entry : blocks.entrySet()) {
            Position at = entry.getKey();
            PlayerCalls.call(key(), "block_change", (long) at.blockX(), (long) at.blockY(), (long) at.blockZ(),
                (long) BasaltBlockData.stateOf(entry.getValue()));
        }
    }

    @Override
    public void sendBlockDamage(Location location, float progress, int entityId) {
        if (progress < 0.0F || progress > 1.0F) {
            throw new IllegalArgumentException("progress must be between 0.0 and 1.0 (inclusive)");
        }
        PlayerCalls.call(key(), "block_damage", (long) location.getBlockX(), (long) location.getBlockY(),
            (long) location.getBlockZ(), progress == 0.0F ? -1L : (long) (progress * 9.0F), (long) entityId);
    }

    @Override
    public void sendBlockDamage(Location location, float progress, Entity source) {
        sendBlockDamage(location, progress, source.getEntityId());
    }

    private static String joined(List<String> lines) {
        return String.join("\n", lines);
    }

    private static List<String> padded(List<? extends Component> lines) {
        String[] out = new String[4];
        for (int at = 0; at < 4; at++) {
            out[at] = lines != null && at < lines.size() ? Text.mini(lines.get(at)) : "";
        }
        return List.of(out);
    }

    private void signText(Location location, List<String> lines, DyeColor color, boolean glowing) {
        PlayerCalls.call(key(), "sign_text", (long) location.getBlockX(), (long) location.getBlockY(),
            (long) location.getBlockZ(), joined(lines), "", (color == null ? DyeColor.BLACK : color).name()
                .toLowerCase(java.util.Locale.ROOT), glowing);
    }

    @Override
    public void sendSignChange(Location location, String[] lines, DyeColor color, boolean hasGlowingText) {
        if (lines == null) {
            lines = new String[4];
        }
        if (lines.length < 4) {
            throw new IllegalArgumentException("Must have at least 4 lines");
        }
        String[] mini = new String[4];
        for (int at = 0; at < 4; at++) {
            mini[at] = lines[at] == null ? "" : Text.mini(lines[at]);
        }
        signText(location, List.of(mini), color, hasGlowingText);
    }

    @Override
    public void sendSignChange(Location location, List<? extends Component> lines, DyeColor color,
            boolean hasGlowingText) {
        if (lines != null && lines.size() < 4) {
            throw new IllegalArgumentException("Must have at least 4 lines");
        }
        signText(location, padded(lines), color, hasGlowingText);
    }

    @Override
    public void openSign(org.bukkit.block.Sign sign, Side side) {
        Location at = sign.getLocation();
        PlayerCalls.call(key(), "open_sign", (long) at.getBlockX(), (long) at.getBlockY(), (long) at.getBlockZ(),
            side == Side.FRONT);
    }

    @Override
    public void openVirtualSign(Position position, Side side) {
        Location at = new Location(getWorld(), position.blockX(), position.blockY(), position.blockZ());
        PlayerCalls.call(key(), "open_sign", (long) at.getBlockX(), (long) at.getBlockY(), (long) at.getBlockZ(),
            side == Side.FRONT);
    }

    @Override
    public void sendMap(org.bukkit.map.MapView map) {
        ((org.basaltmc.bukkit.misc.BasaltMapView) map).sendTo(this);
    }

    @Override
    public WorldBorder getWorldBorder() {
        return border;
    }

    @Override
    public void setWorldBorder(WorldBorder worldBorder) {
        border = worldBorder;
        if (worldBorder == null) {
            PlayerCalls.call(key(), "border_reset");
            return;
        }
        Location center = worldBorder.getCenter();
        PlayerCalls.call(key(), "border_set", center.getX(), center.getZ(), worldBorder.getSize(),
            (long) worldBorder.getWarningDistance(), (long) worldBorder.getWarningTime());
    }

    @Override
    public void lookAt(double x, double y, double z, io.papermc.paper.entity.LookAnchor anchor) {
        PlayerCalls.call(key(), "look_at", x, y, z, anchor == io.papermc.paper.entity.LookAnchor.EYES);
    }

    @Override
    public void lookAt(Entity entity, io.papermc.paper.entity.LookAnchor playerAnchor,
            io.papermc.paper.entity.LookAnchor entityAnchor) {
        Location at = entityAnchor == io.papermc.paper.entity.LookAnchor.EYES && entity instanceof org.bukkit.entity.LivingEntity living
            ? living.getEyeLocation() : entity.getLocation();
        lookAt(at.getX(), at.getY(), at.getZ(), playerAnchor);
    }

    @Override
    public void setRotation(io.papermc.paper.math.Angle yaw, io.papermc.paper.math.Angle pitch) {
        Location here = getLocation();
        here.setYaw(yaw.degrees());
        here.setPitch(pitch.degrees());
        teleport(here);
    }
}
