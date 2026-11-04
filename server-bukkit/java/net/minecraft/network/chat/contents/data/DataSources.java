// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.network.chat.contents.data;
import com.mojang.serialization.MapCodec;
import net.minecraft.network.chat.ComponentSerialization;
import net.minecraft.util.ExtraCodecs;
public class DataSources {
    private static final ExtraCodecs.LateBoundIdMapper<String, MapCodec<? extends DataSource>> ID_MAPPER = new ExtraCodecs.LateBoundIdMapper<>();
    public static final MapCodec<DataSource> CODEC;
    static {
        ID_MAPPER.put("entity", EntityDataSource.MAP_CODEC);
        ID_MAPPER.put("block", BlockDataSource.MAP_CODEC);
        ID_MAPPER.put("storage", StorageDataSource.MAP_CODEC);
        CODEC = ComponentSerialization.createLegacyComponentMatcher(ID_MAPPER, DataSource::codec, "source");
    }
}
