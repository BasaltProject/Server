package net.minecraft.util;

import net.minecraft.TracingExecutor;
import org.basaltmc.bukkit.Unsupported;
public class Util {
    protected Util() {
    }
    public static TracingExecutor backgroundExecutor() {
        throw Unsupported.hit("net.minecraft.util.Util.backgroundExecutor");
    }
    public static long getMillis() {
        throw Unsupported.hit("net.minecraft.util.Util.getMillis");
    }
}
