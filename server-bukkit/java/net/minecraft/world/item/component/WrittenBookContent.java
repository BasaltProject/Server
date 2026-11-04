// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.item.component;
import com.mojang.serialization.Codec;
import java.util.List;
import java.util.function.Consumer;
import net.minecraft.core.HolderLookup;
import net.minecraft.core.component.DataComponentGetter;
import net.minecraft.network.RegistryFriendlyByteBuf;
import net.minecraft.network.chat.Component;
import net.minecraft.network.chat.ResolutionContext;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.server.network.Filterable;
import net.minecraft.world.item.Item;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.item.TooltipFlag;
public record WrittenBookContent(Filterable<String> title, String author, int generation, List<Filterable<Component>> pages, boolean resolved) implements BookContent<Component, WrittenBookContent>, TooltipProvider {
    public static final WrittenBookContent EMPTY = null;
    public static final int PAGE_LENGTH = 32767;
    public static final int TITLE_LENGTH = 16;
    public static final int TITLE_MAX_LENGTH = 32;
    public static final int MAX_GENERATION = 3;
    public static final Codec<Component> CONTENT_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.item.component.WrittenBookContent.class, "CONTENT_CODEC");
    public static final Codec<List<Filterable<Component>>> PAGES_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.item.component.WrittenBookContent.class, "PAGES_CODEC");
    public static final Codec<WrittenBookContent> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.item.component.WrittenBookContent.class, "CODEC");
    public static final StreamCodec<RegistryFriendlyByteBuf, WrittenBookContent> STREAM_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.streamCodec(net.minecraft.world.item.component.WrittenBookContent.class, "STREAM_CODEC");
    public WrittenBookContent(Filterable<String> title, String author, int generation, List<Filterable<Component>> pages, boolean resolved) {
        this.title = title;
        this.author = author;
        this.generation = generation;
        this.pages = pages;
        this.resolved = resolved;
    }
    public static Codec<List<Filterable<Component>>> pagesCodec(Codec<Component> contentCodec) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.item.component.WrittenBookContent#pagesCodec");
    }
    public WrittenBookContent craftCopy() {
        return new WrittenBookContent(title, author, generation + 1, pages, resolved);
    }
    public static boolean resolveForItem(ItemStack itemStack, ResolutionContext context, HolderLookup.Provider registries) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.item.component.WrittenBookContent#resolveForItem");
    }
    public WrittenBookContent resolve(ResolutionContext context, HolderLookup.Provider registries) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.item.component.WrittenBookContent#resolve");
    }
    public WrittenBookContent markResolved() {
        return new WrittenBookContent(title, author, generation, pages, true);
    }
    public List<Component> getPages(boolean filterEnabled) {
        List<Component> out = new java.util.ArrayList<>(pages.size());
        for (Filterable<Component> page : pages) {
            out.add(page.get(filterEnabled));
        }
        return out;
    }
    public WrittenBookContent withReplacedPages(List<Filterable<Component>> newPages) {
        return new WrittenBookContent(title, author, generation, newPages, false);
    }
    public void addToTooltip(Item.TooltipContext context, Consumer<Component> consumer, TooltipFlag flag, DataComponentGetter components) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.item.component.WrittenBookContent#addToTooltip");
    }
}
