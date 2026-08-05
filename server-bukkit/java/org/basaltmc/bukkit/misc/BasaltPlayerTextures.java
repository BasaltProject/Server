package org.basaltmc.bukkit.misc;

import com.destroystokyo.paper.profile.ProfileProperty;
import com.google.gson.JsonObject;
import com.google.gson.JsonParser;
import java.net.MalformedURLException;
import java.net.URI;
import java.net.URL;
import java.nio.charset.StandardCharsets;
import java.util.Base64;
import org.bukkit.profile.PlayerTextures;

public final class BasaltPlayerTextures implements PlayerTextures {
    private final BasaltPlayerProfile owner;
    private URL skin;
    private SkinModel model = SkinModel.CLASSIC;
    private URL cape;
    private long timestamp;
    private boolean signed;

    BasaltPlayerTextures(BasaltPlayerProfile owner) {
        this.owner = owner;
    }

    void load(ProfileProperty property) {
        skin = null;
        cape = null;
        model = SkinModel.CLASSIC;
        timestamp = 0;
        signed = property != null && property.isSigned();
        if (property == null) {
            return;
        }
        try {
            String json = new String(Base64.getDecoder().decode(property.getValue()), StandardCharsets.UTF_8);
            JsonObject root = JsonParser.parseString(json).getAsJsonObject();
            timestamp = root.has("timestamp") ? root.get("timestamp").getAsLong() : 0;
            JsonObject textures = root.has("textures") ? root.getAsJsonObject("textures") : null;
            if (textures == null) {
                return;
            }
            if (textures.has("SKIN")) {
                JsonObject entry = textures.getAsJsonObject("SKIN");
                skin = URI.create(entry.get("url").getAsString()).toURL();
                if (entry.has("metadata") && entry.getAsJsonObject("metadata").has("model")
                        && "slim".equals(entry.getAsJsonObject("metadata").get("model").getAsString())) {
                    model = SkinModel.SLIM;
                }
            }
            if (textures.has("CAPE")) {
                cape = URI.create(textures.getAsJsonObject("CAPE").get("url").getAsString()).toURL();
            }
        } catch (RuntimeException | MalformedURLException e) {
            skin = null;
            cape = null;
        }
    }

    private void store() {
        signed = false;
        if (isEmpty()) {
            owner.removePropertyQuiet("textures");
            return;
        }
        JsonObject root = new JsonObject();
        root.addProperty("timestamp", timestamp == 0 ? System.currentTimeMillis() : timestamp);
        if (owner.getId() != null) {
            root.addProperty("profileId", owner.getId().toString().replace("-", ""));
        }
        if (owner.getName() != null) {
            root.addProperty("profileName", owner.getName());
        }
        JsonObject textures = new JsonObject();
        if (skin != null) {
            JsonObject entry = new JsonObject();
            entry.addProperty("url", skin.toString());
            if (model == SkinModel.SLIM) {
                JsonObject metadata = new JsonObject();
                metadata.addProperty("model", "slim");
                entry.add("metadata", metadata);
            }
            textures.add("SKIN", entry);
        }
        if (cape != null) {
            JsonObject entry = new JsonObject();
            entry.addProperty("url", cape.toString());
            textures.add("CAPE", entry);
        }
        root.add("textures", textures);
        String value = Base64.getEncoder().encodeToString(root.toString().getBytes(StandardCharsets.UTF_8));
        owner.setPropertyQuiet(new ProfileProperty("textures", value));
    }

    @Override
    public boolean isEmpty() {
        return skin == null && cape == null;
    }

    @Override
    public void clear() {
        skin = null;
        cape = null;
        model = SkinModel.CLASSIC;
        store();
    }

    @Override
    public URL getSkin() {
        return skin;
    }

    @Override
    public void setSkin(URL skinUrl) {
        setSkin(skinUrl, SkinModel.CLASSIC);
    }

    @Override
    public void setSkin(URL skinUrl, SkinModel skinModel) {
        skin = skinUrl;
        model = skinModel == null ? SkinModel.CLASSIC : skinModel;
        store();
    }

    @Override
    public SkinModel getSkinModel() {
        return model;
    }

    @Override
    public URL getCape() {
        return cape;
    }

    @Override
    public void setCape(URL capeUrl) {
        cape = capeUrl;
        store();
    }

    @Override
    public long getTimestamp() {
        return timestamp;
    }

    @Override
    public boolean isSigned() {
        return signed;
    }

    void copyFrom(BasaltPlayerTextures other) {
        skin = other.skin;
        cape = other.cape;
        model = other.model;
        timestamp = other.timestamp;
        signed = other.signed;
    }
}
