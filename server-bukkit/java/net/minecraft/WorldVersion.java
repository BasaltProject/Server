// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft;

import net.minecraft.world.level.storage.DataVersion;
import java.util.Date;
import net.minecraft.server.packs.PackType;
import net.minecraft.server.packs.metadata.pack.PackFormat;

public interface WorldVersion {
    DataVersion dataVersion();

    String id();

    String name();

    int protocolVersion();
    default PackFormat packVersion(PackType packType) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.WorldVersion#packVersion");
    }
    default Date buildTime() {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.WorldVersion#buildTime");
    }
    default boolean stable() {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.WorldVersion#stable");
    }
    public static record Simple(String id, String name, DataVersion dataVersion, int protocolVersion, PackFormat resourcePackVersion, PackFormat datapackVersion, Date buildTime, boolean stable) implements WorldVersion {
        public PackFormat packVersion(PackType packType) {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.WorldVersion.Simple#packVersion");
        }
    }
}
