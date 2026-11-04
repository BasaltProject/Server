// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.server.jsonrpc.methods;
public class EncodeJsonRpcException extends RuntimeException {
    public EncodeJsonRpcException(String message) {
        super(message);
    }
}
