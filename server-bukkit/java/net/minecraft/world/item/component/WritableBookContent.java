// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.item.component;
import com.mojang.serialization.Codec;
import io.netty.buffer.ByteBuf;
import java.util.List;
import java.util.stream.Stream;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.server.network.Filterable;
public record WritableBookContent(List<Filterable<String>> pages) implements BookContent<String, WritableBookContent> {
    public static final WritableBookContent EMPTY = new WritableBookContent(List.of());
    public static final int PAGE_EDIT_LENGTH = 1024;
    public static final int MAX_PAGES = 100;
    public static final Codec<List<Filterable<String>>> PAGES_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.item.component.WritableBookContent.class, "PAGES_CODEC");
    public static final Codec<WritableBookContent> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.item.component.WritableBookContent.class, "CODEC");
    public static final StreamCodec<ByteBuf, WritableBookContent> STREAM_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.streamCodec(net.minecraft.world.item.component.WritableBookContent.class, "STREAM_CODEC");
    public Stream<String> getPages(boolean filterEnabled) {
        return pages.stream().map(page -> page.get(filterEnabled));
    }
    public WritableBookContent withReplacedPages(List<Filterable<String>> newPages) {
        return new WritableBookContent(newPages);
    }
}
