// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.server.jsonrpc.methods;
public class InvalidParameterJsonRpcException extends RuntimeException {
    public InvalidParameterJsonRpcException(String message) {
        super(message);
    }
}
