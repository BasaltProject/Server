package org.basaltmc.bukkit.misc;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.UUID;
import java.util.concurrent.CopyOnWriteArrayList;
import org.basaltmc.bukkit.BasaltServer;
import org.basaltmc.bukkit.MiscNatives;
import org.basaltmc.bukkit.WorldNatives;
import org.bukkit.Bukkit;
import org.bukkit.World;
import org.bukkit.entity.Player;
import org.bukkit.map.MapCursor;
import org.bukkit.map.MapCursorCollection;
import org.bukkit.map.MapRenderer;
import org.bukkit.map.MapView;

public final class BasaltMapView implements MapView {
    private static final Map<Integer, BasaltMapView> kViews = new HashMap<>();
    private static final int kPruneAbove = 1024;
    private static final int kRenderPeriod = 10;
    private static final long kResendTicks = 100;
    private static boolean kTicking;
    private static List<String> kCursorTypes;

    private final int id;
    private final List<MapRenderer> renderers = new CopyOnWriteArrayList<>();
    private record Sent(int fingerprint, long tick) {}

    private final Map<UUID, Sent> sent = new java.util.concurrent.ConcurrentHashMap<>();
    private final boolean virtual;
    private boolean defaultRenderer = true;

    private BasaltMapView(int id, boolean virtual) {
        this.id = id;
        this.virtual = virtual;
    }

    public static synchronized MapView get(int id) {
        BasaltMapView known = kViews.get(id);
        if (known != null) {
            return known;
        }
        if (MiscNatives.mapInfo(id) == null) {
            return null;
        }
        BasaltMapView view = new BasaltMapView(id, false);
        pruneViews();
        kViews.put(id, view);
        return view;
    }

    private static void pruneViews() {
        if (kViews.size() > kPruneAbove) {
            kViews.values().removeIf(view -> view.renderers.isEmpty());
        }
    }

    public static synchronized MapView create(World world) {
        int id = MiscNatives.mapCreate(world.getName(), world.getSpawnLocation().getBlockX(),
            world.getSpawnLocation().getBlockZ(), Scale.NORMAL.getValue());
        if (id < 0) {
            throw new IllegalStateException("The map could not be created");
        }
        BasaltMapView view = new BasaltMapView(id, false);
        kViews.put(id, view);
        org.bukkit.Bukkit.getPluginManager().callEvent(new org.bukkit.event.server.MapInitializeEvent(view));
        return view;
    }

    private String[] info() {
        String[] info = MiscNatives.mapInfo(id);
        if (info == null) {
            throw new IllegalStateException("The map " + id + " is gone");
        }
        return info;
    }

    private void write(String[] info) {
        MiscNatives.mapUpdate(id, info);
    }

    @Override
    public int getId() {
        return id;
    }

    @Override
    public boolean isVirtual() {
        return virtual;
    }

    @Override
    public Scale getScale() {
        return Scale.valueOf(Byte.parseByte(info()[1]));
    }

    @Override
    public void setScale(Scale scale) {
        String[] info = info();
        info[1] = Byte.toString(scale.getValue());
        write(info);
    }

    @Override
    public int getCenterX() {
        return Integer.parseInt(info()[2]);
    }

    @Override
    public int getCenterZ() {
        return Integer.parseInt(info()[3]);
    }

    @Override
    public void setCenterX(int x) {
        String[] info = info();
        info[2] = Integer.toString(x);
        write(info);
    }

    @Override
    public void setCenterZ(int z) {
        String[] info = info();
        info[3] = Integer.toString(z);
        write(info);
    }

    @Override
    public World getWorld() {
        String dimension = info()[0];
        for (World world : Bukkit.getWorlds()) {
            if (WorldNatives.worldStrings(world.getName())[0].equals(dimension)) {
                return world;
            }
        }
        return null;
    }

    @Override
    public void setWorld(World world) {
        String[] info = info();
        info[0] = WorldNatives.worldStrings(world.getName())[0];
        write(info);
    }

    @Override
    public synchronized List<MapRenderer> getRenderers() {
        return new ArrayList<>(renderers);
    }

    @Override
    public synchronized void addRenderer(MapRenderer renderer) {
        if (!renderers.contains(renderer)) {
            renderers.add(renderer);
            renderer.initialize(this);
            watch();
        }
    }

    @Override
    public synchronized boolean removeRenderer(MapRenderer renderer) {
        boolean removed = renderers.remove(renderer);
        if (renderers.isEmpty()) {
            sent.clear();
        }
        return removed;
    }

