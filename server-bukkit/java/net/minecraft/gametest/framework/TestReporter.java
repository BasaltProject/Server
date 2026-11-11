// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.gametest.framework;

public interface TestReporter {
    void onTestFailed(GameTestInfo testInfo);

    void onTestSuccess(GameTestInfo testInfo);

    default void finish() {}
}
