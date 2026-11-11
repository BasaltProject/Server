// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.server.jsonrpc;

import com.google.gson.JsonArray;
import com.google.gson.JsonElement;
import com.google.gson.JsonNull;
import com.google.gson.JsonObject;
import com.google.gson.JsonParseException;
import io.netty.channel.Channel;
import io.netty.channel.ChannelHandlerContext;
import io.netty.channel.SimpleChannelInboundHandler;
import io.netty.handler.timeout.ReadTimeoutException;
import java.util.List;
import java.util.Map;
import java.util.Optional;
import java.util.concurrent.CompletableFuture;
import java.util.concurrent.CompletionException;
import java.util.concurrent.ConcurrentHashMap;
import java.util.concurrent.atomic.AtomicInteger;
import net.minecraft.core.Holder;
import net.minecraft.core.registries.BuiltInRegistries;
import net.minecraft.resources.Identifier;
import net.minecraft.server.jsonrpc.internalapi.MinecraftApi;
import net.minecraft.server.jsonrpc.methods.ClientInfo;
import net.minecraft.server.jsonrpc.methods.EncodeJsonRpcException;
import net.minecraft.server.jsonrpc.methods.InvalidParameterJsonRpcException;
import net.minecraft.server.jsonrpc.methods.InvalidRequestJsonRpcException;
import net.minecraft.server.jsonrpc.methods.MethodNotFoundJsonRpcException;
import net.minecraft.server.jsonrpc.methods.RemoteRpcErrorException;
import net.minecraft.server.notifications.NotificationManager;
import net.minecraft.util.GsonHelper;
import net.minecraft.util.Util;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

public class Connection extends SimpleChannelInboundHandler<JsonElement> {
    private static final Logger LOG = LoggerFactory.getLogger(Connection.class);
    private static final AtomicInteger CONNECTIONS = new AtomicInteger();
    private static final long REPLY_WINDOW_MS = 5000L;
    private final JsonRpcLogger logger;
    private final ClientInfo client;
    private final ManagementServer owner;
    private final Channel channel;
    private final MinecraftApi api;
    private final AtomicInteger nextRequest = new AtomicInteger();
    private final Map<Integer, PendingRpcRequest<?>> awaiting = new ConcurrentHashMap<>();

    public Connection(Channel channel, ManagementServer managementServer, MinecraftApi minecraftApi, JsonRpcLogger jsonrpcLogger) {
        this.client = ClientInfo.of(CONNECTIONS.incrementAndGet());
        this.owner = managementServer;
        this.api = minecraftApi;
        this.channel = channel;
        this.logger = jsonrpcLogger;
    }

    public void tick() {
        long now = Util.getMillis();
        awaiting.entrySet().removeIf(entry -> {
            PendingRpcRequest<?> request = entry.getValue();
            if (!request.timedOut(now)) {
                return false;
            }
            request.resultFuture().completeExceptionally(new ReadTimeoutException(
                "RPC method " + request.method().key().identifier() + " timed out waiting for response"));
            return true;
        });
    }

    public void channelActive(ChannelHandlerContext ctx) throws Exception {
        logger.log(client, "Management connection opened for {}", channel.remoteAddress());
        super.channelActive(ctx);
        owner.onConnected(this);
    }

    public void channelInactive(ChannelHandlerContext ctx) throws Exception {
        logger.log(client, "Management connection closed for {}", channel.remoteAddress());
        super.channelInactive(ctx);
        owner.onDisconnected(this);
    }

    public void exceptionCaught(ChannelHandlerContext ctx, Throwable cause) throws Exception {
        if (cause.getCause() instanceof JsonParseException) {
            channel.writeAndFlush(JsonRPCErrors.PARSE_ERROR.createWithUnknownId(cause.getMessage()));
            return;
        }
        super.exceptionCaught(ctx, cause);
        channel.close().awaitUninterruptibly();
    }

    protected void channelRead0(ChannelHandlerContext channelHandlerContext, JsonElement jsonElement) {
        if (jsonElement.isJsonObject()) {
            JsonObject reply = handleJsonObject(jsonElement.getAsJsonObject());
            if (reply != null) {
                channel.writeAndFlush(reply);
            }
        } else if (jsonElement.isJsonArray()) {
            JsonArray replies = new JsonArray();
            for (JsonElement entry : jsonElement.getAsJsonArray()) {
                JsonObject reply = handleJsonObject(entry.getAsJsonObject());
                if (reply != null) {
                    replies.add(reply);
                }
            }
            channel.writeAndFlush(replies);
        } else {
            channel.writeAndFlush(JsonRPCErrors.INVALID_REQUEST.createWithUnknownId(null));
        }
    }

    public void sendNotification(Holder.Reference<? extends OutgoingRpcMethod<Void, ?>> method) {
        dispatch(method, null, false);
    }

    public <Params> void sendNotification(Holder.Reference<? extends OutgoingRpcMethod<Params, ?>> method, Params params) {
        dispatch(method, params, false);
    }

    public <Result> CompletableFuture<Result> sendRequest(Holder.Reference<? extends OutgoingRpcMethod<Void, Result>> method) {
        return dispatch(method, null, true);
    }

    public <Params, Result> CompletableFuture<Result> sendRequest(Holder.Reference<? extends OutgoingRpcMethod<Params, Result>> method, Params params) {
        return dispatch(method, params, true);
    }