    @Override
    public boolean isTrackingPosition() {
        return "1".equals(info()[4]);
    }

    @Override
    public void setTrackingPosition(boolean trackingPosition) {
        String[] info = info();
        info[4] = trackingPosition ? "1" : "0";
        write(info);
    }

    @Override
    public boolean isUnlimitedTracking() {
        return "1".equals(info()[5]);
    }

    @Override
    public void setUnlimitedTracking(boolean unlimited) {
        String[] info = info();
        info[5] = unlimited ? "1" : "0";
        write(info);
    }

    @Override
    public boolean isLocked() {
        return "1".equals(info()[6]);
    }

    @Override
    public void setLocked(boolean locked) {
        String[] info = info();
        info[6] = locked ? "1" : "0";
        write(info);
    }

    private void watch() {
        synchronized (BasaltMapView.class) {
            kViews.putIfAbsent(id, this);
            if (kTicking) {
                return;
            }
            kTicking = true;
        }
        BasaltServer.instance().getScheduler().runTaskTimer((org.bukkit.plugin.Plugin) null, BasaltMapView::renderAll,
            kRenderPeriod, kRenderPeriod);
    }

    private static void renderAll() {
        List<BasaltMapView> views;
        synchronized (BasaltMapView.class) {
            views = new ArrayList<>(kViews.values());
        }
        for (BasaltMapView view : views) {
            if (!view.renderers.isEmpty()) {
                view.renderForCarriers();
            }
        }
    }

    public void sendTo(Player player) {
        byte[] colors = MiscNatives.mapColors(id);
        if (colors != null) {
            sent.remove(player.getUniqueId());
            render(player, colors);
        }
    }

    private void renderForCarriers() {
        byte[] colors = MiscNatives.mapColors(id);
        if (colors == null) {
            return;
        }
        for (String uuid : MiscNatives.mapCarriers(id)) {
            Player player = Bukkit.getPlayer(UUID.fromString(uuid));
            if (player != null) {
                render(player, colors);
            }
        }
    }

    private void render(Player player, byte[] base) {
        BasaltMapCanvas canvas = new BasaltMapCanvas(this, base);
        for (MapRenderer renderer : renderers) {
            canvas.setCursors(new MapCursorCollection());
            try {
                renderer.render(this, canvas, player);
            } catch (RuntimeException e) {
                org.basaltmc.bukkit.Natives.log(3, "Bukkit", "A map renderer failed: " + e);
            }
        }
        MapCursorCollection cursors = canvas.getCursors();
        int[] flat = new int[cursors.size() * 4];
        String[] captions = new String[cursors.size()];
        int kept = 0;
        for (int i = 0; i < cursors.size(); i++) {
            MapCursor cursor = cursors.getCursor(i);
            if (!cursor.isVisible()) {
                continue;
            }
            flat[kept * 4] = cursor.getX();
            flat[kept * 4 + 1] = cursor.getY();
            flat[kept * 4 + 2] = cursor.getDirection();
            flat[kept * 4 + 3] = typeId(cursor.getType());
            captions[kept] = cursor.caption() == null ? null : Json.of(cursor.caption());
            kept++;
        }
        int[] cursorIds = Arrays.copyOf(flat, kept * 4);
        int fingerprint = Arrays.hashCode(canvas.buffer()) * 31 + Arrays.hashCode(cursorIds)
            + Arrays.hashCode(Arrays.copyOf(captions, kept));
        Sent before = sent.get(player.getUniqueId());
        long now = BasaltServer.instance().getCurrentTick();
        if (before != null && before.fingerprint() == fingerprint && now - before.tick() < kResendTicks) {
            return;
        }
        sent.put(player.getUniqueId(), new Sent(fingerprint, now));
        MiscNatives.mapShow(player.getUniqueId().toString(), id, canvas.buffer(), cursorIds,
            Arrays.copyOf(captions, kept));
    }

    private static synchronized int typeId(MapCursor.Type type) {
        if (kCursorTypes == null) {
            kCursorTypes = List.of(WorldNatives.registryIds("map_decoration_type"));
        }
        int index = kCursorTypes.indexOf(type.getKey().toString());
        return index < 0 ? 0 : index;
    }

    @Override
    public boolean equals(Object other) {
        return other instanceof BasaltMapView view && view.id == id;
    }

    @Override
    public int hashCode() {
        return id;
    }
}
