// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.server.rcon.thread;
import java.io.BufferedInputStream;
import java.io.IOException;
import java.net.Socket;
import java.net.SocketTimeoutException;
import java.nio.charset.StandardCharsets;
import net.minecraft.server.ServerInterface;
import net.minecraft.server.rcon.NetworkDataOutputStream;
import net.minecraft.server.rcon.PktUtils;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
public class RconClient extends GenericThread {
    private static final Logger kLog = LoggerFactory.getLogger(RconClient.class);
    private static final int kServerdata = 0;
    private static final int kCommand = 2;
    private static final int kLogin = 3;
    private static final int kMaxPayload = 4096;
    private boolean authed;
    private final Socket client;
    private final byte[] buf = new byte[kMaxPayload + 6];
    private final String password;
    private final ServerInterface serverInterface;
    public RconClient(ServerInterface serverInterface, String rconPassword, Socket socket) {
        super("RCON Client " + socket.getInetAddress());
        this.serverInterface = serverInterface;
        this.client = socket;
        try {
            socket.setSoTimeout(0);
        } catch (java.net.SocketException ignored) {
            running = false;
        }
        this.password = rconPassword;
    }
    public void run() {
        try {
            while (running) {
                BufferedInputStream in = new BufferedInputStream(client.getInputStream());
                int read = in.read(buf, 0, buf.length);
                if (read < 0) {
                    return;
                }
                if (read < 10) {
                    continue;
                }
                int length = PktUtils.intFromByteArray(buf, 0, read);
                if (length != read - 4) {
                    return;
                }
                int requestId = PktUtils.intFromByteArray(buf, 4, read);
                int type = PktUtils.intFromByteArray(buf, 8, read);
                String payload = PktUtils.stringFromByteArray(buf, 12, read);
                if (type == kLogin) {
                    handleLogin(requestId, payload);
                } else if (type == kCommand && authed) {
                    handleCommand(requestId, payload);
                } else if (!authed) {
                    sendFailure();
                } else {
                    send(requestId, kServerdata, String.format("Unknown request %s", Integer.toHexString(type)));
                }
            }
        } catch (SocketTimeoutException | java.net.SocketException | java.io.EOFException ignored) {
            // peer went away
        } catch (IOException failure) {
            kLog.error("Exception whilst parsing RCON input", failure);
        } finally {
            closeSocket();
            kLog.info("Thread {} shutting down", name);
            running = false;
        }
    }
    private void handleLogin(int requestId, String payload) throws IOException {
        if (password.equals(payload)) {
            authed = true;
            send(requestId, kCommand, "");
        } else {
            authed = false;
            sendFailure();
        }
    }
    private void handleCommand(int requestId, String payload) throws IOException {
        String response;
        try {
            response = serverInterface.runCommand(payload);
        } catch (RuntimeException failure) {
            kLog.warn("RCON command failed", failure);
            response = "Command failed";
        }
        sendSplit(requestId, response == null ? "" : response);
    }
    private void sendSplit(int requestId, String text) throws IOException {
        byte[] bytes = text.getBytes(StandardCharsets.UTF_8);
        if (bytes.length <= kMaxPayload) {
            send(requestId, kServerdata, text);
            return;
        }
        for (int from = 0; from < bytes.length; from += kMaxPayload) {
            int to = Math.min(from + kMaxPayload, bytes.length);
            send(requestId, kServerdata, new String(bytes, from, to - from, StandardCharsets.UTF_8));
        }
    }
    private void sendFailure() throws IOException {
        send(-1, kCommand, "");
    }
    private void send(int requestId, int type, String text) throws IOException {
        byte[] body = text.getBytes(StandardCharsets.UTF_8);
        NetworkDataOutputStream out = new NetworkDataOutputStream(body.length + 10);
        out.writeInt(body.length + 10);
        out.writeInt(requestId);
        out.writeInt(type);
        out.writeBytes(body);
        out.write(0);
        out.write(0);
        client.getOutputStream().write(out.toByteArray());
    }
    public void stop() {
        running = false;
        closeSocket();
        super.stop();
    }
    private void closeSocket() {
        try {
            client.close();
        } catch (IOException ignored) {
            // already closed
        }
    }
}