    @SuppressWarnings({"unchecked", "rawtypes"})
    private <Params, Result> CompletableFuture<Result> dispatch(Holder.Reference<? extends OutgoingRpcMethod<Params, ? extends Result>> holder,
        Params params, boolean expectReply) {
        OutgoingRpcMethod<Params, ? extends Result> method = holder.value();
        if (api.notificationManager().server() == null && !method.attributes().allowPreServerInit()) {
            return CompletableFuture.failedFuture(new InvalidRequestJsonRpcException("Method cannot be dispatched pre server initialization: " + method));
        }
        List<JsonElement> arguments = params == null ? List.of() : List.of(method.encodeParams(params));
        Identifier name = holder.key().identifier();
        if (!expectReply) {
            channel.writeAndFlush(JsonRPCUtils.createRequest((Integer) null, name, arguments));
            return null;
        }
        CompletableFuture<Result> future = new CompletableFuture<>();
        int id = nextRequest.incrementAndGet();
        awaiting.put(id, new PendingRpcRequest(holder, future, Util.getMillis() + REPLY_WINDOW_MS));
        channel.writeAndFlush(JsonRPCUtils.createRequest(id, name, arguments));
        return future;
    }

    JsonObject handleJsonObject(JsonObject message) {
        try {
            JsonElement id = JsonRPCUtils.getRequestId(message);
            String method = JsonRPCUtils.getMethodName(message);
            JsonElement result = JsonRPCUtils.getResult(message);
            JsonObject error = JsonRPCUtils.getError(message);
            if (method != null && result == null && error == null) {
                if (id != null && !(id.isJsonNull() || GsonHelper.isNumberValue(id) || GsonHelper.isStringValue(id))) {
                    return JsonRPCErrors.INVALID_REQUEST.createWithUnknownId("Invalid request id - only String, Number and NULL supported");
                }
                return answer(id, method, JsonRPCUtils.getParams(message));
            }
            if (method == null && result != null && error == null && id != null) {
                if (GsonHelper.isNumberValue(id)) {
                    PendingRpcRequest<?> request = awaiting.remove(id.getAsInt());
                    if (request != null) {
                        request.accept(result);
                    } else {
                        LOG.warn("Received unknown response (id: {}): {}", id, result);
                    }
                } else {
                    LOG.warn("Received respose {} with id {} we did not request", result, id);
                }
                return null;
            }
            if (method == null && result == null && error != null) {
                failPending(id, error);
                return null;
            }
            return JsonRPCErrors.INVALID_REQUEST.createWithoutData(id == null ? JsonNull.INSTANCE : id);
        } catch (Exception failure) {
            LOG.error("Error while handling rpc request", failure);
            return JsonRPCErrors.INTERNAL_ERROR.createWithUnknownId("Unknown error handling request - check server logs for stack trace");
        }
    }

    private void failPending(JsonElement id, JsonObject error) {
        if (id != null && GsonHelper.isNumberValue(id)) {
            PendingRpcRequest<?> request = awaiting.remove(id.getAsInt());
            if (request != null) {
                request.resultFuture().completeExceptionally(new RemoteRpcErrorException(id, error));
            }
        }
        LOG.error("Received error (id: {}): {}", id, error);
    }

    private JsonObject answer(JsonElement id, String method, JsonElement params) {
        boolean wantsReply = id != null;
        try {
            JsonElement result = dispatchIncomingRequest(method, params);
            return result != null && wantsReply ? JsonRPCUtils.createSuccessResult(id, result) : null;
        } catch (InvalidParameterJsonRpcException bad) {
            LOG.debug("Invalid parameter invocation {}: {}", method, params, bad);
            return wantsReply ? JsonRPCErrors.INVALID_PARAMS.create(id, bad.getMessage()) : null;
        } catch (EncodeJsonRpcException bad) {
            LOG.error("Failed to encode json rpc response {}", method, bad);
            return wantsReply ? JsonRPCErrors.INTERNAL_ERROR.create(id, bad.getMessage()) : null;
        } catch (InvalidRequestJsonRpcException bad) {
            return wantsReply ? JsonRPCErrors.INVALID_REQUEST.create(id, bad.getMessage()) : null;
        } catch (MethodNotFoundJsonRpcException bad) {
            return wantsReply ? JsonRPCErrors.METHOD_NOT_FOUND.create(id, bad.getMessage()) : null;
        } catch (Exception bad) {
            LOG.error("Error while dispatching rpc method {}", method, bad);
            return wantsReply ? JsonRPCErrors.INTERNAL_ERROR.createWithoutData(id) : null;
        }
    }

    public JsonElement dispatchIncomingRequest(String method, JsonElement params) {
        IncomingRpcMethods.ensureRegistered();
        Identifier name = Identifier.tryParse(method);
        if (name == null) {
            throw new InvalidRequestJsonRpcException("Failed to parse method value: " + method);
        }
        Optional<IncomingRpcMethod<?, ?>> found = BuiltInRegistries.INCOMING_RPC_METHOD.getOptional(name);
        if (found.isEmpty()) {
            throw new MethodNotFoundJsonRpcException("Method not found: " + method);
        }
        IncomingRpcMethod<?, ?> target = found.get();
        NotificationManager notifications = api.notificationManager();
        if (notifications != null && notifications.server() == null && !target.attributes().allowPreServerInit()) {
            throw new InvalidRequestJsonRpcException("Method cannot be dispatched pre server initialization: " + method);
        }
        if (!target.attributes().runOnMainThread()) {
            return target.apply(api, params, client);
        }
        try {
            return api.submit(() -> target.apply(api, params, client)).join();
        } catch (CompletionException wrapped) {
            if (wrapped.getCause() instanceof RuntimeException inner) {
                throw inner;
            }
            throw wrapped;
        }
    }
}
