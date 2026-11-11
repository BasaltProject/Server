// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.data.tags;

import net.minecraft.references.BlockItemId;
import net.minecraft.resources.ResourceKey;
import net.minecraft.tags.TagKey;
import net.minecraft.world.level.block.ColorCollection;
import net.minecraft.world.level.block.WeatheringCopperCollection;

public abstract class BlockItemTagAppender<Element> implements TagAppender<Element> {
    private final TagAppender<Element> original;

    public BlockItemTagAppender(TagAppender<Element> original) {
        this.original = original;
    }

    protected abstract ResourceKey<Element> convertElement(BlockItemId element);

    @Override
    public BlockItemTagAppender<Element> add(ResourceKey<Element> element) {
        original.add(element);
        return this;
    }

    public BlockItemTagAppender<Element> add(BlockItemId... ids) {
        for (BlockItemId id : ids) {
            original.add(convertElement(id));
        }
        return this;
    }

    public BlockItemTagAppender<Element> addAll(ColorCollection<ResourceKey<Element>> collection) {
        collection.forEach(original::add);
        return this;
    }

    public BlockItemTagAppender<Element> addAll(WeatheringCopperCollection<ResourceKey<Element>> collection) {
        collection.asList().forEach(original::add);
        return this;
    }

    @Override
    @SuppressWarnings("unchecked")
    public final BlockItemTagAppender<Element> add(ResourceKey<Element>... elements) {
        for (ResourceKey<Element> element : elements) {
            original.add(element);
        }
        return this;
    }

    @Override
    public BlockItemTagAppender<Element> addOptional(ResourceKey<Element> element) {
        original.addOptional(element);
        return this;
    }

    @Override
    public BlockItemTagAppender<Element> addTag(TagKey<Element> tag) {
        original.addTag(tag);
        return this;
    }

    @Override
    public BlockItemTagAppender<Element> addOptionalTag(TagKey<Element> tag) {
        original.addOptionalTag(tag);
        return this;
    }
}
