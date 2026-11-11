// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.server.network;

import com.google.gson.JsonArray;
import com.google.gson.JsonObject;
import com.google.gson.stream.JsonWriter;
import com.mojang.authlib.GameProfile;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.io.OutputStreamWriter;
import java.net.HttpURLConnection;
import java.net.MalformedURLException;
import java.net.URI;
import java.net.URL;
import java.nio.charset.StandardCharsets;
import java.util.List;
import java.util.Map;
import java.util.concurrent.CompletableFuture;
import java.util.concurrent.Executor;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.atomic.AtomicInteger;
import net.minecraft.SharedConstants;
import net.minecraft.network.chat.FilterMask;
import net.minecraft.server.dedicated.DedicatedServerProperties;
import net.minecraft.util.GsonHelper;
import net.minecraft.util.LenientJsonParser;
import net.minecraft.util.StringUtil;
import net.minecraft.util.Util;
import net.minecraft.util.thread.ConsecutiveExecutor;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

public abstract class ServerTextFilter implements AutoCloseable {
    protected static final Logger LOGGER = LoggerFactory.getLogger(ServerTextFilter.class);
    private static final AtomicInteger WORKERS = new AtomicInteger(1);
    private static final int CONNECT_TIMEOUT_MS = 15000;
    private final URL chatEndpoint;
    private final MessageEncoder chatEncoder;
    private final IgnoreStrategy chatIgnoreStrategy;
    private final ExecutorService workerPool;

    protected static ExecutorService createWorkerPool(int maxConcurrentRequests) {
        return Executors.newFixedThreadPool(maxConcurrentRequests, task -> new Thread(task, "Chat-Filter-Worker-" + WORKERS.getAndIncrement()));
    }

    protected ServerTextFilter(URL chatEndpoint, MessageEncoder chatEncoder, IgnoreStrategy chatIgnoreStrategy, ExecutorService workerPool) {
        this.workerPool = workerPool;
        this.chatEndpoint = chatEndpoint;
        this.chatEncoder = chatEncoder;
        this.chatIgnoreStrategy = chatIgnoreStrategy;
    }

    protected static URL getEndpoint(URI host, JsonObject source, String id, String def) throws MalformedURLException {
        return host.resolve("/" + getEndpointFromConfig(source, id, def)).toURL();
    }

    protected static String getEndpointFromConfig(JsonObject source, String id, String def) {
        return source == null ? def : GsonHelper.getAsString(source, id, def);
    }

    public static ServerTextFilter createFromConfig(DedicatedServerProperties config) {
        if (StringUtil.isBlank(config.textFilteringConfig)) {
            return null;
        }
        if (config.textFilteringVersion == 0) {
            return LegacyTextFilter.createTextFilterFromConfig(config.textFilteringConfig);
        }
        LOGGER.warn("Could not create text filter - unsupported text filtering version used");
        return null;
    }

    protected CompletableFuture<FilteredText> requestMessageProcessing(GameProfile sender, String message, IgnoreStrategy ignoreStrategy, Executor executor) {
        if (message.isEmpty()) {
            return CompletableFuture.completedFuture(FilteredText.EMPTY);
        }
        return CompletableFuture.supplyAsync(() -> {
            try {
                return filterText(message, ignoreStrategy, post(chatEncoder.encode(sender, message), chatEndpoint));
            } catch (Exception failure) {
                LOGGER.warn("Failed to validate message '{}'", message, failure);
                return FilteredText.fullyFiltered(message);
            }
        }, executor);
    }

    protected abstract FilteredText filterText(String message, IgnoreStrategy ignoreStrategy, JsonObject result);

    protected FilterMask parseMask(String message, JsonArray removedChars, IgnoreStrategy ignoreStrategy) {
        if (removedChars.isEmpty()) {
            return FilterMask.PASS_THROUGH;
        }
        if (ignoreStrategy.shouldIgnore(message, removedChars.size())) {
            return FilterMask.FULLY_FILTERED;
        }
        FilterMask partial = new FilterMask(message.length());
        removedChars.forEach(index -> partial.setFiltered(index.getAsInt()));
        return partial;
    }

