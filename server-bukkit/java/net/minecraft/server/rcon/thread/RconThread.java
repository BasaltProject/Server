// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.server.rcon.thread;
import java.io.IOException;
import java.net.InetAddress;
import java.net.ServerSocket;
import java.net.Socket;
import java.net.SocketTimeoutException;
import java.util.ArrayList;
import java.util.List;
import net.minecraft.server.ServerInterface;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
public class RconThread extends GenericThread {
    private static final Logger kLog = LoggerFactory.getLogger(RconThread.class);
    private final ServerSocket socket;
    private final String password;
    private final List<RconClient> clients = new ArrayList<>();
    private final ServerInterface serverInterface;
    private RconThread(ServerInterface serverInterface, ServerSocket socket, String rconPassword) {
        super("RCON Listener");
        this.serverInterface = serverInterface;
        this.socket = socket;
        this.password = rconPassword;
    }
    private void dropFinished() {
        clients.removeIf(client -> !client.isRunning());
    }
    public void run() {
        try {
            while (running) {
                try {
                    Socket accepted = socket.accept();
                    RconClient client = new RconClient(serverInterface, password, accepted);
                    client.start();
                    clients.add(client);
                    dropFinished();
                } catch (SocketTimeoutException idle) {
                    dropFinished();
                } catch (IOException failure) {
                    if (running) {
                        kLog.info("IO exception: ", failure);
                    }
                }
            }
        } finally {
            closeSocket(socket);
        }
    }
    public static RconThread create(ServerInterface serverInterface) {
        var properties = serverInterface.getProperties();
        String ip = serverInterface.getServerIp();
        if (ip == null || ip.isEmpty()) {
            ip = "0.0.0.0";
        }
        int port = properties.rconPort;
        if (port < 1 || port > 65535) {
            kLog.warn("Invalid rcon port: {}, rcon disabled!", port);
            return null;
        }
        String password = properties.rconPassword;
        if (password.isEmpty()) {
            kLog.warn("No rcon password set in server.properties, rcon disabled!");
            return null;
        }
        try {
            ServerSocket bound = new ServerSocket(port, 0, InetAddress.getByName(ip));
            bound.setSoTimeout(500);
            return new RconThread(serverInterface, bound, password);
        } catch (IOException failure) {
            kLog.warn("Unable to initialise RCON on {}:{}", ip, port, failure);
            return null;
        }
    }
    public void stop() {
        running = false;
        closeSocket(socket);
        super.stop();
        for (RconClient client : clients) {
            if (client.isRunning()) {
                client.stop();
            }
        }
        clients.clear();
    }
    static void closeSocket(ServerSocket toClose) {
        try {
            toClose.close();
        } catch (IOException ignored) {
            // already closed
        }
    }
}
