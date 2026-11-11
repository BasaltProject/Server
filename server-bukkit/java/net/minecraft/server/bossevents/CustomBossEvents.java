// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.server.bossevents;

import java.util.Collection;
import java.util.LinkedHashMap;
import java.util.Map;
import net.minecraft.network.chat.Component;
import net.minecraft.resources.Identifier;
import net.minecraft.server.level.ServerPlayer;
import net.minecraft.util.RandomSource;
import net.minecraft.world.level.saveddata.SavedData;
import net.minecraft.world.level.saveddata.SavedDataType;

public class CustomBossEvents extends SavedData {
    public static final SavedDataType<CustomBossEvents> TYPE = new SavedDataType<>(net.minecraft.resources.Identifier.withDefaultNamespace("custom_boss_events"), CustomBossEvents::new, com.mojang.serialization.MapCodec.unitCodec(CustomBossEvents::new), net.minecraft.util.datafix.DataFixTypes.SAVED_DATA_CUSTOM_BOSS_EVENTS);
    private final Map<Identifier, CustomBossEvent> events = new LinkedHashMap<>();

    public CustomBossEvent get(Identifier id) {
        return events.get(id);
    }

    public CustomBossEvent create(RandomSource random, Identifier id, Component name) {
        CustomBossEvent event = new CustomBossEvent(new java.util.UUID(random.nextLong(), random.nextLong()), id, name, this::setDirty);
        events.put(id, event);
        setDirty();
        return event;
    }

    public void remove(CustomBossEvent event) {
        events.remove(event.customId());
        setDirty();
    }

    public Collection<Identifier> getIds() {
        return events.keySet();
    }

    public Collection<CustomBossEvent> getEvents() {
        return events.values();
    }

    public void onPlayerConnect(ServerPlayer player) {
        events.values().forEach(event -> event.onPlayerConnect(player));
    }

    public void onPlayerDisconnect(ServerPlayer player) {
        events.values().forEach(event -> event.onPlayerDisconnect(player));
    }
}
