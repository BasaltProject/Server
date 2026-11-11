// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft;
import org.slf4j.Logger;
public class DefaultUncaughtExceptionHandler implements Thread.UncaughtExceptionHandler {
    private final Logger log;
    public DefaultUncaughtExceptionHandler(Logger logger) {
        this.log = logger;
    }
    public void uncaughtException(Thread t, Throwable e) {
        log.error("Caught previously unhandled exception :", e);
    }
}
