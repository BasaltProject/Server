// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.tags;

import com.mojang.datafixers.util.Either;
import com.mojang.serialization.Codec;
import com.mojang.serialization.DataResult;
import com.mojang.serialization.codecs.RecordCodecBuilder;
import java.util.Collection;
import java.util.function.Consumer;
import java.util.function.Predicate;
import net.minecraft.resources.Identifier;

public class TagEntry {
    private static final Codec<Reference> kReference = Codec.STRING.comapFlatMap(TagEntry::parseReference, Reference::text);
    private static final Codec<TagEntry> kFull = RecordCodecBuilder.<TagEntry>create(i -> i.group(
        kReference.fieldOf("id").forGetter(entry -> new Reference(entry.id, entry.tag)),
        Codec.BOOL.optionalFieldOf("required", true).forGetter(entry -> entry.required)).apply(i, TagEntry::new));
    public static final Codec<TagEntry> CODEC = Codec.either(kReference, kFull).xmap(
        either -> either.map(reference -> new TagEntry(reference, true), entry -> entry),
        entry -> entry.required ? Either.left(new Reference(entry.id, entry.tag)) : Either.right(entry));
    private final Identifier id;
    private final boolean tag;
    private final boolean required;

    private record Reference(Identifier id, boolean tag) {
        String text() {
            return (tag ? "#" : "") + id;
        }
    }

    private static DataResult<Reference> parseReference(String text) {
        boolean tag = text.startsWith("#");
        Identifier id = Identifier.tryParse(tag ? text.substring(1) : text);
        return id == null ? DataResult.error(() -> "Not a valid resource location: " + text) : DataResult.success(new Reference(id, tag));
    }

    private TagEntry(Identifier id, boolean tag, boolean required) {
        this.id = id;
        this.tag = tag;
        this.required = required;
    }

    private TagEntry(Reference reference, boolean required) {
        this(reference.id, reference.tag, required);
    }

    public static TagEntry element(Identifier id) {
        return new TagEntry(id, false, true);
    }

    public static TagEntry optionalElement(Identifier id) {
        return new TagEntry(id, false, false);
    }

    public static TagEntry tag(Identifier id) {
        return new TagEntry(id, true, true);
    }

    public static TagEntry optionalTag(Identifier id) {
        return new TagEntry(id, true, false);
    }

    public <T> boolean build(Lookup<T> lookup, Consumer<T> output) {
        if (tag) {
            Collection<T> members = lookup.tag(id);
            if (members == null) {
                return !required;
            }
            members.forEach(output);
            return true;
        }
        T element = lookup.element(id, required);
        if (element == null) {
            return !required;
        }
        output.accept(element);
        return true;
    }

    public void visitRequiredDependencies(Consumer<Identifier> output) {
        if (tag && required) {
            output.accept(id);
        }
    }

    public void visitOptionalDependencies(Consumer<Identifier> output) {
        if (tag && !required) {
            output.accept(id);
        }
    }

    public boolean verifyIfPresent(Predicate<Identifier> elementCheck, Predicate<Identifier> tagCheck) {
        return !required || (tag ? tagCheck : elementCheck).test(id);
    }

    public String toString() {
        return (tag ? "#" : "") + id + (required ? "" : "?");
    }

    public interface Lookup<T> {
        T element(Identifier key, boolean required);

        Collection<T> tag(Identifier key);
    }
}
