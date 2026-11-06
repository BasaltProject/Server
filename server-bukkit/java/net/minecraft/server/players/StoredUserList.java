// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.server.players;

import com.google.gson.Gson;
import com.google.gson.GsonBuilder;
import com.google.gson.JsonArray;
import com.google.gson.JsonElement;
import com.google.gson.JsonObject;
import java.io.File;
import java.io.IOException;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.util.ArrayList;
import java.util.Collection;
import java.util.LinkedHashMap;
import java.util.Map;
import net.minecraft.server.notifications.NotificationService;

public abstract class StoredUserList<K, V extends StoredUserEntry<K>> {
    private static final Gson kGson = new GsonBuilder().setPrettyPrinting().create();
    private final File file;
    private final Map<String, V> map = new LinkedHashMap<>();
    protected final NotificationService notificationService;

    public StoredUserList(File file, NotificationService notificationService) {
        this.file = file;
        this.notificationService = notificationService;
    }

    public File getFile() {
        return file;
    }

    public boolean add(V infos) {
        String key = getKeyForUser(infos.getUser());
        V previous = map.put(key, infos);
        return !infos.equals(previous);
    }

    public final void add0(V entry) {
        map.put(getKeyForUser(entry.getUser()), entry);
    }

    public final void clear0() {
        map.clear();
    }

    public V get(K user) {
        removeExpired();
        return map.get(getKeyForUser(user));
    }

    public boolean remove(K user) {
        return map.remove(getKeyForUser(user)) != null;
    }

    public boolean remove(StoredUserEntry<K> infos) {
        return map.remove(getKeyForUser(infos.getUser())) != null;
    }

    public void clear() {
        map.clear();
    }

    public String[] getUserList() {
        return map.keySet().toArray(new String[0]);
    }

    public boolean isEmpty() {
        return map.isEmpty();
    }

    protected String getKeyForUser(K user) {
        return user.toString();
    }

    protected boolean contains(K user) {
        return map.containsKey(getKeyForUser(user));
    }

    protected abstract StoredUserEntry<K> createEntry(JsonObject object);

    public Collection<V> getEntries() {
        removeExpired();
        return new ArrayList<>(map.values());
    }

    private void removeExpired() {
        map.values().removeIf(StoredUserEntry::hasExpired);
    }

    public void save() throws IOException {
        JsonArray array = new JsonArray();
        for (V entry : map.values()) {
            JsonObject object = new JsonObject();
            entry.serialize(object);
            array.add(object);
        }
        Files.writeString(file.toPath(), kGson.toJson(array), StandardCharsets.UTF_8);
    }

    @SuppressWarnings("unchecked")
    public void load() throws IOException {
        if (!file.exists()) {
            return;
        }
        map.clear();
        for (JsonElement element : kGson.fromJson(Files.readString(file.toPath(), StandardCharsets.UTF_8), JsonArray.class)) {
            StoredUserEntry<K> entry = createEntry(element.getAsJsonObject());
            if (entry != null && entry.getUser() != null) {
                map.put(getKeyForUser(entry.getUser()), (V) entry);
            }
        }
    }
}
