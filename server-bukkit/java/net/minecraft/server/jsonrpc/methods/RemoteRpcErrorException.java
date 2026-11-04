// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.server.jsonrpc.methods;
import com.google.gson.JsonElement;
import com.google.gson.JsonObject;
public class RemoteRpcErrorException extends RuntimeException {
    public RemoteRpcErrorException(JsonElement id, JsonObject error) {
        super();
    }
}
