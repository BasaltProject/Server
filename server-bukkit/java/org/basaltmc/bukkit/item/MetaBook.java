package org.basaltmc.bukkit.item;

import java.util.ArrayList;
import java.util.List;
import java.util.Map;
import net.kyori.adventure.text.Component;
import org.bukkit.Material;
import org.bukkit.configuration.serialization.DelegateDeserialization;
import org.bukkit.inventory.meta.BookMeta;

@DelegateDeserialization(ItemMetas.Serial.class)
public final class MetaBook extends MetaItem implements BookMeta {
    private static final String kContent = "minecraft:written_book_content";

    public MetaBook(Material type, Map<String, Object> components) {
        super(type, components);
    }

    @Override
    public MetaBook clone() {
        return (MetaBook) super.clone();
    }

    private Map<String, Object> content(boolean create) {
        Map<String, Object> found = compoundOf(kContent);
        if (found == null && create) {
            found = Nbt.compound();
            found.put("title", "");
            found.put("author", "");
            c.put(kContent, found);
        }
        return found;
    }

    private static Object raw(Object filterable) {
        Map<String, Object> map = Nbt.map(filterable);
        return map != null && map.containsKey("raw") ? map.get("raw") : filterable;
    }

    @Override
    public boolean hasPages() {
        return !rawPages().isEmpty();
    }

    @Override
    public net.kyori.adventure.inventory.Book asBook() {
        return net.kyori.adventure.inventory.Book.book(title() == null ? Component.empty() : title(),
            author() == null ? Component.empty() : author(), pages());
    }

    @Override
    public boolean hasTitle() {
        Map<String, Object> found = content(false);
        return found != null && raw(found.get("title")) instanceof String text && !text.isEmpty();
    }

    @Override
    public String getTitle() {
        Map<String, Object> found = content(false);
        return found != null && raw(found.get("title")) instanceof String text && !text.isEmpty() ? text : null;
    }

    @Override
    public boolean setTitle(String title) {
        if (title == null) {
            Map<String, Object> found = content(false);
            if (found != null) {
                found.put("title", "");
            }
            return true;
        }
        if (title.length() > 32) {
            return false;
        }
        content(true).put("title", title);
        return true;
    }

    @Override
    public Component title() {
        return hasTitle() ? Component.text(getTitle()) : null;
    }

    @Override
    public BookMeta title(Component title) {
        setTitle(title == null ? null : Texts.toLegacy(title));
        return this;
    }

    @Override
    public boolean hasAuthor() {
        Map<String, Object> found = content(false);
        return found != null && found.get("author") instanceof String text && !text.isEmpty();
    }

    @Override
    public String getAuthor() {
        return hasAuthor() ? (String) content(false).get("author") : null;
    }

    @Override
    public void setAuthor(String author) {
        content(true).put("author", author == null ? "" : author);
    }

    @Override
    public Component author() {
        return hasAuthor() ? Texts.fromLegacy(getAuthor()) : null;
    }

    @Override
    public BookMeta author(Component author) {
        setAuthor(author == null ? null : Texts.toLegacy(author));
        return this;
    }

    @Override
    public boolean hasGeneration() {
        Map<String, Object> found = content(false);
        return found != null && found.containsKey("generation");
    }

    @Override
    public Generation getGeneration() {
        Map<String, Object> found = content(false);
        if (found == null || !found.containsKey("generation")) {
            return null;
        }
        int ordinal = Nbt.integer(found, "generation", 0);
        return Generation.values()[Math.max(0, Math.min(ordinal, Generation.values().length - 1))];
    }

    @Override
    public void setGeneration(Generation generation) {
        if (generation == null) {
            Map<String, Object> found = content(false);
            if (found != null) {
                found.remove("generation");
            }
            return;
        }
        content(true).put("generation", generation.ordinal());
    }

    private List<Object> rawPages() {
        Map<String, Object> found = content(false);
        List<Object> list = found == null ? null : Nbt.list(found.get("pages"));
        return list == null ? new ArrayList<>() : list;
    }

    private void storePages(List<Object> pages) {
        content(true).put("pages", pages);
    }

    @Override
    public List<Component> pages() {
        List<Component> out = new ArrayList<>();
        for (Object page : rawPages()) {
            out.add(Texts.fromNbt(raw(page)));
        }
        return out;
    }

    @Override
    public BookMeta pages(List<Component> pages) {
        List<Object> stored = new ArrayList<>();
        for (Component page : pages) {
            stored.add(Texts.toNbt(page));
        }
        storePages(stored);
        return this;
    }

    @Override
    public Component page(int page) {
        List<Object> pages = rawPages();
        checkPage(page, pages.size());
        return Texts.fromNbt(raw(pages.get(page - 1)));
    }

    @Override
    public void page(int page, Component data) {
        List<Object> pages = rawPages();
        checkPage(page, pages.size());
        pages.set(page - 1, Texts.toNbt(data));
        storePages(pages);
    }

    @Override
    public void addPages(Component... pages) {
        List<Object> stored = rawPages();
        for (Component page : pages) {
            stored.add(Texts.toNbt(page));
        }
        storePages(stored);
    }

    private static void checkPage(int page, int size) {
        if (page < 1 || page > size) {
            throw new IllegalArgumentException("Invalid page number " + page + "/" + size);
        }
    }

    @Override
    public String getPage(int page) {
        return Texts.toLegacy(page(page));
    }

    @Override
    public void setPage(int page, String text) {
        page(page, Texts.fromLegacy(text));
    }

    @Override
    public List<String> getPages() {
        List<String> out = new ArrayList<>();
        for (Component page : pages()) {
            out.add(Texts.toLegacy(page));
        }
        return out;
    }

    @Override
    public void setPages(List<String> pages) {
        List<Component> components = new ArrayList<>();
        for (String page : pages) {
            components.add(Texts.fromLegacy(page));
        }
        pages(components);
    }

    @Override
    public void setPages(String... pages) {
        setPages(List.of(pages));
    }

    @Override
    public void addPage(String... pages) {
        Component[] components = new Component[pages.length];
        for (int at = 0; at < pages.length; at++) {
            components[at] = Texts.fromLegacy(pages[at]);
        }
        addPages(components);
    }

    @Override
    public int getPageCount() {
        return rawPages().size();
    }

    @Override
    public BookMeta.Spigot spigot() {
        return new BookMeta.Spigot() {
            @Override
            public net.md_5.bungee.api.chat.BaseComponent[] getPage(int page) {
                return bungee(page(page));
            }

            @Override
            public void setPage(int page, net.md_5.bungee.api.chat.BaseComponent... data) {
                page(page, unbungee(data));
            }

            @Override
            public List<net.md_5.bungee.api.chat.BaseComponent[]> getPages() {
                List<net.md_5.bungee.api.chat.BaseComponent[]> out = new ArrayList<>();
                for (Component page : pages()) {
                    out.add(bungee(page));
                }
                return out;
            }

            @Override
            public void setPages(List<net.md_5.bungee.api.chat.BaseComponent[]> pages) {
                List<Component> components = new ArrayList<>();
                for (net.md_5.bungee.api.chat.BaseComponent[] page : pages) {
                    components.add(unbungee(page));
                }
                pages(components);
            }

            @Override
            public void setPages(net.md_5.bungee.api.chat.BaseComponent[]... pages) {
                setPages(List.of(pages));
            }

            @Override
            public void addPage(net.md_5.bungee.api.chat.BaseComponent[]... pages) {
                for (net.md_5.bungee.api.chat.BaseComponent[] page : pages) {
                    addPages(unbungee(page));
                }
            }
        };
    }
}
