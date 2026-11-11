// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.server.chase;
import java.io.BufferedReader;
import java.io.IOException;
import java.io.InputStreamReader;
import java.net.Socket;
import java.nio.charset.StandardCharsets;
import java.util.Locale;
import java.util.Optional;
import java.util.Set;
import net.minecraft.resources.ResourceKey;
import net.minecraft.server.MinecraftServer;
import net.minecraft.server.commands.ChaseCommand;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.server.level.ServerPlayer;
import net.minecraft.world.level.Level;
import net.minecraft.world.phys.Vec2;
import net.minecraft.world.phys.Vec3;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
public class ChaseClient {
    private static final Logger kLog = LoggerFactory.getLogger(ChaseClient.class);
    private static final int kRetryDelayMs = 5000;
    private final String host;
    private final int port;
    private final MinecraftServer server;
    private volatile boolean running;
    private volatile Socket socket;
    private Thread thread;
    public ChaseClient(String serverHost, int serverPort, MinecraftServer server) {
        this.host = serverHost;
        this.port = serverPort;
        this.server = server;
    }
    public void start() {
        if (thread != null && thread.isAlive()) {
            kLog.warn("Remote control client was asked to start, but it is already running. Will ignore.");
            return;
        }
        running = true;
        thread = new Thread(this::run, "chase-client");
        thread.setDaemon(true);
        thread.start();
    }
    public void stop() {
        running = false;
        Socket open = socket;
        socket = null;
        try {
            if (open != null) {
                open.close();
            }
        } catch (IOException ignored) {
            // already closed
        }
    }
    public void run() {
        while (running) {
            try (Socket connected = new Socket(host, port)) {
                socket = connected;
                kLog.info("Connected to remote control server at {}:{}", host, port);
                BufferedReader lines = new BufferedReader(new InputStreamReader(connected.getInputStream(), StandardCharsets.US_ASCII));
                for (String line = lines.readLine(); running && line != null; line = lines.readLine()) {
                    parse(line).ifPresent(this::teleport);
                }
            } catch (IOException failure) {
                if (running) {
                    kLog.warn("Lost connection to remote control server, retrying", failure);
                }
            }
            try {
                Thread.sleep(kRetryDelayMs);
            } catch (InterruptedException ignored) {
                return;
            }
        }
    }
    private static Optional<TeleportTarget> parse(String line) {
        String[] parts = line.trim().toLowerCase(Locale.ROOT).split(" ");
        if (parts.length != 7 || !parts[0].equals("t")) {
            return Optional.empty();
        }
        ResourceKey<Level> level = ChaseCommand.DIMENSION_NAMES.get(parts[1]);
        if (level == null) {
            return Optional.empty();
        }
        try {
            Vec3 position = new Vec3(Double.parseDouble(parts[2]), Double.parseDouble(parts[3]), Double.parseDouble(parts[4]));
            Vec2 rotation = new Vec2(Float.parseFloat(parts[6]), Float.parseFloat(parts[5]));
            return Optional.of(new TeleportTarget(level, position, rotation));
        } catch (NumberFormatException malformed) {
            return Optional.empty();
        }
    }
    private void teleport(TeleportTarget target) {
        server.execute(() -> {
            ServerLevel level = server.getLevel(target.level());
            if (level == null) {
                return;
            }
            for (ServerPlayer player : server.getPlayerList().getPlayers()) {
                player.teleportTo(level, target.pos().x, target.pos().y, target.pos().z, Set.of(), target.rot().y, target.rot().x, true);
            }
        });
    }
    static record TeleportTarget(ResourceKey<Level> level, Vec3 pos, Vec2 rot) {
    }
}
