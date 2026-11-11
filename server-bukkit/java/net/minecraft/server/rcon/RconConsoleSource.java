// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.server.rcon;
import net.minecraft.commands.CommandSource;
import net.minecraft.commands.CommandSourceStack;
import net.minecraft.network.chat.Component;
import net.minecraft.server.MinecraftServer;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.server.permissions.LevelBasedPermissionSet;
import net.minecraft.world.phys.Vec2;
import net.minecraft.world.phys.Vec3;
public class RconConsoleSource implements CommandSource {
    private static final Component kName = Component.literal("Rcon");
    private final StringBuffer output = new StringBuffer();
    private final MinecraftServer server;
    public RconConsoleSource(MinecraftServer server) {
        this.server = server;
    }
    public void prepareForCommand() {
        output.setLength(0);
    }
    public String getCommandResponse() {
        return output.toString();
    }
    public CommandSourceStack createCommandSourceStack() {
        ServerLevel level = server.overworld();
        return new CommandSourceStack(this, Vec3.atLowerCornerOf(level.getRespawnData().pos()), Vec2.ZERO, level, LevelBasedPermissionSet.OWNER, kName, server);
    }
    public void sendSystemMessage(Component message) {
        output.append(message.getString());
    }
    public boolean acceptsSuccess() {
        return true;
    }
    public boolean acceptsFailure() {
        return true;
    }
    public boolean shouldInformAdmins() {
        return server.shouldRconBroadcast();
    }
}
