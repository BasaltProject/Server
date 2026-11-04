// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.gametest.framework;
public class ExhaustedAttemptsException extends Throwable {
    public ExhaustedAttemptsException(int attempts, int successes, GameTestInfo testInfo) {
        super();
    }
}
