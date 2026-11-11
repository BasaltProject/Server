// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.server.bossevents;

import com.mojang.serialization.Codec;
import java.util.Collection;
import java.util.HashSet;
import java.util.Set;
import java.util.UUID;
import net.minecraft.network.chat.Component;
import net.minecraft.network.chat.ComponentUtils;
import net.minecraft.network.chat.HoverEvent;
import net.minecraft.resources.Identifier;
import net.minecraft.server.level.ServerBossEvent;
import net.minecraft.server.level.ServerPlayer;
import net.minecraft.util.Mth;
import net.minecraft.world.BossEvent;
import org.basaltmc.bukkit.facade.LazyCodecs;

public class CustomBossEvent extends ServerBossEvent {
    private static final int kDefaultMax = 100;
    private final Identifier customId;
    private final Set<UUID> members = new HashSet<>();
    private int value;
    private int max = kDefaultMax;
    private final Runnable dirtyCallback;

    public CustomBossEvent(UUID id, Identifier customId, Component name, Runnable dirtyCallback) {
        super(id, name, BossEvent.BossBarColor.WHITE, BossEvent.BossBarOverlay.PROGRESS);
        this.dirtyCallback = dirtyCallback;
        this.customId = customId;
        setProgress(0.0F);
    }

    public Identifier customId() {
        return customId;
    }

    @Override
    public void addPlayer(ServerPlayer player) {
        super.addPlayer(player);
        if (members.add(player.getUUID())) {
            setDirty();
        }
    }

    @Override
    public void removePlayer(ServerPlayer player) {
        super.removePlayer(player);
        if (members.remove(player.getUUID())) {
            setDirty();
        }
    }

    @Override
    public void removeAllPlayers() {
        super.removeAllPlayers();
        if (!members.isEmpty()) {
            members.clear();
            setDirty();
        }
    }

    public int value() {
        return value;
    }

    public int max() {
        return max;
    }

    public void setValue(int value) {
        this.value = value;
        setProgress(Mth.clamp((float) value / (float) max, 0.0F, 1.0F));
        setDirty();
    }

    public void setMax(int max) {
        this.max = max;
        setProgress(Mth.clamp((float) value / (float) max, 0.0F, 1.0F));
        setDirty();
    }

    public final Component getDisplayName() {
        String id = customId.toString();
        return ComponentUtils.wrapInSquareBrackets(getName()).withStyle(style -> style.withColor(getColor().getFormatting())
            .withHoverEvent(new HoverEvent.ShowText(Component.literal(id))).withInsertion(id));
    }

    public boolean setPlayers(Collection<ServerPlayer> players) {
        Set<UUID> wanted = new HashSet<>();
        for (ServerPlayer player : players) {
            wanted.add(player.getUUID());
        }
        boolean changed = false;
        for (ServerPlayer present : new HashSet<>(getPlayers())) {
            if (!wanted.contains(present.getUUID())) {
                removePlayer(present);
                changed = true;
            }
        }
        members.removeIf(member -> !wanted.contains(member));
        for (ServerPlayer player : players) {
            if (!members.contains(player.getUUID())) {
                addPlayer(player);
                changed = true;
            }
        }
        if (changed) {
            setDirty();
        }
        return changed;
    }

    public static CustomBossEvent load(UUID id, Identifier customId, Packed packed, Runnable setDirty) {
        CustomBossEvent event = new CustomBossEvent(id, customId, packed.name(), setDirty);
        event.setVisible(packed.visible());
        event.setValue(packed.value());
        event.setMax(packed.max());
        event.setColor(packed.color());
        event.setOverlay(packed.overlay());
        event.setDarkenScreen(packed.darkenScreen());
        event.setPlayBossMusic(packed.playBossMusic());
        event.setCreateWorldFog(packed.createWorldFog());
        event.members.addAll(packed.players());
        return event;
    }

    public Packed pack() {
        return new Packed(getName(), isVisible(), value, max, getColor(), getOverlay(), shouldDarkenScreen(),
            shouldPlayBossMusic(), shouldCreateWorldFog(), Set.copyOf(members));
    }

    public void onPlayerConnect(ServerPlayer player) {
        if (members.contains(player.getUUID())) {
            addPlayer(player);
        }
    }

    public void onPlayerDisconnect(ServerPlayer player) {
        super.removePlayer(player);
    }

    @Override
    public void setDirty() {
        if (dirtyCallback != null) {
            dirtyCallback.run();
        }
    }

    public static record Packed(Component name, boolean visible, int value, int max, BossEvent.BossBarColor color,
        BossEvent.BossBarOverlay overlay, boolean darkenScreen, boolean playBossMusic, boolean createWorldFog, Set<UUID> players) {
        public static final Codec<Packed> CODEC = LazyCodecs.codec(CustomBossEvent.Packed.class, "CODEC");
    }
}
