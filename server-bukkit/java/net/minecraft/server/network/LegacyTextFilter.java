// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.server.network;

import com.google.gson.JsonArray;
import com.google.gson.JsonObject;
import com.mojang.authlib.GameProfile;
import java.io.InputStream;
import java.net.HttpURLConnection;
import java.net.URI;
import java.net.URL;
import java.nio.charset.StandardCharsets;
import java.util.Base64;
import java.util.concurrent.Executor;
import java.util.concurrent.ExecutorService;
import net.minecraft.network.chat.FilterMask;
import net.minecraft.util.GsonHelper;

public class LegacyTextFilter extends ServerTextFilter {
    private final URL joinEndpoint;
    private final JoinOrLeaveEncoder joinEncoder;
    private final URL leaveEndpoint;
    private final JoinOrLeaveEncoder leaveEncoder;
    private final String authKey;

    private LegacyTextFilter(URL chatEndpoint, ServerTextFilter.MessageEncoder chatEncoder, URL joinEndpoint, JoinOrLeaveEncoder joinEncoder,
        URL leaveEndpoint, JoinOrLeaveEncoder leaveEncoder, String authKey, ServerTextFilter.IgnoreStrategy chatIgnoreStrategy, ExecutorService workerPool) {
        super(chatEndpoint, chatEncoder, chatIgnoreStrategy, workerPool);
        this.joinEndpoint = joinEndpoint;
        this.joinEncoder = joinEncoder;
        this.leaveEndpoint = leaveEndpoint;
        this.leaveEncoder = leaveEncoder;
        this.authKey = authKey;
    }

    public static ServerTextFilter createTextFilterFromConfig(String config) {
        try {
            JsonObject json = GsonHelper.parse(config);
            URI host = new URI(GsonHelper.getAsString(json, "apiServer"));
            String key = GsonHelper.getAsString(json, "apiKey");
            if (key.isEmpty()) {
                throw new IllegalArgumentException("Missing API key");
            }
            int ruleId = GsonHelper.getAsInt(json, "ruleId", 1);
            String serverId = GsonHelper.getAsString(json, "serverId", "");
            String roomId = GsonHelper.getAsString(json, "roomId", "Java:Chat");
            int hashesToDrop = GsonHelper.getAsInt(json, "hashesToDrop", -1);
            int concurrency = GsonHelper.getAsInt(json, "maxConcurrentRequests", 7);
            JsonObject endpoints = GsonHelper.getAsJsonObject(json, "endpoints", null);
            String chatPath = getEndpointFromConfig(endpoints, "chat", "v1/chat");
            JoinOrLeaveEncoder presence = user -> {
                JsonObject body = new JsonObject();
                body.addProperty("server", serverId);
                body.addProperty("room", roomId);
                body.addProperty("user_id", user.id().toString());
                body.addProperty("user_display_name", user.name());
                return body;
            };
            ServerTextFilter.MessageEncoder chat = chatPath.equals("v1/chat")
                ? (sender, text) -> chatBody(sender, text, "rule", ruleId, "server", serverId, "room", roomId, "player", "player_display_name")
                : (sender, text) -> chatBody(sender, text, "rule_id", String.valueOf(ruleId), "category", serverId, "subcategory", roomId,
                    "user_id", "user_display_name");
            return new LegacyTextFilter(host.resolve("/" + chatPath).toURL(), chat, getEndpoint(host, endpoints, "join", "v1/join"), presence,
                getEndpoint(host, endpoints, "leave", "v1/leave"), presence,
                Base64.getEncoder().encodeToString(key.getBytes(StandardCharsets.US_ASCII)),
                ServerTextFilter.IgnoreStrategy.select(hashesToDrop), createWorkerPool(concurrency));
        } catch (Exception failure) {
            LOGGER.warn("Failed to parse chat filter config {}", config, failure);
            return null;
        }
    }

    private static JsonObject chatBody(GameProfile sender, String text, String ruleKey, Object rule, String serverKey, String serverId,
        String roomKey, String roomId, String playerKey, String nameKey) {
        JsonObject body = new JsonObject();
        if (rule instanceof Number number) {
            body.addProperty(ruleKey, number);
        } else {
            body.addProperty(ruleKey, String.valueOf(rule));
        }
        body.addProperty(serverKey, serverId);
        body.addProperty(roomKey, roomId);
        body.addProperty(playerKey, sender.id().toString());
        body.addProperty(nameKey, sender.name());
        body.addProperty("text", text);
        body.addProperty("language", "*");
        return body;
    }

    public TextFilter createContext(GameProfile gameProfile) {
        return new ServerTextFilter.PlayerContext(gameProfile) {
            public void join() {
                presence(profile, joinEndpoint, joinEncoder, streamExecutor);
            }

            public void leave() {
                presence(profile, leaveEndpoint, leaveEncoder, streamExecutor);
            }
        };
    }

    private void presence(GameProfile user, URL endpoint, JoinOrLeaveEncoder encoder, Executor executor) {
        executor.execute(() -> {
            try {
                HttpURLConnection connection = makeRequest(encoder.encode(user), endpoint);
                try (InputStream body = connection.getInputStream()) {
                    drainStream(body);
                }
            } catch (Exception failure) {
                LOGGER.warn("Failed to send join/leave packet to {} for player {}", endpoint, user, failure);
            }
        });
    }

    protected void setAuthorizationProperty(HttpURLConnection connection) {
        connection.setRequestProperty("Authorization", "Basic " + authKey);
    }

    protected FilteredText filterText(String message, ServerTextFilter.IgnoreStrategy ignoreStrategy, JsonObject result) {
        if (GsonHelper.getAsBoolean(result, "response", false)) {
            return FilteredText.passThrough(message);
        }
        if (GsonHelper.getAsString(result, "hashed", null) == null) {
            return FilteredText.fullyFiltered(message);
        }
        JsonArray removed = GsonHelper.getAsJsonArray(result, "hashes");
        FilterMask mask = parseMask(message, removed, ignoreStrategy);
        return new FilteredText(message, mask);
    }

    interface JoinOrLeaveEncoder {
        JsonObject encode(GameProfile profile);
    }
}
