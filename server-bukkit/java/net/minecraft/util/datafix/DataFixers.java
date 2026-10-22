package net.minecraft.util.datafix;

import com.mojang.datafixers.DataFixer;
import org.basaltmc.bukkit.facade.ServerAccess;

public class DataFixers {
    private DataFixers() {}

    public static DataFixer getDataFixer() {
        return ServerAccess.dataFixer();
    }
}
