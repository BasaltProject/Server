package net.minecraft.util;

import com.google.gson.Gson;
import org.basaltmc.bukkit.Unsupported;
public class GsonHelper {
    protected GsonHelper() {
    }
    public static Object fromJson(Gson gson, String string, Class type) {
        throw Unsupported.hit("net.minecraft.util.GsonHelper.fromJson");
    }
}
