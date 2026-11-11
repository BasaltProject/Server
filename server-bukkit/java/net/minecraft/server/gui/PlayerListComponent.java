// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.server.gui;
import java.util.Vector;
import javax.swing.JList;
import net.minecraft.server.MinecraftServer;
public class PlayerListComponent extends JList<String> {
    private final MinecraftServer server;
    private int lastTick;
    public PlayerListComponent(MinecraftServer server) {
        this.server = server;
    }
    public void tick() {
        if (lastTick++ % 20 != 0) {
            return;
        }
        Vector<String> names = new Vector<>();
        for (String name : server.getPlayerNames()) {
            names.add(name);
        }
        setListData(names);
    }
}
