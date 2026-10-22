package net.minecraft.core;

import java.util.UUID;
import org.basaltmc.bukkit.Unsupported;
public class UUIDUtil {
    protected UUIDUtil() {
    }
    public static int[] uuidToIntArray(UUID uuid) {
        throw Unsupported.hit("net.minecraft.core.UUIDUtil.uuidToIntArray");
    }
}
