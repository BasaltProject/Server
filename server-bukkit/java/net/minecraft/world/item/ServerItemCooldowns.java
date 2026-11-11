// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.item;
import net.minecraft.network.protocol.game.ClientboundCooldownPacket;
import net.minecraft.resources.Identifier;
import net.minecraft.server.level.ServerPlayer;
public class ServerItemCooldowns extends ItemCooldowns {
    private final ServerPlayer player;
    public ServerItemCooldowns(ServerPlayer player) {
        this.player = player;
    }
    protected void onCooldownStarted(Identifier cooldownGroup, int duration) {
        super.onCooldownStarted(cooldownGroup, duration);
        player.connection.send(new ClientboundCooldownPacket(cooldownGroup, duration));
    }
    protected void onCooldownEnded(Identifier cooldownGroup) {
        super.onCooldownEnded(cooldownGroup);
        player.connection.send(new ClientboundCooldownPacket(cooldownGroup, 0));
    }
}
