// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.server.rcon.thread;
import java.io.IOException;
import java.net.DatagramPacket;
import java.net.DatagramSocket;
import java.net.InetAddress;
import java.net.SocketAddress;
import java.net.SocketTimeoutException;
import java.nio.charset.StandardCharsets;
import java.util.HashMap;
import java.util.Map;
import java.util.Random;
import net.minecraft.server.ServerInterface;
import net.minecraft.server.rcon.NetworkDataOutputStream;
import net.minecraft.server.rcon.PktUtils;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
public class QueryThreadGs4 extends GenericThread {
    private static final Logger kLog = LoggerFactory.getLogger(QueryThreadGs4.class);
    private static final byte kHandshake = 9;
    private static final byte kStat = 0;
    private static final long kChallengeLifetimeMs = 30000L;
    private final ServerInterface serverInterface;
    private final int port;
    private final int serverPort;
    private final String hostname;
    private final Map<SocketAddress, RequestChallenge> validChallenges = new HashMap<>();
    private final byte[] inbound = new byte[PktUtils.MAX_PACKET_SIZE];
    private DatagramSocket socket;
    private long lastChallengeCheck;
    private QueryThreadGs4(ServerInterface serverInterface, int port) {
        super("Query Listener");
        this.serverInterface = serverInterface;
        this.port = port;
        this.serverPort = serverInterface.getServerPort();
        String ip = serverInterface.getServerIp();
        this.hostname = ip == null || ip.isEmpty() ? "0.0.0.0" : ip;
    }
    public static QueryThreadGs4 create(ServerInterface serverInterface) {
        int port = serverInterface.getProperties().queryPort;
        if (port < 1 || port > 65535) {
            kLog.warn("Invalid query port {} found in server.properties (queries disabled)", port);
            return null;
        }
        return new QueryThreadGs4(serverInterface, port);
    }
    private void send(byte[] data, DatagramPacket request) throws IOException {
        socket.send(new DatagramPacket(data, data.length, request.getSocketAddress()));
    }
    private boolean processPacket(DatagramPacket request) throws IOException {
        byte[] data = request.getData();
        int length = request.getLength();
        if (length < 7 || (data[0] & 0xFF) != 0xFE || (data[1] & 0xFF) != 0xFD) {
            return false;
        }
        kLog.debug("Packet len {} [{}]", length, request.getSocketAddress());
        switch (data[2]) {
            case kHandshake -> {
                send(handshakeReply(request), request);
                return true;
            }
            case kStat -> {
                if (!validChallenge(request)) {
                    return false;
                }
                send(length == 15 ? fullStat(request) : basicStat(request), request);
                return true;
            }
            default -> {
                return false;
            }
        }
    }
    private byte[] handshakeReply(DatagramPacket request) throws IOException {
        RequestChallenge challenge = new RequestChallenge(request);
        validChallenges.put(request.getSocketAddress(), challenge);
        NetworkDataOutputStream out = new NetworkDataOutputStream(PktUtils.MAX_PACKET_SIZE);
        out.write(kHandshake);
        out.writeBytes(challenge.getIdentBytes());
        out.writeString(Integer.toString(challenge.getChallenge()));
        return out.toByteArray();
    }
    private boolean validChallenge(DatagramPacket request) {
        byte[] data = request.getData();
        RequestChallenge challenge = validChallenges.get(request.getSocketAddress());
        if (challenge == null) {
            return false;
        }
        return PktUtils.intFromNetworkByteArray(data, 7, request.getLength()) == challenge.getChallenge();
    }
    private byte[] basicStat(DatagramPacket request) throws IOException {
        NetworkDataOutputStream out = new NetworkDataOutputStream(PktUtils.MAX_PACKET_SIZE);
        out.write(kStat);
        out.writeBytes(identOf(request));
        out.writeString(serverInterface.getMotd());
        out.writeString("SMP");
        out.writeString(serverInterface.getLevelIdName());
        out.writeString(Integer.toString(serverInterface.getPlayerCount()));
        out.writeString(Integer.toString(serverInterface.getMaxPlayers()));
        out.writeShort((short) serverPort);
        out.writeString(hostname);
        return out.toByteArray();
    }
    private byte[] fullStat(DatagramPacket request) throws IOException {
        NetworkDataOutputStream out = new NetworkDataOutputStream(PktUtils.MAX_PACKET_SIZE);
        out.write(kStat);
        out.writeBytes(identOf(request));
        out.writeString("splitnum");
        out.write(128);
        out.write(0);
        Map<String, String> info = new java.util.LinkedHashMap<>();
        info.put("hostname", serverInterface.getMotd());
        info.put("gametype", "SMP");
        info.put("game_id", "MINECRAFT");
        info.put("version", serverInterface.getServerVersion());
        info.put("plugins", serverInterface.getPluginNames());
        info.put("map", serverInterface.getLevelIdName());
        info.put("numplayers", Integer.toString(serverInterface.getPlayerCount()));
        info.put("maxplayers", Integer.toString(serverInterface.getMaxPlayers()));
        info.put("hostport", Integer.toString(serverPort));
        info.put("hostip", hostname);
        for (Map.Entry<String, String> entry : info.entrySet()) {
            out.writeString(entry.getKey());
            out.writeString(entry.getValue());
        }
        out.write(0);
        out.write(1);
        out.writeString("player_");
        out.write(0);
        for (String name : serverInterface.getPlayerNames()) {
            out.writeString(name);
        }
        out.write(0);
        return out.toByteArray();
    }
    private static byte[] identOf(DatagramPacket request) {
        byte[] data = request.getData();
        return new byte[] {data[3], data[4], data[5], data[6]};
    }
    private void expireChallenges() {
        if (!running) {
            return;
        }
        long now = System.currentTimeMillis();
        if (now < lastChallengeCheck + 30000L) {
            return;
        }
        lastChallengeCheck = now;
        validChallenges.values().removeIf(challenge -> challenge.before(now));
    }
    public void run() {
        kLog.info("Query running on {}:{}", hostname, port);
        lastChallengeCheck = System.currentTimeMillis();
        DatagramPacket packet = new DatagramPacket(inbound, inbound.length);
        try {
            while (running) {
                try {
                    socket.receive(packet);
                    expireChallenges();
                    processPacket(packet);
                } catch (SocketTimeoutException idle) {
                    expireChallenges();
                } catch (java.net.PortUnreachableException ignored) {
                    // client left before the reply
                } catch (IOException failure) {
                    if (running) {
                        kLog.warn("Query socket failed", failure);
                    }
                }
            }
        } finally {
            kLog.debug("closeSocket: {}:{}", hostname, port);
            socket.close();
        }
    }
    public boolean start() {
        if (running) {
            return true;
        }
        try {
            socket = new DatagramSocket(port, InetAddress.getByName(hostname));
            socket.setSoTimeout(500);
            return super.start();
        } catch (IOException failure) {
            kLog.warn("Unable to initialise query system on {}:{}", hostname, port, failure);
            return false;
        }
    }
    static class RequestChallenge {
        private final long time = System.currentTimeMillis();
        private final int challenge;
        private final byte[] identBytes;
        private final byte[] challengeBytes;
        private final String ident;
        public RequestChallenge(DatagramPacket src) {
            byte[] data = src.getData();
            identBytes = new byte[] {data[3], data[4], data[5], data[6]};
            ident = new String(identBytes, StandardCharsets.UTF_8);
            challenge = new Random().nextInt(16777216);
            challengeBytes = String.format("\t%s%d\u0000", ident, challenge).getBytes(StandardCharsets.UTF_8);
        }
        public Boolean before(long time) {
            return this.time < time - kChallengeLifetimeMs;
        }
        public int getChallenge() {
            return challenge;
        }
        public byte[] getChallengeBytes() {
            return challengeBytes;
        }
        public byte[] getIdentBytes() {
            return identBytes;
        }
        public String getIdent() {
            return ident;
        }
    }
}
