// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft;
import com.google.gson.JsonObject;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.nio.charset.StandardCharsets;
import java.time.ZonedDateTime;
import java.util.Date;
import java.util.UUID;
import net.minecraft.server.packs.metadata.pack.PackFormat;
import net.minecraft.util.GsonHelper;
import net.minecraft.world.level.storage.DataVersion;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
public class DetectedVersion {
    private static final Logger LOGGER = LoggerFactory.getLogger(DetectedVersion.class);
    public static final WorldVersion BUILT_IN = createBuiltIn(UUID.randomUUID().toString().replace("-", ""), "Development Version");
    public static WorldVersion createBuiltIn(String id, String name) {
        return createBuiltIn(id, name, true);
    }
    public static WorldVersion createBuiltIn(String id, String name, boolean stable) {
        return new WorldVersion.Simple(id, name, new DataVersion(5023, "main"), SharedConstants.getProtocolVersion(),
                PackFormat.of(97, 1), PackFormat.of(121, 0), new Date(), stable);
    }
    private static WorldVersion fromJson(JsonObject root) {
        JsonObject pack = GsonHelper.getAsJsonObject(root, "pack_version");
        return new WorldVersion.Simple(GsonHelper.getAsString(root, "id"), GsonHelper.getAsString(root, "name"),
                new DataVersion(GsonHelper.getAsInt(root, "world_version"), GsonHelper.getAsString(root, "series_id", "main")),
                GsonHelper.getAsInt(root, "protocol_version"),
                PackFormat.of(GsonHelper.getAsInt(pack, "resource_major"), GsonHelper.getAsInt(pack, "resource_minor")),
                PackFormat.of(GsonHelper.getAsInt(pack, "data_major"), GsonHelper.getAsInt(pack, "data_minor")),
                Date.from(ZonedDateTime.parse(GsonHelper.getAsString(root, "build_time")).toInstant()), GsonHelper.getAsBoolean(root, "stable"));
    }
    public static WorldVersion tryDetectVersion() {
        try (InputStream in = DetectedVersion.class.getResourceAsStream("/version.json")) {
            if (in == null) {
                LOGGER.warn("Missing version information!");
                return BUILT_IN;
            }
            try (InputStreamReader reader = new InputStreamReader(in, StandardCharsets.UTF_8)) {
                return fromJson(GsonHelper.parse(reader));
            }
        } catch (RuntimeException | IOException e) {
            throw new IllegalStateException("Game version information is corrupt", e);
        }
    }
}
