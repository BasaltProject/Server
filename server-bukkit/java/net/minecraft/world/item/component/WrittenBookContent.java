// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.item.component;
import com.mojang.serialization.Codec;
import java.util.List;
import java.util.Optional;
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
    public static final WrittenBookContent EMPTY = new WrittenBookContent(net.minecraft.server.network.Filterable.passThrough(""), "", 0, List.of(), true);
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
        return net.minecraft.server.network.Filterable.codec(contentCodec).listOf();
    }
    public WrittenBookContent craftCopy() {
        return new WrittenBookContent(title, author, generation + 1, pages, resolved);
    }
    public static boolean resolveForItem(ItemStack itemStack, ResolutionContext context, HolderLookup.Provider registries) {
        WrittenBookContent content = itemStack.get(net.minecraft.core.component.DataComponents.WRITTEN_BOOK_CONTENT);
        if (content == null || content.resolved()) {
            return false;
        }
        WrittenBookContent resolved = content.resolve(context, registries);
        if (resolved == null) {
            return false;
        }
        itemStack.set(net.minecraft.core.component.DataComponents.WRITTEN_BOOK_CONTENT, resolved);
        return true;
    }
    public WrittenBookContent resolve(ResolutionContext context, HolderLookup.Provider registries) {
        if (resolved) {
            return this;
        }
        List<Filterable<Component>> out = new java.util.ArrayList<>(pages.size());
        for (Filterable<Component> page : pages) {
            Optional<Filterable<Component>> resolvedPage = page.resolve(component -> resolveOne(context, component));
            if (resolvedPage.isEmpty()) {
                return null;
            }
            out.add(resolvedPage.get());
        }
        return new WrittenBookContent(title, author, generation, out, true);
    }
    private static Optional<Component> resolveOne(ResolutionContext context, Component component) {
        try {
            return Optional.of(net.minecraft.network.chat.ComponentUtils.resolve(context, component));
        } catch (com.mojang.brigadier.exceptions.CommandSyntaxException e) {
            return Optional.empty();
        }
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
        if (author != null && !author.isBlank()) {
            consumer.accept(Component.translatable("book.byAuthor", author).withStyle(net.minecraft.ChatFormatting.GRAY));
        }
        consumer.accept(Component.translatable("book.generation." + generation).withStyle(net.minecraft.ChatFormatting.GRAY));
    }
}
