// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.clock;
import java.util.stream.Stream;
import net.minecraft.core.Holder;
import net.minecraft.network.protocol.game.ClientboundSetTimePacket;
import net.minecraft.resources.ResourceKey;
import net.minecraft.server.MinecraftServer;
import net.minecraft.world.level.saveddata.SavedData;
import net.minecraft.world.level.saveddata.SavedDataType;
public class ServerClockManager extends SavedData implements ClockManager {
    public static final SavedDataType<ServerClockManager> TYPE = new SavedDataType<>(net.minecraft.resources.Identifier.withDefaultNamespace("world_clocks"), ServerClockManager::new, com.mojang.serialization.MapCodec.unitCodec(ServerClockManager::new), net.minecraft.util.datafix.DataFixTypes.SAVED_DATA_WORLD_CLOCKS);
    private ServerClockManager(PackedClockStates packedClockStates) {
    }

    public ServerClockManager() {
    }

    private static final ResourceKey<net.minecraft.core.Registry<ClockTimeMarker>> kMarkers = ResourceKey.createRegistryKey(net.minecraft.resources.Identifier.withDefaultNamespace("time_marker"));

    private static org.bukkit.World world() {
        return org.bukkit.Bukkit.getWorlds().get(0);
    }

    private static String[] clockRows() {
        return org.basaltmc.bukkit.WorldEnvNatives.clock(world().getName());
    }

    private static int markerTicks(ResourceKey<ClockTimeMarker> id) {
        for (String row : clockRows()) {
            if (row.startsWith(id.identifier().getPath() + "=")) {
                return Integer.parseInt(row.substring(row.indexOf('=') + 1));
            }
        }
        return -1;
    }
    public void init(MinecraftServer server) {
    }
    public PackedClockStates packState() {
        return null;
    }
    public void tick() {
    }
    public ServerClockInstance getInstance(Holder<WorldClock> definition) {
        return new ServerClockInstance();
    }
    public void setTotalTicks(Holder<WorldClock> clock, long totalTicks) {
        world().setFullTime(totalTicks);
    }
    public MoveResult moveToTimeMarker(Holder<WorldClock> clock, ResourceKey<ClockTimeMarker> timeMarkerId) {
        return switch (org.basaltmc.bukkit.WorldEnvNatives.moveToTimeMarker(world().getName(), timeMarkerId.identifier().getPath())) {
            case 2 -> MoveResult.MOVED;
            case 1 -> MoveResult.NOT_MOVED;
            default -> MoveResult.NO_TIME_MARKER_FOUND;
        };
    }
    public void addTicks(Holder<WorldClock> clock, int ticks) {
        world().setFullTime(world().getFullTime() + ticks);
    }
    public void setPaused(Holder<WorldClock> clock, boolean paused) {
        world().setGameRule(org.bukkit.GameRule.DO_DAYLIGHT_CYCLE, !paused);
    }
    public void setRate(Holder<WorldClock> clock, float rate) {
        org.basaltmc.bukkit.WorldEnvNatives.setClockRate(world().getName(), rate);
    }
    public ClientboundSetTimePacket createFullSyncPacket() {
        return null;
    }
    public boolean isAtTimeMarker(Holder<WorldClock> clock, ResourceKey<ClockTimeMarker> timeMarkerId) {
        int ticks = markerTicks(timeMarkerId);
        return ticks >= 0 && Math.floorMod(world().getFullTime(), 24000L) == ticks;
    }
    public Stream<ResourceKey<ClockTimeMarker>> commandTimeMarkersForClock(Holder<WorldClock> clock) {
        return java.util.Arrays.stream(clockRows()).skip(3).map(row -> ResourceKey.create(kMarkers, net.minecraft.resources.Identifier.withDefaultNamespace(row.substring(0, row.indexOf('=')))));
    }
    public static enum MoveResult {
        NO_TIME_MARKER_FOUND, NOT_MOVED, MOVED;
    }
    public static class ServerClockInstance implements ClockInstance {
        private static org.bukkit.World world() {
            return org.bukkit.Bukkit.getWorlds().get(0);
        }

        public void loadFrom(ClockState state) {
        }
        public void tick() {
        }
        public long totalTicks() {
        return world().getFullTime();
        }
        public float partialTick() {
        return 0.0F;
        }
        public float rate() {
            return Float.parseFloat(clockRows()[1]);
        }
        public boolean isPaused() {
            return clockRows()[2].equals("1") || !Boolean.TRUE.equals(world().getGameRuleValue(org.bukkit.GameRule.DO_DAYLIGHT_CYCLE));
        }
        public ClockState packState() {
        return null;
        }
        public ClockNetworkState packNetworkState(MinecraftServer server) {
        return null;
        }
    }
}
