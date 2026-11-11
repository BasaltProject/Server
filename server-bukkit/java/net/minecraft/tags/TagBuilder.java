// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.tags;

import java.util.ArrayList;
import java.util.List;
import net.minecraft.resources.Identifier;

public class TagBuilder {
    private final List<TagEntry> entries = new ArrayList<>();
    private boolean replace;

    public static TagBuilder create() {
        return new TagBuilder();
    }

    public List<TagEntry> build() {
        return List.copyOf(entries);
    }

    public boolean shouldReplace() {
        return replace;
    }

    public TagBuilder setReplace(boolean replace) {
        this.replace = replace;
        return this;
    }

    public TagBuilder add(TagEntry entry) {
        entries.add(entry);
        return this;
    }

    public TagBuilder addElement(Identifier id) {
        return add(TagEntry.element(id));
    }

    public TagBuilder addOptionalElement(Identifier id) {
        return add(TagEntry.optionalElement(id));
    }

    public TagBuilder addTag(Identifier id) {
        return add(TagEntry.tag(id));
    }

    public TagBuilder addOptionalTag(Identifier id) {
        return add(TagEntry.optionalTag(id));
    }
}
