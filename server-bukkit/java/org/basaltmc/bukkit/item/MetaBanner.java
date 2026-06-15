package org.basaltmc.bukkit.item;

import java.util.ArrayList;
import java.util.List;
import java.util.Map;
import org.bukkit.DyeColor;
import org.bukkit.Material;
import org.bukkit.NamespacedKey;
import org.bukkit.Registry;
import org.bukkit.block.banner.Pattern;
import org.bukkit.block.banner.PatternType;
import org.bukkit.configuration.serialization.DelegateDeserialization;
import org.bukkit.inventory.meta.BannerMeta;

@DelegateDeserialization(ItemMetas.Serial.class)
public class MetaBanner extends MetaItem implements BannerMeta {
    private static final String kPatterns = "minecraft:banner_patterns";

    public MetaBanner(Material type, Map<String, Object> components) {
        super(type, components);
    }

    @Override
    public MetaBanner clone() {
        return (MetaBanner) super.clone();
    }

    private List<Object> layers() {
        List<Object> list = Nbt.list(c.get(kPatterns));
        return list == null ? new ArrayList<>() : list;
    }

    private void store(List<Object> layers) {
        if (layers.isEmpty()) {
            c.remove(kPatterns);
        } else {
            c.put(kPatterns, layers);
        }
    }

    static Map<String, Object> layer(Pattern pattern) {
        Map<String, Object> out = Nbt.compound();
        out.put("color", Convert.dye(pattern.getColor()));
        out.put("pattern", pattern.getPattern().getKey().toString());
        return out;
    }

    static Pattern pattern(Object entry) {
        Map<String, Object> data = Nbt.map(entry);
        if (data == null) {
            return null;
        }
        NamespacedKey key = NamespacedKey.fromString(Nbt.string(data, "pattern", ""));
        PatternType type = key == null ? null : Registry.BANNER_PATTERN.get(key);
        DyeColor color = Convert.dye(Nbt.string(data, "color", "white"));
        return type == null || color == null ? null : new Pattern(color, type);
    }

    @Override
    public List<Pattern> getPatterns() {
        List<Pattern> out = new ArrayList<>();
        for (Object entry : layers()) {
            Pattern pattern = pattern(entry);
            if (pattern != null) {
                out.add(pattern);
            }
        }
        return out;
    }

    @Override
    public void setPatterns(List<Pattern> patterns) {
        List<Object> out = new ArrayList<>();
        for (Pattern pattern : patterns) {
            out.add(layer(pattern));
        }
        store(out);
    }

    @Override
    public void addPattern(Pattern pattern) {
        List<Object> out = layers();
        out.add(layer(pattern));
        store(out);
    }

    @Override
    public Pattern getPattern(int index) {
        return getPatterns().get(index);
    }

    @Override
    public Pattern removePattern(int index) {
        List<Pattern> patterns = getPatterns();
        Pattern removed = patterns.remove(index);
        setPatterns(patterns);
        return removed;
    }

    @Override
    public void setPattern(int index, Pattern pattern) {
        List<Pattern> patterns = getPatterns();
        patterns.set(index, pattern);
        setPatterns(patterns);
    }

    @Override
    public int numberOfPatterns() {
        return getPatterns().size();
    }
}