    public void close() {
        workerPool.shutdownNow();
    }

    protected void drainStream(InputStream input) throws IOException {
        input.transferTo(java.io.OutputStream.nullOutputStream());
    }

    private JsonObject post(JsonObject payload, URL url) throws IOException {
        HttpURLConnection connection = makeRequest(payload, url);
        try (InputStream body = connection.getInputStream()) {
            if (connection.getResponseCode() == 204) {
                return new JsonObject();
            }
            try {
                return LenientJsonParser.parse(new InputStreamReader(body, StandardCharsets.UTF_8)).getAsJsonObject();
            } finally {
                drainStream(body);
            }
        }
    }

    protected HttpURLConnection makeRequest(JsonObject payload, URL url) throws IOException {
        HttpURLConnection connection = getURLConnection(url);
        setAuthorizationProperty(connection);
        try (JsonWriter out = new JsonWriter(new OutputStreamWriter(connection.getOutputStream(), StandardCharsets.UTF_8))) {
            com.google.gson.internal.Streams.write(payload, out);
        }
        int status = connection.getResponseCode();
        if (status < 200 || status >= 300) {
            throw new RequestFailedException(status + " " + connection.getResponseMessage());
        }
        return connection;
    }

    protected abstract void setAuthorizationProperty(HttpURLConnection connection);

    protected int connectionReadTimeout() {
        return 2000;
    }

    protected HttpURLConnection getURLConnection(URL url) throws IOException {
        HttpURLConnection connection = (HttpURLConnection) url.openConnection();
        connection.setConnectTimeout(CONNECT_TIMEOUT_MS);
        connection.setReadTimeout(connectionReadTimeout());
        connection.setUseCaches(false);
        connection.setDoOutput(true);
        connection.setDoInput(true);
        connection.setRequestMethod("POST");
        Map.of("Content-Type", "application/json; charset=utf-8", "Accept", "application/json",
            "User-Agent", "Minecraft server" + SharedConstants.getCurrentVersion().name()).forEach(connection::setRequestProperty);
        return connection;
    }

    public TextFilter createContext(GameProfile gameProfile) {
        return new PlayerContext(gameProfile);
    }

    protected static class RequestFailedException extends RuntimeException {
        protected RequestFailedException(String message) {
            super(message);
        }
    }

    protected class PlayerContext implements TextFilter {
        protected final GameProfile profile;
        protected final Executor streamExecutor;

        protected PlayerContext(GameProfile profile) {
            this.profile = profile;
            ConsecutiveExecutor ordered = new ConsecutiveExecutor(workerPool, "chat stream for " + profile.name());
            this.streamExecutor = ordered::schedule;
        }

        public CompletableFuture<List<FilteredText>> processMessageBundle(List<String> messages) {
            List<CompletableFuture<FilteredText>> pending = messages.stream()
                .map(text -> requestMessageProcessing(profile, text, chatIgnoreStrategy, streamExecutor)).toList();
            return Util.sequenceFailFast(pending).exceptionally(failure -> List.of());
        }

        public CompletableFuture<FilteredText> processStreamMessage(String message) {
            return requestMessageProcessing(profile, message, chatIgnoreStrategy, streamExecutor);
        }
    }

    public interface IgnoreStrategy {
        IgnoreStrategy NEVER_IGNORE = (text, removed) -> false;
        IgnoreStrategy IGNORE_FULLY_FILTERED = (text, removed) -> text.length() == removed;

        static IgnoreStrategy ignoreOverThreshold(int threshold) {
            return (text, removed) -> removed >= threshold;
        }

        static IgnoreStrategy select(int hashesToDrop) {
            if (hashesToDrop < 0) {
                return NEVER_IGNORE;
            }
            return hashesToDrop == 0 ? IGNORE_FULLY_FILTERED : ignoreOverThreshold(hashesToDrop);
        }

        boolean shouldIgnore(String message, int removedCharCount);
    }

    protected interface MessageEncoder {
        JsonObject encode(GameProfile profile, String text);
    }
}
