// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.server;
public final class RunningOnDifferentThreadException extends RuntimeException {
    public static final RunningOnDifferentThreadException RUNNING_ON_DIFFERENT_THREAD = new RunningOnDifferentThreadException();
    private RunningOnDifferentThreadException() {
        super("Running on a different thread", null, false, false);
    }
    @Override
    public synchronized Throwable fillInStackTrace() {
        return this;
    }
}
