// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.chunk.storage;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.util.zip.DeflaterOutputStream;
import java.util.zip.GZIPInputStream;
import java.util.zip.GZIPOutputStream;
import java.util.zip.InflaterInputStream;
public class RegionFileVersion {
    private static final java.util.Map<Integer, RegionFileVersion> kById = new java.util.HashMap<>();
    public static final RegionFileVersion VERSION_GZIP = new RegionFileVersion(1, "gzip", GZIPInputStream::new, GZIPOutputStream::new);
    public static final RegionFileVersion VERSION_DEFLATE = new RegionFileVersion(2, "deflate", InflaterInputStream::new, DeflaterOutputStream::new);
    public static final RegionFileVersion VERSION_NONE = new RegionFileVersion(3, "none", stream -> stream, stream -> stream);
    public static final RegionFileVersion VERSION_LZ4 = new RegionFileVersion(4, "lz4", stream -> stream, stream -> stream);
    public static final RegionFileVersion VERSION_CUSTOM = new RegionFileVersion(127, "custom", stream -> stream, stream -> stream);
    public static final RegionFileVersion DEFAULT = VERSION_DEFLATE;
    private static volatile RegionFileVersion selected = DEFAULT;
    private final int id;
    private final String optionName;
    private final StreamWrapper<InputStream> inputWrapper;
    private final StreamWrapper<OutputStream> outputWrapper;
    private RegionFileVersion(int id, String optionName, StreamWrapper<InputStream> inputWrapper, StreamWrapper<OutputStream> outputWrapper) {
        this.id = id;
        this.optionName = optionName;
        this.inputWrapper = inputWrapper;
        this.outputWrapper = outputWrapper;
        kById.put(id, this);
    }
    public static RegionFileVersion fromId(int id) {
        return kById.get(id);
    }
    public static void configure(String optionName) {
        for (RegionFileVersion each : kById.values()) {
            if (each.optionName.equals(optionName)) {
                selected = each;
                return;
            }
        }
        selected = DEFAULT;
    }
    public static RegionFileVersion getSelected() {
        return selected;
    }
    public static boolean isValidVersion(int version) {
        return kById.containsKey(version);
    }
    public int getId() {
        return id;
    }
    public OutputStream wrap(OutputStream is) throws IOException {
        return outputWrapper.wrap(is);
    }
    public InputStream wrap(InputStream is) throws IOException {
        return inputWrapper.wrap(is);
    }
    interface StreamWrapper<O> {
        O wrap(O stream) throws IOException;
    }
}
