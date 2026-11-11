// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.gametest.framework;

public record RetryOptions(int numberOfTries, boolean haltOnFailure) {
    private static final RetryOptions NO_RETRIES = new RetryOptions(1, true);

    public static RetryOptions noRetries() {
        return NO_RETRIES;
    }

    public boolean unlimitedTries() {
        return numberOfTries < 1;
    }

    public boolean hasTriesLeft(int attempts, int successes) {
        boolean failedOnce = attempts != successes;
        boolean halted = haltOnFailure && failedOnce;
        return !halted && (unlimitedTries() || attempts < numberOfTries);
    }

    public boolean hasRetries() {
        return numberOfTries != 1;
    }
}
