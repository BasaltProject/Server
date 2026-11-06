// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.saveddata.maps;
import com.mojang.serialization.Codec;
import io.netty.buffer.ByteBuf;
import java.nio.ByteBuffer;
import java.util.Collection;
import java.util.List;
import java.util.Optional;
import net.minecraft.core.BlockPos;
import net.minecraft.core.Holder;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.network.protocol.Packet;
import net.minecraft.resources.ResourceKey;
import net.minecraft.world.entity.decoration.ItemFrame;
import net.minecraft.world.entity.player.Player;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.level.BlockGetter;
import net.minecraft.world.level.Level;
import net.minecraft.world.level.LevelAccessor;
import net.minecraft.world.level.saveddata.SavedData;
import net.minecraft.world.level.saveddata.SavedDataType;
public class MapItemSavedData extends SavedData {
    public static final int MAX_SCALE = 4;
    public static final int TRACKED_DECORATION_LIMIT = 256;
    public static final Codec<MapItemSavedData> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.level.saveddata.maps.MapItemSavedData.class, "CODEC");
    public final int centerX;
    public final int centerZ;
    public final ResourceKey<Level> dimension;
    public final byte scale;
    public byte[] colors;
    public final boolean locked;
    public static SavedDataType<MapItemSavedData> type(MapId id) {
        return null;
    }
    private MapItemSavedData(int centerX, int centerZ, byte scale, boolean trackingPosition, boolean unlimitedTracking, boolean locked, ResourceKey<Level> dimension) {
        this.centerX = centerX;
        this.centerZ = centerZ;
        this.scale = scale;
        this.locked = locked;
        this.dimension = dimension;
        this.colors = new byte[16384];
    }
    public static MapItemSavedData createFresh(double originX, double originY, byte scale, boolean trackingPosition, boolean unlimitedTracking, ResourceKey<Level> dimension) {
        int size = 128 * (1 << scale);
        int cx = (int) Math.floor((originX + 64.0) / size);
        int cz = (int) Math.floor((originY + 64.0) / size);
        return new MapItemSavedData(cx * size + size / 2 - 64, cz * size + size / 2 - 64, scale, trackingPosition, unlimitedTracking, false, dimension);
    }
    public static MapItemSavedData createForClient(byte scale, boolean isLocked, ResourceKey<Level> dimension) {
        return new MapItemSavedData(0, 0, scale, false, false, isLocked, dimension);
    }
    public MapItemSavedData locked() {
        MapItemSavedData copy = new MapItemSavedData(centerX, centerZ, scale, false, false, true, dimension);
        copy.colors = colors.clone();
        return copy;
    }
    public MapItemSavedData scaled() {
        MapItemSavedData copy = new MapItemSavedData(centerX, centerZ, (byte) Math.min(scale + 1, MAX_SCALE), false, false, locked, dimension);
        copy.colors = colors.clone();
        return copy;
    }
    public void tickCarriedBy(Player tickingPlayer, ItemStack itemStack, ItemFrame placedInFrame) {
    }
    public static void addTargetDecoration(ItemStack itemStack, BlockPos position, String key, Holder<MapDecorationType> decorationType) {
    }
    public Packet<?> getUpdatePacket(MapId id, Player player) {
        if (!holders.containsKey(player)) {
            return null;
        }
        byte[] current = org.basaltmc.bukkit.MiscNatives.mapColors(id.id());
        if (current != null && current.length == 16384) {
            colors = current;
        }
        return new net.minecraft.network.protocol.game.ClientboundMapItemDataPacket(id, scale, locked, List.of(),
            new MapPatch(0, 0, 128, 128, colors.clone()));
    }
    public HoldingPlayer getHoldingPlayer(Player player) {
        return holders.computeIfAbsent(player, HoldingPlayer::new);
    }
    public boolean toggleBanner(LevelAccessor level, BlockPos pos) {
        return false;
    }
    public void checkBanners(BlockGetter level, int x, int z) {
    }
    public Collection<MapBanner> getBanners() {
        return List.of();
    }
    public void removedFromFrame(BlockPos pos, int entityID) {
    }
    public boolean updateColor(int x, int y, byte newColor) {
        int at = x + y * 128;
        if (colors[at] == newColor) {
            return false;
        }
        colors[at] = newColor;
        return true;
    }
    public void setColor(int x, int y, byte newColor) {
        colors[x + y * 128] = newColor;
    }
    public void addClientSideDecorations(List<MapDecoration> decorations) {
    }
    public Iterable<MapDecoration> getDecorations() {
        return List.of();
    }
    public boolean isTrackedCountOverLimit(int limit) {
        return false;
    }
    public static record MapPatch(int startX, int startY, int width, int height, byte[] mapColors) {
        public static final StreamCodec<ByteBuf, Optional<MapPatch>> STREAM_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.streamCodec(net.minecraft.world.level.saveddata.maps.MapItemSavedData.MapPatch.class, "STREAM_CODEC");
        public void applyToMap(MapItemSavedData map) {
            for (int row = 0; row < height; row++) {
                for (int column = 0; column < width; column++) {
                    map.setColor(startX + column, startY + row, mapColors[column + row * width]);
                }
            }
        }
    }
    /** The map the native server holds under that id; null when there is none. */
    public static MapItemSavedData fromNative(int id) {
        String[] info = org.basaltmc.bukkit.MiscNatives.mapInfo(id);
        if (info == null) {
            return null;
        }
        ResourceKey<Level> dimension = ResourceKey.create(net.minecraft.core.registries.Registries.DIMENSION,
            net.minecraft.resources.Identifier.parse(info[0]));
        MapItemSavedData data = new MapItemSavedData(Integer.parseInt(info[2]), Integer.parseInt(info[3]), Byte.parseByte(info[1]),
            "1".equals(info[4]), "1".equals(info[5]), "1".equals(info[6]), dimension);
        byte[] colors = org.basaltmc.bukkit.MiscNatives.mapColors(id);
        if (colors != null && colors.length == 16384) {
            data.colors = colors;
        }
        return data;
    }
    private final java.util.Map<Player, HoldingPlayer> holders = new java.util.WeakHashMap<>();
    public class HoldingPlayer {
        public final Player player;
        public int step;
        private HoldingPlayer(Player player) {
            this.player = player;
        }
    }
    static record MapDecorationLocation(Holder<MapDecorationType> type, byte x, byte y, byte rot) {
    }
}
