// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util;
public class ExceptionCollector<T extends Throwable> {
    private T result;
    public void add(T throwable) {
        if (result == null) {
            result = throwable;
        } else {
            result.addSuppressed(throwable);
        }
    }
    public void throwIfPresent() throws T {
        if (result != null) {
            throw result;
        }
    }
}
