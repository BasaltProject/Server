// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.gametest.framework;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

public class LogTestReporter implements TestReporter {
    private static final Logger LOGGER = LoggerFactory.getLogger("GameTest");

    @Override
    public void onTestFailed(GameTestInfo testInfo) {
        String name = testInfo.id().toString();
        if (testInfo.isRequired()) {
            LOGGER.error("{} failed! {}", name, testInfo.getError() == null ? "" : testInfo.getError().getDescription().getString());
        } else {
            LOGGER.warn("(optional) {} failed.", name);
        }
    }

    @Override
    public void onTestSuccess(GameTestInfo testInfo) {}
}
