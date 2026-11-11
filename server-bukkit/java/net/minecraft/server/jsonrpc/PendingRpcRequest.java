// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.server.jsonrpc;

import com.google.gson.JsonElement;
import java.util.Objects;
import java.util.concurrent.CompletableFuture;
import net.minecraft.core.Holder;

public record PendingRpcRequest<Result>(Holder.Reference<? extends OutgoingRpcMethod<?, ? extends Result>> method,
    CompletableFuture<Result> resultFuture, long timeoutTime) {
    public void accept(JsonElement response) {
        try {
            resultFuture.complete(Objects.requireNonNull(method.value().decodeResult(response)));
        } catch (Exception failure) {
            resultFuture.completeExceptionally(failure);
        }
    }

    public boolean timedOut(long currentTime) {
        return currentTime > timeoutTime;
    }
}
