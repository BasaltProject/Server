// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.server;

import java.io.OutputStream;
import java.io.PrintStream;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

public class LoggedPrintStream extends PrintStream {
    private static final Logger LOG = LoggerFactory.getLogger("STDOUT");
    protected final String name;

    public LoggedPrintStream(String name, OutputStream out) {
        super(out);
        this.name = name;
    }

    public void println(String string) {
        logLine(string);
    }

    public void println(Object object) {
        logLine(String.valueOf(object));
    }

    protected void logLine(String out) {
        LOG.info("[{}]: {}", name, out);
    }
}
