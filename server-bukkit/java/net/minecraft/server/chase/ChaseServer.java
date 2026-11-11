// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.server.chase;
import java.io.IOException;
import java.io.OutputStream;
import java.net.InetAddress;
import java.net.ServerSocket;
import java.net.Socket;
import java.nio.charset.StandardCharsets;
import java.util.List;
import java.util.Locale;
import java.util.concurrent.CopyOnWriteArrayList;
import net.minecraft.server.commands.ChaseCommand;
import net.minecraft.server.level.ServerPlayer;
import net.minecraft.server.players.PlayerList;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
public class ChaseServer {
    private static final Logger kLog = LoggerFactory.getLogger(ChaseServer.class);
    private final String bindAddress;
    private final int port;
    private final PlayerList playerList;
    private final int intervalMs;
    private final List<Socket> clients = new CopyOnWriteArrayList<>();
    private volatile boolean running;
    private volatile ServerSocket listener;
    public ChaseServer(String serverBindAddress, int serverPort, PlayerList playerList, int broadcastIntervalMs) {
        this.bindAddress = serverBindAddress;
        this.port = serverPort;
        this.playerList = playerList;
        this.intervalMs = broadcastIntervalMs;
    }
    public void start() throws IOException {
        if (listener != null && !listener.isClosed()) {
            kLog.warn("Remote control server was asked to start, but it is already running. Will ignore.");
            return;
        }
        running = true;
        listener = new ServerSocket(port, 50, InetAddress.getByName(bindAddress));
        spawn("chase-server-acceptor", this::acceptLoop);
        spawn("chase-server-sender", this::sendLoop);
    }
    private static void spawn(String name, Runnable body) {
        Thread thread = new Thread(body, name);
        thread.setDaemon(true);
        thread.start();
    }
    private void acceptLoop() {
        ServerSocket socket = listener;
        try {
            while (running && socket != null) {
                kLog.info("Remote control server is listening for connections on port {}", port);
                Socket accepted = socket.accept();
                kLog.info("Remote control server received client connection on port {}", accepted.getPort());
                clients.add(accepted);
            }
        } catch (IOException failure) {
            if (running) {
                kLog.error("Remote control server closed because of an IO exception", failure);
            }
        } finally {
            closeQuietly(socket);
        }
        kLog.info("Remote control server is now stopped");
        running = false;
    }
    private void sendLoop() {
        PlayerPosition last = null;
        while (running) {
            PlayerPosition now = clients.isEmpty() ? null : currentPosition();
            if (now != null && !now.equals(last)) {
                last = now;
                byte[] line = now.format().getBytes(StandardCharsets.US_ASCII);
                for (Socket client : clients) {
                    push(client, line);
                }
            }
            clients.removeIf(Socket::isClosed);
            try {
                Thread.sleep(intervalMs);
            } catch (InterruptedException ignored) {
                // wake early on stop
            }
        }
    }
    private static void push(Socket client, byte[] line) {
        if (client.isClosed()) {
            return;
        }
        try {
            OutputStream out = client.getOutputStream();
            out.write(line);
            out.flush();
        } catch (IOException failure) {
            kLog.info("Remote control client socket got an IO exception and will be closed", failure);
            closeQuietly(client);
        }
    }
    public void stop() {
        running = false;
        closeQuietly(listener);
        listener = null;
    }
    private static void closeQuietly(java.io.Closeable target) {
        try {
            if (target != null) {
                target.close();
            }
        } catch (IOException ignored) {
            // already closed
        }
    }
    private PlayerPosition currentPosition() {
        List<ServerPlayer> players = playerList.getPlayers();
        if (players.isEmpty()) {
            return null;
        }
        ServerPlayer first = players.get(0);
        String dimension = ChaseCommand.DIMENSION_NAMES.inverse().get(first.level().dimension());
        return dimension == null ? null : new PlayerPosition(dimension, first.getX(), first.getY(), first.getZ(), first.getYRot(), first.getXRot());
    }
    static record PlayerPosition(String dimensionName, double x, double y, double z, float yRot, float xRot) {
        String format() {
            return String.format(Locale.ROOT, "t %s %.2f %.2f %.2f %.2f %.2f\n", dimensionName, x, y, z, yRot, xRot);
        }
    }
}
