package org.basaltmc.bukkit.item;

import com.destroystokyo.paper.profile.ProfileProperty;
import java.util.ArrayList;
import java.util.List;
import java.util.Map;
import java.util.UUID;
import org.bukkit.Bukkit;
import org.bukkit.Material;
import org.bukkit.NamespacedKey;
import org.bukkit.OfflinePlayer;
import org.bukkit.configuration.serialization.DelegateDeserialization;
import org.bukkit.inventory.meta.SkullMeta;

@DelegateDeserialization(ItemMetas.Serial.class)
public final class MetaSkull extends MetaItem implements SkullMeta {
    private static final String kProfile = "minecraft:profile";
    private static final String kSound = "minecraft:note_block_sound";

    public MetaSkull(Material type, Map<String, Object> components) {
        super(type, components);
    }

    @Override
    public MetaSkull clone() {
        return (MetaSkull) super.clone();
    }

    private Map<String, Object> profile() {
        Object value = c.get(kProfile);
        if (value instanceof String name) {
            Map<String, Object> out = Nbt.compound();
            out.put("name", name);
            return out;
        }
        return Nbt.map(value);
    }

    @Override
    public boolean hasOwner() {
        Map<String, Object> profile = profile();
        return profile != null && profile.get("name") instanceof String;
    }

    @Override
    public String getOwner() {
        Map<String, Object> profile = profile();
        return profile == null ? null : Nbt.string(profile, "name", null);
    }

    @Override
    public boolean setOwner(String name) {
        if (name != null && name.length() > 16) {
            return false;
        }
        if (name == null) {
            c.remove(kProfile);
        } else {
            Map<String, Object> profile = Nbt.compound();
            profile.put("name", name);
            c.put(kProfile, profile);
        }
        return true;
    }

    @Override
    public OfflinePlayer getOwningPlayer() {
        Map<String, Object> profile = profile();
        if (profile == null) {
            return null;
        }
        UUID id = Convert.uuidOf(profile.get("id"));
        if (id != null) {
            return Bukkit.getOfflinePlayer(id);
        }
        String name = Nbt.string(profile, "name", null);
        return name == null ? null : Bukkit.getOfflinePlayer(name);
    }

    @Override
    public boolean setOwningPlayer(OfflinePlayer owner) {
        if (owner == null) {
            c.remove(kProfile);
            return true;
        }
        Map<String, Object> profile = Nbt.compound();
        if (owner.getName() != null) {
            profile.put("name", owner.getName());
        }
        profile.put("id", Convert.uuidToInts(owner.getUniqueId()));
        c.put(kProfile, profile);
        return true;
    }

    @Override
    public com.destroystokyo.paper.profile.PlayerProfile getPlayerProfile() {
        Map<String, Object> profile = profile();
        if (profile == null) {
            return null;
        }
        UUID id = Convert.uuidOf(profile.get("id"));
        com.destroystokyo.paper.profile.PlayerProfile out = Bukkit.createProfile(id, Nbt.string(profile, "name", null));
        List<Object> properties = Nbt.list(profile.get("properties"));
        if (properties != null) {
            for (Object entry : properties) {
                Map<String, Object> property = Nbt.map(entry);
                if (property != null) {
                    out.setProperty(new ProfileProperty(Nbt.string(property, "name", ""), Nbt.string(property, "value", ""),
                        Nbt.string(property, "signature", null)));
                }
            }
        }
        return out;
    }

    @Override
    public void setPlayerProfile(com.destroystokyo.paper.profile.PlayerProfile profile) {
        if (profile == null) {
            c.remove(kProfile);
            return;
        }
        Map<String, Object> out = Nbt.compound();
        if (profile.getName() != null) {
            out.put("name", profile.getName());
        }
        if (profile.getId() != null) {
            out.put("id", Convert.uuidToInts(profile.getId()));
        }
        List<Object> properties = new ArrayList<>();
        for (ProfileProperty property : profile.getProperties()) {
            Map<String, Object> entry = Nbt.compound();
            entry.put("name", property.getName());
            entry.put("value", property.getValue());
            if (property.getSignature() != null) {
                entry.put("signature", property.getSignature());
            }
            properties.add(entry);
        }
        if (!properties.isEmpty()) {
            out.put("properties", properties);
        }
        c.put(kProfile, out);
    }

    @Override
    public org.bukkit.profile.PlayerProfile getOwnerProfile() {
        return getPlayerProfile();
    }

    @Override
    public void setOwnerProfile(org.bukkit.profile.PlayerProfile profile) {
        if (profile instanceof com.destroystokyo.paper.profile.PlayerProfile paper) {
            setPlayerProfile(paper);
        } else if (profile == null) {
            c.remove(kProfile);
        } else {
            com.destroystokyo.paper.profile.PlayerProfile built = Bukkit.createProfile(profile.getUniqueId(), profile.getName());
            built.setTextures(profile.getTextures());
            setPlayerProfile(built);
        }
    }

    @Override
    public void setNoteBlockSound(NamespacedKey sound) {
        put(kSound, sound == null ? null : sound.toString());
    }

    @Override
    public NamespacedKey getNoteBlockSound() {
        return key(kSound);
    }
}
