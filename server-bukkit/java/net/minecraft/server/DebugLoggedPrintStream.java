// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.server;

import java.io.OutputStream;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

public class DebugLoggedPrintStream extends LoggedPrintStream {
    private static final Logger LOG = LoggerFactory.getLogger("DEBUG");

    public DebugLoggedPrintStream(String name, OutputStream out) {
        super(name, out);
    }

    protected void logLine(String out) {
        StackTraceElement[] trace = Thread.currentThread().getStackTrace();
        StackTraceElement caller = trace[Math.min(3, trace.length - 1)];
        LOG.info("[{}]@.({}:{}): {}", name, caller.getFileName(), caller.getLineNumber(), out);
    }
}
