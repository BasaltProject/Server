// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.saveddata.maps;
import com.mojang.serialization.Codec;
import net.minecraft.world.level.saveddata.SavedData;
import net.minecraft.world.level.saveddata.SavedDataType;
public class MapIndex extends SavedData {
    private int lastMapId = -1;
    public static final Codec<MapIndex> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.level.saveddata.maps.MapIndex.class, "CODEC");
    public static final SavedDataType<MapIndex> TYPE = new SavedDataType<>(net.minecraft.resources.Identifier.withDefaultNamespace("maps/last_id"), MapIndex::new, CODEC, net.minecraft.util.datafix.DataFixTypes.SAVED_DATA_MAP_INDEX);
    public MapIndex() {
    }
    public MapIndex(int lastMapId) {
        this.lastMapId = lastMapId;
    }
    public MapId getNextMapId() {
        MapId next = new MapId(++lastMapId);
        setDirty();
        return next;
    }
}
