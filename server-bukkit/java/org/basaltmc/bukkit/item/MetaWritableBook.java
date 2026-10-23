package org.basaltmc.bukkit.item;

import java.util.ArrayList;
import java.util.List;
import java.util.Map;
import org.bukkit.Material;
import org.bukkit.configuration.serialization.DelegateDeserialization;
import org.bukkit.inventory.meta.WritableBookMeta;

@DelegateDeserialization(ItemMetas.Serial.class)
public final class MetaWritableBook extends MetaItem implements WritableBookMeta {
    private static final String kContent = "minecraft:writable_book_content";

    public MetaWritableBook(Material type, Map<String, Object> components) {
        super(type, components);
    }

    @Override
    public MetaWritableBook clone() {
        return (MetaWritableBook) super.clone();
    }

    private List<Object> rawPages() {
        Map<String, Object> found = compoundOf(kContent);
        List<Object> list = found == null ? null : Nbt.list(found.get("pages"));
        return list == null ? new ArrayList<>() : list;
    }

    private void store(List<Object> pages) {
        Map<String, Object> found = Nbt.compound();
        found.put("pages", pages);
        c.put(kContent, found);
    }

    private static String text(Object page) {
        Map<String, Object> map = Nbt.map(page);
        Object raw = map != null && map.containsKey("raw") ? map.get("raw") : page;
        return String.valueOf(raw);
    }

    @Override
    public net.kyori.adventure.inventory.Book asBook() {
        List<net.kyori.adventure.text.Component> pages = new ArrayList<>();
        for (String page : getPages()) {
            pages.add(net.kyori.adventure.text.Component.text(page));
        }
        return net.kyori.adventure.inventory.Book.book(net.kyori.adventure.text.Component.empty(),
            net.kyori.adventure.text.Component.empty(), pages);
    }

    @Override
    public boolean hasPages() {
        return !rawPages().isEmpty();
    }

    @Override
    public String getPage(int page) {
        List<Object> pages = rawPages();
        check(page, pages.size());
        return text(pages.get(page - 1));
    }

    @Override
    public void setPage(int page, String data) {
        List<Object> pages = rawPages();
        check(page, pages.size());
        pages.set(page - 1, data);
        store(pages);
    }

    private static void check(int page, int size) {
        if (page < 1 || page > size) {
            throw new IllegalArgumentException("Invalid page number " + page + "/" + size);
        }
    }

    @Override
    public List<String> getPages() {
        List<String> out = new ArrayList<>();
        for (Object page : rawPages()) {
            out.add(text(page));
        }
        return out;
    }

    @Override
    public void setPages(List<String> pages) {
        store(new ArrayList<Object>(pages));
    }

    @Override
    public void setPages(String... pages) {
        setPages(List.of(pages));
    }

    @Override
    public void addPage(String... pages) {
        List<Object> stored = rawPages();
        stored.addAll(List.of(pages));
        store(stored);
    }

    @Override
    public int getPageCount() {
        return rawPages().size();
    }
}
