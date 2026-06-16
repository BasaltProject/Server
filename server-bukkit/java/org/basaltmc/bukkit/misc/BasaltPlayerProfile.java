package org.basaltmc.bukkit.misc;

import com.destroystokyo.paper.profile.PlayerProfile;
import com.destroystokyo.paper.profile.ProfileProperty;
import java.util.Collection;
import java.util.HashMap;
import java.util.LinkedHashMap;
import java.util.Map;
import java.util.Set;
import java.util.UUID;
import java.util.concurrent.CompletableFuture;
import org.bukkit.configuration.serialization.ConfigurationSerializable;
import org.bukkit.configuration.serialization.SerializableAs;
import org.bukkit.profile.PlayerTextures;

@SerializableAs("PlayerProfile")
public final class BasaltPlayerProfile implements PlayerProfile, ConfigurationSerializable {
    private UUID id;
    private String name;
    private final Map<String, ProfileProperty> properties = new LinkedHashMap<>();
    private final BasaltPlayerTextures textures = new BasaltPlayerTextures(this);

    public BasaltPlayerProfile(UUID id, String name) {
        this.id = id;
        this.name = name;
    }

    public static BasaltPlayerProfile deserialize(Map<String, Object> map) {
        UUID id = map.get("uniqueId") == null ? null : UUID.fromString((String) map.get("uniqueId"));
        BasaltPlayerProfile profile = new BasaltPlayerProfile(id, (String) map.get("name"));
        Object stored = map.get("properties");
        if (stored instanceof java.util.List<?> list) {
            for (Object item : list) {
                if (item instanceof Map<?, ?> entry) {
                    profile.setProperty(new ProfileProperty((String) entry.get("name"), (String) entry.get("value"),
                        (String) entry.get("signature")));
                }
            }
        }
        return profile;
    }

    @Override
    public String getName() {
        return name;
    }

    @Override
    public String setName(String name) {
        String old = this.name;
        this.name = name;
        return old;
    }

    @Override
    public UUID getId() {
        return id;
    }

    @Override
    public UUID getUniqueId() {
        return id;
    }

    @Override
    public UUID setId(UUID uuid) {
        UUID old = id;
        id = uuid;
        return old;
    }

    @Override
    public PlayerTextures getTextures() {
        return textures;
    }

    @Override
    public void setTextures(PlayerTextures other) {
        if (other == null) {
            textures.clear();
        } else if (other instanceof BasaltPlayerTextures copy) {
            textures.copyFrom(copy);
            textures.setSkin(copy.getSkin(), copy.getSkinModel());
            textures.setCape(copy.getCape());
        } else {
            textures.setSkin(other.getSkin(), other.getSkinModel());
            textures.setCape(other.getCape());
        }
    }

    @Override
    public Set<ProfileProperty> getProperties() {
        return Set.copyOf(properties.values());
    }

    @Override
    public boolean hasProperty(String property) {
        return property != null && properties.containsKey(property);
    }

    @Override
    public void setProperty(ProfileProperty property) {
        properties.put(property.getName(), property);
        if (property.getName().equals("textures")) {
            textures.load(property);
        }
    }

    void setPropertyQuiet(ProfileProperty property) {
        properties.put(property.getName(), property);
    }

    void removePropertyQuiet(String name) {
        properties.remove(name);
    }

    @Override
    public void setProperties(Collection<ProfileProperty> properties) {
        for (ProfileProperty property : properties) {
            setProperty(property);
        }
    }

    @Override
    public boolean removeProperty(String property) {
        if ("textures".equals(property)) {
            textures.load(null);
        }
        return property != null && properties.remove(property) != null;
    }

    @Override
    public void clearProperties() {
        properties.clear();
        textures.load(null);
    }

    @Override
    public boolean isComplete() {
        return id != null && name != null;
    }

    @Override
    public boolean completeFromCache() {
        return completeFromCache(true);
    }

    @Override
    public boolean completeFromCache(boolean onlineMode) {
        return completeFromCache(true, onlineMode);
    }

    @Override
    public boolean completeFromCache(boolean lookupUUID, boolean onlineMode) {
        Access.Known known = id != null ? Access.byUuid(id) : name != null && lookupUUID ? Access.byName(name) : null;
        if (known == null) {
            return isComplete();
        }
        if (id == null) {
            id = known.id();
        }
        if (name == null) {
            name = known.name();
        }
        return isComplete();
    }

    @Override
    public boolean complete(boolean textures) {
        return complete(textures, true);
    }

    @Override
    public boolean complete(boolean textures, boolean onlineMode) {
        if (!completeFromCache(true, onlineMode) && id == null && name != null) {
            id = Access.offlineUuid(name);
        }
        return isComplete();
    }

    @Override
    public CompletableFuture<PlayerProfile> update() {
        BasaltPlayerProfile copy = clone();
        copy.complete(true);
        return CompletableFuture.completedFuture(copy);
    }

    @Override
    public BasaltPlayerProfile clone() {
        BasaltPlayerProfile copy = new BasaltPlayerProfile(id, name);
        copy.properties.putAll(properties);
        copy.textures.copyFrom(textures);
        return copy;
    }

    @Override
    public Map<String, Object> serialize() {
        Map<String, Object> map = new HashMap<>();
        if (id != null) {
            map.put("uniqueId", id.toString());
        }
        if (name != null) {
            map.put("name", name);
        }
        if (!properties.isEmpty()) {
            java.util.List<Map<String, Object>> list = new java.util.ArrayList<>();
            for (ProfileProperty property : properties.values()) {
                Map<String, Object> entry = new HashMap<>();
                entry.put("name", property.getName());
                entry.put("value", property.getValue());
                if (property.getSignature() != null) {
                    entry.put("signature", property.getSignature());
                }
                list.add(entry);
            }
            map.put("properties", list);
        }
        return map;
    }

    @Override
    public boolean equals(Object other) {
        return other instanceof BasaltPlayerProfile profile && java.util.Objects.equals(id, profile.id)
            && java.util.Objects.equals(name, profile.name) && properties.equals(profile.properties);
    }

    @Override
    public int hashCode() {
        return java.util.Objects.hash(id, name);
    }

    @Override
    public String toString() {
        return "BasaltPlayerProfile{id=" + id + ", name=" + name + "}";
    }
}
