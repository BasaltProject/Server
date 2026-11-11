// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.server.jsonrpc;

import net.minecraft.server.jsonrpc.methods.ClientInfo;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

public class JsonRpcLogger {
    private static final Logger LOG = LoggerFactory.getLogger(JsonRpcLogger.class);

    public void log(ClientInfo clientInfo, String message, Object... args) {
        Object[] all = new Object[args.length + 1];
        all[0] = clientInfo.connectionId();
        System.arraycopy(args, 0, all, 1, args.length);
        LOG.info("RPC Connection #{}: " + message, all);
    }
}
