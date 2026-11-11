// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.gametest.framework;

import net.minecraft.ChatFormatting;
import net.minecraft.network.chat.Component;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.world.level.block.entity.TestInstanceBlockEntity;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

public class ReportGameListener implements GameTestListener {
    private static final Logger LOGGER = LoggerFactory.getLogger(ReportGameListener.class);
    private int attempts;
    private int successes;

    @Override
    public void testStructureLoaded(GameTestInfo testInfo) {
        attempts++;
    }

    private boolean retryWanted(GameTestInfo testInfo) {
        if (testInfo.isFlaky()) {
            return attempts < testInfo.maxAttempts() && successes < testInfo.requiredSuccesses();
        }
        return testInfo.retryOptions().hasTriesLeft(attempts, successes);
    }

    @Override
    public void testPassed(GameTestInfo testInfo, GameTestRunner runner) {
        successes++;
        if (retryWanted(testInfo)) {
            say(testInfo.getLevel(), ChatFormatting.GREEN, testInfo.id() + " passed " + successes + " time(s), running again");
            runner.rerunTest(testInfo);
            return;
        }
        reportPassed(testInfo, testInfo.id() + " passed");
    }

    @Override
    public void testFailed(GameTestInfo testInfo, GameTestRunner runner) {
        reportFailure(testInfo, testInfo.getError());
        if (retryWanted(testInfo)) {
            runner.rerunTest(testInfo);
        } else {
            GlobalTestReporter.onTestFailed(testInfo);
        }
    }

    @Override
    public void testAddedForRerun(GameTestInfo original, GameTestInfo copy, GameTestRunner runner) {
        copy.addListener(this);
    }

    public static void reportPassed(GameTestInfo testInfo, String text) {
        testInfo.getTestInstanceBlockEntity().setSuccess();
        say(testInfo.getLevel(), ChatFormatting.GREEN, text);
        GlobalTestReporter.onTestSuccess(testInfo);
    }

    protected static void reportFailure(GameTestInfo testInfo, Throwable error) {
        Component description = error instanceof GameTestException failure ? failure.getDescription() : Component.literal(Util.describe(error));
        testInfo.getTestInstanceBlockEntity().setErrorMessage(description);
        visualizeFailedTest(testInfo, error);
        String line = (testInfo.isRequired() ? "" : "(optional) ") + testInfo.id() + " failed! " + description.getString();
        say(testInfo.getLevel(), testInfo.isRequired() ? ChatFormatting.RED : ChatFormatting.YELLOW, line);
        if (!(error instanceof GameTestException)) {
            LOGGER.error("Unexpected failure in {}", testInfo.id(), error);
        }
    }

    protected static void visualizeFailedTest(GameTestInfo testInfo, Throwable error) {
        if (error instanceof GameTestAssertPosException positioned) {
            testInfo.getTestInstanceBlockEntity().markError(positioned.getAbsolutePos(), positioned.getMessageToShowAtBlock());
        }
    }

    protected static void say(ServerLevel level, ChatFormatting format, String text) {
        level.getServer().sendSystemMessage(Component.literal(text).withStyle(format));
        LOGGER.info(text);
    }

    private static final class Util {
        static String describe(Throwable error) {
            return error.getClass().getSimpleName() + (error.getMessage() == null ? "" : ": " + error.getMessage());
        }
    }
}
