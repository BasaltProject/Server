// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world;

public record Stopwatch(long creationTime, long accumulatedElapsedTime) {
    public Stopwatch(long creationTime) {
        this(creationTime, 0L);
    }

    public long elapsedMilliseconds(long currentTime) {
        return accumulatedElapsedTime + (currentTime - creationTime);
    }

    public double elapsedSeconds(long currentTime) {
        return elapsedMilliseconds(currentTime) / 1000.0;
    }
}
