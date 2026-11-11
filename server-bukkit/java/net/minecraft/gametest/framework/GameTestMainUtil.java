// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.gametest.framework;

import java.io.File;
import java.util.Optional;
import java.util.function.Consumer;

public class GameTestMainUtil {
    private static String option(String[] args, String name) {
        for (int i = 0; i < args.length - 1; i++) {
            if (args[i].equals("--" + name)) {
                return args[i + 1];
            }
        }
        return null;
    }

    private static boolean flag(String[] args, String name) {
        for (String arg : args) {
            if (arg.equals("--" + name)) {
                return true;
            }
        }
        return false;
    }

    public static void runGameTestServer(String[] args, Consumer<String> onUniverseCreated) throws Exception {
        String universe = option(args, "universe");
        if (universe == null) {
            universe = "gametestworld";
        }
        onUniverseCreated.accept(universe);
        String report = option(args, "report");
        if (report != null) {
            GlobalTestReporter.replaceWith(new JUnitLikeTestReporter(new File(report)));
        }
        String repeat = option(args, "repeat");
        GameTestServer server = GameTestServer.create(Thread.currentThread(), null, null,
            Optional.ofNullable(option(args, "tests")), flag(args, "verify"), repeat == null ? 1 : Integer.parseInt(repeat));
        server.start();
    }
}
