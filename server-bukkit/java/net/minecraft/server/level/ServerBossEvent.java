// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.server.level;

import java.util.Collection;
import java.util.Collections;
import java.util.LinkedHashSet;
import java.util.Set;
import java.util.UUID;
import net.minecraft.network.chat.Component;
import net.minecraft.network.protocol.game.ClientboundBossEventPacket;
import net.minecraft.world.BossEvent;

public class ServerBossEvent extends BossEvent {
    private final Set<ServerPlayer> viewers = new LinkedHashSet<>();
    private final Collection<ServerPlayer> viewersView = Collections.unmodifiableCollection(viewers);
    private boolean visible = true;

    public ServerBossEvent(UUID id, Component name, BossEvent.BossBarColor color, BossEvent.BossBarOverlay overlay) {
        super(id, name, color, overlay);
    }

    @Override
    public void setProgress(float progress) {
        if (progress != this.progress) {
            super.setProgress(progress);
            broadcast(ClientboundBossEventPacket.createUpdateProgressPacket(this));
        }
    }

    @Override
    public void setColor(BossEvent.BossBarColor color) {
        if (color != this.color) {
            super.setColor(color);
            broadcast(ClientboundBossEventPacket.createUpdateStylePacket(this));
        }
    }

    @Override
    public void setOverlay(BossEvent.BossBarOverlay overlay) {
        if (overlay != this.overlay) {
            super.setOverlay(overlay);
            broadcast(ClientboundBossEventPacket.createUpdateStylePacket(this));
        }
    }

    @Override
    public BossEvent setDarkenScreen(boolean darkenScreen) {
        if (darkenScreen != this.darkenScreen) {
            super.setDarkenScreen(darkenScreen);
            broadcast(ClientboundBossEventPacket.createUpdatePropertiesPacket(this));
        }
        return this;
    }

    @Override
    public BossEvent setPlayBossMusic(boolean playBossMusic) {
        if (playBossMusic != this.playBossMusic) {
            super.setPlayBossMusic(playBossMusic);
            broadcast(ClientboundBossEventPacket.createUpdatePropertiesPacket(this));
        }
        return this;
    }

    @Override
    public BossEvent setCreateWorldFog(boolean createWorldFog) {
        if (createWorldFog != this.createWorldFog) {
            super.setCreateWorldFog(createWorldFog);
            broadcast(ClientboundBossEventPacket.createUpdatePropertiesPacket(this));
        }
        return this;
    }

    @Override
    public void setName(Component name) {
        if (!java.util.Objects.equals(name, this.name)) {
            super.setName(name);
            broadcast(ClientboundBossEventPacket.createUpdateNamePacket(this));
        }
    }

    public void addPlayer(ServerPlayer player) {
        if (viewers.add(player) && visible) {
            player.connection.send(ClientboundBossEventPacket.createAddPacket(this));
        }
    }

    public void removePlayer(ServerPlayer player) {
        if (viewers.remove(player) && visible) {
            player.connection.send(ClientboundBossEventPacket.createRemovePacket(getId()));
        }
    }

    public void removeAllPlayers() {
        if (!viewers.isEmpty()) {
            for (ServerPlayer player : new LinkedHashSet<>(viewers)) {
                removePlayer(player);
            }
        }
    }

    public boolean isVisible() {
        return visible;
    }

    public void setVisible(boolean visible) {
        if (visible == this.visible) {
            return;
        }
        this.visible = visible;
        broadcast(visible ? ClientboundBossEventPacket.createAddPacket(this)
            : ClientboundBossEventPacket.createRemovePacket(getId()), true);
    }

    public Collection<ServerPlayer> getPlayers() {
        return viewersView;
    }

    protected void setDirty() {}

    private void broadcast(ClientboundBossEventPacket packet) {
        broadcast(packet, false);
    }

    private void broadcast(ClientboundBossEventPacket packet, boolean ignoreVisibility) {
        if (visible || ignoreVisibility) {
            for (ServerPlayer player : viewers) {
                player.connection.send(packet);
            }
        }
    }
}
