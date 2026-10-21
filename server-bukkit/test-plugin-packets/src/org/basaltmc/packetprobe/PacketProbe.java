package org.basaltmc.packetprobe;

import com.github.retrooper.packetevents.PacketEvents;
import com.github.retrooper.packetevents.event.PacketListenerAbstract;
import com.github.retrooper.packetevents.event.PacketListenerPriority;
import com.github.retrooper.packetevents.event.PacketReceiveEvent;
import com.github.retrooper.packetevents.event.PacketSendEvent;
import com.github.retrooper.packetevents.protocol.packettype.PacketType;
import com.github.retrooper.packetevents.protocol.player.User;
import com.github.retrooper.packetevents.wrapper.play.client.WrapperPlayClientChatMessage;
import com.github.retrooper.packetevents.wrapper.play.client.WrapperPlayClientPlayerPositionAndRotation;
import com.github.retrooper.packetevents.wrapper.play.server.WrapperPlayServerSetTitleText;
import com.github.retrooper.packetevents.wrapper.play.server.WrapperPlayServerSystemChatMessage;
import java.util.concurrent.atomic.AtomicInteger;
import net.kyori.adventure.text.Component;
import org.bukkit.plugin.java.JavaPlugin;

/** Listens through PacketEvents the way a real plugin would, and logs what it sees. */
public final class PacketProbe extends JavaPlugin {
    private final AtomicInteger moves = new AtomicInteger();
    private final AtomicInteger sent = new AtomicInteger();

    @Override
    public void onEnable() {
        PacketEvents.getAPI().getEventManager().registerListener(new PacketListenerAbstract(PacketListenerPriority.NORMAL) {
            @Override
            public void onPacketReceive(PacketReceiveEvent event) {
                if (event.getPacketType() == PacketType.Play.Client.CHAT_MESSAGE) {
                    WrapperPlayClientChatMessage chat = new WrapperPlayClientChatMessage(event);
                    getLogger().info("PE receive CHAT_MESSAGE: " + chat.getMessage() + " from " + event.getUser().getName());
                    if (chat.getMessage().contains("cancelme")) {
                        event.setCancelled(true);
                        getLogger().info("PE cancelled the chat message");
                    }
                } else if (event.getPacketType() == PacketType.Play.Client.PLAYER_POSITION_AND_ROTATION) {
                    WrapperPlayClientPlayerPositionAndRotation position = new WrapperPlayClientPlayerPositionAndRotation(event);
                    if (moves.incrementAndGet() <= 3) {
                        getLogger().info("PE receive PLAYER_POSITION_AND_ROTATION: " + position.getPosition());
                    }
                }
            }

            @Override
            public void onPacketSend(PacketSendEvent event) {
                if (event.getPacketType() == PacketType.Play.Server.SYSTEM_CHAT_MESSAGE) {
                    WrapperPlayServerSystemChatMessage chat = new WrapperPlayServerSystemChatMessage(event);
                    String text = net.kyori.adventure.text.serializer.plain.PlainTextComponentSerializer.plainText()
                        .serialize(chat.getMessage());
                    getLogger().info("PE send SYSTEM_CHAT_MESSAGE: " + text);
                    if (text.contains("hidden")) {
                        event.setCancelled(true);
                        getLogger().info("PE cancelled a clientbound chat message");
                    }
                } else if (sent.incrementAndGet() <= 5) {
                    getLogger().info("PE send " + event.getPacketType().getName() + " in " + event.getConnectionState());
                }
            }

            @Override
            public void onUserLogin(com.github.retrooper.packetevents.event.UserLoginEvent event) {
                User user = event.getUser();
                getLogger().info("PE user login: " + user.getName() + " client " + user.getClientVersion());
                user.sendPacket(new WrapperPlayServerSystemChatMessage(false, Component.text("PacketEvents says hello")));
                user.sendPacket(new WrapperPlayServerSetTitleText(Component.text("PE title")));
                user.sendPacket(new WrapperPlayServerSystemChatMessage(false, Component.text("hidden message")));
            }
        });
        getLogger().info("PacketProbe listening, PacketEvents " + PacketEvents.getAPI().getVersion());
    }
}
