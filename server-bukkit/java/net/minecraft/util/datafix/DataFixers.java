package net.minecraft.util.datafix;

import com.mojang.datafixers.DataFixer;
import org.basaltmc.bukkit.Unsupported;
public class DataFixers {
    protected DataFixers() {
    }
    public static DataFixer getDataFixer() {
        throw Unsupported.hit("net.minecraft.util.datafix.DataFixers.getDataFixer");
    }
}
