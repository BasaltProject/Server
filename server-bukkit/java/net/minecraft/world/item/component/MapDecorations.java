// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.item.component;
import com.mojang.serialization.Codec;
import java.util.Map;
import net.minecraft.core.Holder;
import net.minecraft.world.level.saveddata.maps.MapDecorationType;
public record MapDecorations(Map<String, Entry> decorations) {
    public static final MapDecorations EMPTY = new MapDecorations(java.util.Map.of());
    public static final Codec<MapDecorations> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.item.component.MapDecorations.class, "CODEC");
    public MapDecorations withDecoration(String id, Entry entry) {
        Map<String, Entry> copy = new java.util.LinkedHashMap<>(decorations);
        copy.put(id, entry);
        return new MapDecorations(java.util.Collections.unmodifiableMap(copy));
    }
    public static record Entry(Holder<MapDecorationType> type, double x, double z, float rotation) {
        public static final Codec<Entry> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.item.component.MapDecorations.Entry.class, "CODEC");
    }
}
