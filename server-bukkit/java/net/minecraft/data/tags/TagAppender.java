// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.data.tags;

import java.util.Arrays;
import java.util.Collection;
import java.util.stream.Stream;
import net.minecraft.resources.ResourceKey;
import net.minecraft.tags.TagBuilder;
import net.minecraft.tags.TagKey;

public interface TagAppender<T> {
    TagAppender<T> add(ResourceKey<T> element);

    @SuppressWarnings("unchecked")
    default TagAppender<T> add(ResourceKey<T>... elements) {
        Arrays.stream(elements).forEach(this::add);
        return this;
    }

    default TagAppender<T> addAll(Collection<ResourceKey<T>> elements) {
        elements.forEach(this::add);
        return this;
    }

    default TagAppender<T> addAll(Stream<ResourceKey<T>> elements) {
        elements.forEach(this::add);
        return this;
    }

    TagAppender<T> addOptional(ResourceKey<T> element);

    TagAppender<T> addTag(TagKey<T> tag);

    TagAppender<T> addOptionalTag(TagKey<T> tag);

    static <T> TagAppender<T> forBuilder(TagBuilder builder) {
        return new TagAppender<>() {
            @Override
            public TagAppender<T> add(ResourceKey<T> element) {
                builder.addElement(element.identifier());
                return this;
            }

            @Override
            public TagAppender<T> addOptional(ResourceKey<T> element) {
                builder.addOptionalElement(element.identifier());
                return this;
            }

            @Override
            public TagAppender<T> addTag(TagKey<T> tag) {
                builder.addTag(tag.location());
                return this;
            }

            @Override
            public TagAppender<T> addOptionalTag(TagKey<T> tag) {
                builder.addOptionalTag(tag.location());
                return this;
            }
        };
    }
}
