// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.server.packs;
public enum PackType {
    CLIENT_RESOURCES("assets"), SERVER_DATA("data");
    private final String directory;

    PackType(String directory) {
        this.directory = directory;
    }

    public String getDirectory() {
        return directory;
    }
}
