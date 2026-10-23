package io.papermc.paper.util;

import java.util.concurrent.Executor;
import org.basaltmc.bukkit.facade.TickQueue;

public class MCUtil {
    protected MCUtil() {
    }
    public static final Executor MAIN_EXECUTOR = TickQueue::run;
}
