// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.storage;
public record DataVersion(int version, String series) {
    public static final String MAIN_SERIES = "main";
    public boolean isSideSeries() {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.storage.DataVersion#isSideSeries");
    }
    public boolean isCompatible(DataVersion other) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.storage.DataVersion#isCompatible");
    }
}
