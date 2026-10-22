package io.papermc.paper.world;

import io.papermc.paper.world.saveddata.PaperLevelOverrides;
import io.papermc.paper.world.saveddata.PaperWorldPDC;
import java.util.UUID;
import net.minecraft.server.MinecraftServer;
public record PaperWorldLoader(MinecraftServer server, String levelId) {

    public record LoadedWorldData(String bukkitName, UUID uuid, PaperWorldPDC pdc, PaperLevelOverrides levelOverrides) {
    }
}
